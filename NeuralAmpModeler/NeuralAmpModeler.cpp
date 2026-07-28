#include <algorithm> // std::clamp, std::min
#include <cctype>
#include <chrono>
#include <cmath> // pow
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_set>
#include <utility>

#include "Colors.h"
#include "Loading/FileInspection.h"
#include "Library/PathSafety.h"
#include "../NeuralAmpModelerCore/NAM/activations.h"
#include "../NeuralAmpModelerCore/NAM/get_dsp.h"
#include "../NeuralAmpModelerCore/NAM/wavenet/a2_fast.h"
// clang-format off
// These includes need to happen in this order or else the latter won't know
// a bunch of stuff.
#include "NeuralAmpModeler.h"
#include "IPlug_include_in_plug_src.h"
// clang-format on
#include "architecture.hpp"

#include "NeuralAmpModelerControls.h"
#include "SharedSettings.h"
#include "UI/EditorLayout.h"

using namespace iplug;
using namespace igraphics;

const double kDCBlockerFrequency = 5.0;

namespace
{
IRECT ToIRECT(const amphibia::ui::Rect& rect)
{
  return {rect.left, rect.top, rect.right, rect.bottom};
}

bool IsLibraryTaskActive(amphibia::library::ImportTaskState state)
{
  using State = amphibia::library::ImportTaskState;
  return state != State::Idle && state != State::Completed && state != State::Failed && state != State::Cancelled;
}

const char* LibraryTaskName(amphibia::library::ImportTaskState state)
{
  using State = amphibia::library::ImportTaskState;
  switch (state)
  {
    case State::Idle: return "Idle";
    case State::Scanning: return "Scanning";
    case State::AwaitingSelection: return "Waiting for import review";
    case State::Extracting: return "Extracting";
    case State::Validating: return "Validating";
    case State::Hashing: return "Hashing";
    case State::Committing: return "Committing";
    case State::Completed: return "Completed";
    case State::Failed: return "Failed";
    case State::Cancelled: return "Cancelled";
  }
  return "Unknown";
}
} // namespace

int _GetStateString(const iplug::IByteChunk& chunk, int startPos, WDL_String& value);

// Styles
const IVColorSpec colorSpec{
  DEFAULT_BGCOLOR, // Background
  PluginColors::NAM_THEMECOLOR, // Foreground
  PluginColors::NAM_THEMECOLOR.WithOpacity(0.3f), // Pressed
  PluginColors::NAM_THEMECOLOR.WithOpacity(0.4f), // Frame
  PluginColors::MOUSEOVER, // Highlight
  DEFAULT_SHCOLOR, // Shadow
  PluginColors::NAM_THEMECOLOR, // Extra 1
  COLOR_RED, // Extra 2 --> color for clipping in meters
  PluginColors::NAM_THEMECOLOR.WithContrast(0.1f), // Extra 3
};

const IVStyle style =
  IVStyle{true, // Show label
          true, // Show value
          colorSpec,
          {DEFAULT_TEXT_SIZE + 3.f, EVAlign::Middle, PluginColors::NAM_THEMEFONTCOLOR}, // Knob label text5
          {DEFAULT_TEXT_SIZE + 3.f, EVAlign::Bottom, PluginColors::NAM_THEMEFONTCOLOR}, // Knob value text
          DEFAULT_HIDE_CURSOR,
          DEFAULT_DRAW_FRAME,
          false,
          DEFAULT_EMBOSS,
          0.2f,
          2.f,
          DEFAULT_SHADOW_OFFSET,
          DEFAULT_WIDGET_FRAC,
          DEFAULT_WIDGET_ANGLE};
const IVStyle titleStyle =
  DEFAULT_STYLE.WithValueText(IText(30, COLOR_WHITE, "Michroma-Regular")).WithDrawFrame(false).WithShadowOffset(2.f);
const IVStyle radioButtonStyle =
  style
    .WithColor(EVColor::kON, PluginColors::NAM_THEMECOLOR) // Pressed buttons and their labels
    .WithColor(EVColor::kOFF, PluginColors::NAM_THEMECOLOR.WithOpacity(0.1f)) // Unpressed buttons
    .WithColor(EVColor::kX1, PluginColors::NAM_THEMECOLOR.WithOpacity(0.6f)); // Unpressed buttons' labels

EMsgBoxResult _ShowMessageBox(iplug::igraphics::IGraphics* pGraphics, const char* str, const char* caption,
                              EMsgBoxType type)
{
#ifdef OS_MAC
  // macOS is backwards?
  return pGraphics->ShowMessageBox(caption, str, type);
#else
  return pGraphics->ShowMessageBox(str, caption, type);
#endif
}

const std::string kCalibrateInputParamName = "CalibrateInput";
const bool kDefaultCalibrateInput = false;
const std::string kInputCalibrationLevelParamName = "InputCalibrationLevel";
const double kDefaultInputCalibrationLevel = 12.0;


Amphibia::Amphibia(const InstanceInfo& info)
: Plugin(info, MakeConfig(kNumParams, kNumPresets))
, mModelLoader([this](const amphibia::loading::LoadRequest& request,
                      const decltype(mModelLoader)::CancelCheck& cancelled,
                      const decltype(mModelLoader)::ProgressCallback& progress) {
    return _PrepareModel(request, cancelled, progress);
  })
, mIRLoader([this](const amphibia::loading::LoadRequest& request,
                   const decltype(mIRLoader)::CancelCheck& cancelled,
                   const decltype(mIRLoader)::ProgressCallback& progress) {
    return _PrepareIR(request, cancelled, progress);
  })
{
  try
  {
    mLibrary = std::make_unique<amphibia::library::ManagedLibrary>(amphibia::library::DefaultLibraryPath());
    mLibrary->Initialize();
    mImportWorker = std::make_unique<amphibia::library::ImportWorker>(*mLibrary);
  }
  catch (const std::exception& exception)
  {
    std::cerr << "Managed library unavailable: " << exception.what() << '\n';
  }
  if (mLibrary && mTone3000Registration.providerUsable)
  {
    mTone3000Provider = std::make_unique<amphibia::providers::tone3000::Tone3000Provider>(
      mTone3000Configuration, mTone3000Browser, mTone3000Credentials, mTone3000Transport, *mLibrary);
    mTone3000Selection =
      std::make_unique<amphibia::providers::tone3000::Tone3000HostedSelectionCoordinator>(
        mTone3000Configuration, *mTone3000Provider, mTone3000CallbackReceiver,
        [this](const amphibia::providers::RemoteModel& model) -> std::optional<std::string> {
          if (!mLibrary) return std::nullopt;
          const auto association =
            mLibrary->FindProviderAssociation("tone3000", model.providerToneId, model.providerModelId);
          return association ? std::optional<std::string>(amphibia::library::ToHex(association->objectHash)) :
                               std::nullopt;
        },
        [this](std::uint64_t taskId, const std::string& managedSha256,
               amphibia::providers::HostedSelectionKind kind) {
          if (!mTone3000Selection) return;
          const auto snapshot = mTone3000Selection->Snapshot();
          if (!snapshot.selectedVariant || *snapshot.selectedVariant >= snapshot.variants.size()) return;
          const auto modelId = snapshot.variants[*snapshot.selectedVariant].model.providerModelId;
          try
          {
            const auto hash = amphibia::library::ContentHashFromHex(managedSha256);
            mPendingProviderActivation = PendingProviderActivation{taskId, modelId, kind, hash};
            _RequestManaged(hash, kind == amphibia::providers::HostedSelectionKind::ImpulseResponse ?
              amphibia::library::ManagedObjectType::WaveImpulseResponse :
              amphibia::library::ManagedObjectType::NamModel);
          }
          catch (const std::exception&)
          {
            mTone3000Selection->NotifyActivation(taskId, modelId, false,
              "The downloaded variant could not be resolved in the managed library.");
            mPendingProviderActivation.reset();
          }
        },
        [this](const amphibia::providers::RemoteModel& model, const std::string&) {
          if (!mLibrary)
            return amphibia::providers::ProviderError{
              amphibia::providers::ProviderErrorCode::StorageFailed, "The managed library is unavailable."};
          std::unordered_set<std::string> protectedHashes;
          {
            std::lock_guard<std::mutex> lock(mPathMutex);
            if (mActiveManagedModel) protectedHashes.insert(amphibia::library::ToHex(*mActiveManagedModel));
            if (mActiveManagedIR) protectedHashes.insert(amphibia::library::ToHex(*mActiveManagedIR));
          }
          const auto removed = mLibrary->RemoveProviderCopy(
            "tone3000", model.providerToneId, model.providerModelId, protectedHashes);
          if (removed == amphibia::library::ProviderCopyRemoval::NotFound)
            return amphibia::providers::ProviderError{
              amphibia::providers::ProviderErrorCode::NotFound, "The local provider copy was not found."};
          if (removed == amphibia::library::ProviderCopyRemoval::ActiveProtected)
            return amphibia::providers::ProviderError{
              amphibia::providers::ProviderErrorCode::Forbidden, "The active provider copy cannot be removed."};
          return amphibia::providers::ProviderError{};
        });
  }
  _InitToneStack();
  nam::activations::Activation::enable_fast_tanh();
  GetParam(kInputLevel)->InitGain("Input", 0.0, -20.0, 20.0, 0.1);
  GetParam(kToneBass)->InitDouble("Bass", 5.0, 0.0, 10.0, 0.1);
  GetParam(kToneMid)->InitDouble("Middle", 5.0, 0.0, 10.0, 0.1);
  GetParam(kToneTreble)->InitDouble("Treble", 5.0, 0.0, 10.0, 0.1);
  GetParam(kOutputLevel)->InitGain("Output", 0.0, -40.0, 40.0, 0.1);
  GetParam(kNoiseGateThreshold)->InitGain("Threshold", -80.0, -100.0, 0.0, 0.1);
  GetParam(kNoiseGateActive)->InitBool("NoiseGateActive", true);
  GetParam(kEQActive)->InitBool("ToneStack", true);
  GetParam(kOutputMode)->InitEnum("OutputMode", 1, {"Raw", "Normalized", "Calibrated"}); // TODO DRY w/ control
  GetParam(kIRToggle)->InitBool("IRToggle", true);
  GetParam(kCalibrateInput)->InitBool(kCalibrateInputParamName.c_str(), kDefaultCalibrateInput);
  GetParam(kInputCalibrationLevel)
    ->InitDouble(kInputCalibrationLevelParamName.c_str(), kDefaultInputCalibrationLevel, -60.0, 60.0, 0.1, "dBu");
  GetParam(kSlim)->InitDouble("Slim", 0.0, 0.0, 1.0, 0.01);

  mNoiseGateTrigger.AddListener(&mNoiseGateGain);

  mMakeGraphicsFunc = [&]() {

#ifdef OS_IOS
    auto scaleFactor = GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT) * 0.85f;
#else
    auto scaleFactor = 1.0f;
#endif

    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, scaleFactor);
  };

  mLayoutFunc = [&](IGraphics* pGraphics) {
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    pGraphics->AttachTextEntryControl();
    pGraphics->EnableMouseOver(true);
    pGraphics->EnableTooltips(true);
    pGraphics->EnableMultiTouch(true);

    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    pGraphics->LoadFont("Michroma-Regular", MICHROMA_FN);

    const auto gearSVG = pGraphics->LoadSVG(GEAR_FN);
    const auto fileSVG = pGraphics->LoadSVG(FILE_FN);
    const auto globeSVG = pGraphics->LoadSVG(GLOBE_ICON_FN);
    const auto crossSVG = pGraphics->LoadSVG(CLOSE_BUTTON_FN);
    const auto rightArrowSVG = pGraphics->LoadSVG(RIGHT_ARROW_FN);
    const auto leftArrowSVG = pGraphics->LoadSVG(LEFT_ARROW_FN);
    const auto modelIconSVG = pGraphics->LoadSVG(MODEL_ICON_FN);
    const auto irIconOnSVG = pGraphics->LoadSVG(IR_ICON_ON_FN);
    const auto irIconOffSVG = pGraphics->LoadSVG(IR_ICON_OFF_FN);
    const auto slimIconSVG = pGraphics->LoadSVG(SLIMMABLE_ICON_FN);

    const auto backgroundBitmap = pGraphics->LoadBitmap(BACKGROUND_FN);
    const auto fileBackgroundBitmap = pGraphics->LoadBitmap(FILEBACKGROUND_FN);
    const auto inputLevelBackgroundBitmap = pGraphics->LoadBitmap(INPUTLEVELBACKGROUND_FN);
    const auto linesBitmap = pGraphics->LoadBitmap(LINES_FN);
    const auto knobBackgroundBitmap = pGraphics->LoadBitmap(KNOBBACKGROUND_FN);
    const auto switchHandleBitmap = pGraphics->LoadBitmap(SLIDESWITCHHANDLE_FN);
    const auto meterBackgroundBitmap = pGraphics->LoadBitmap(METERBACKGROUND_FN);

    const auto b = pGraphics->GetBounds();
    const auto layout = amphibia::ui::CalculateMainEditorLayout(b.W(), b.H());
    const auto titleArea = ToIRECT(layout.title);

    const auto inputKnobArea = ToIRECT(layout.knobs[kInputLevel]);
    const auto noiseGateArea = ToIRECT(layout.knobs[kNoiseGateThreshold]);
    const auto bassKnobArea = ToIRECT(layout.knobs[kToneBass]);
    const auto midKnobArea = ToIRECT(layout.knobs[kToneMid]);
    const auto trebleKnobArea = ToIRECT(layout.knobs[kToneTreble]);
    const auto outputKnobArea = ToIRECT(layout.knobs[kOutputLevel]);

    const auto ngToggleArea =
      noiseGateArea.GetVShifted(noiseGateArea.H()).SubRectVertical(2, 0).GetReducedFromTop(10.0f);
    const auto eqToggleArea = midKnobArea.GetVShifted(midKnobArea.H()).SubRectVertical(2, 0).GetReducedFromTop(10.0f);

    const auto modelArea = ToIRECT(layout.model);
    const auto irArea = ToIRECT(layout.impulseResponse);
    const auto slimIconArea = ToIRECT(layout.slimIcon);
    const auto modelIconArea = ToIRECT(layout.modelIcon);
    const auto irSwitchArea = ToIRECT(layout.impulseResponseSwitch);
    const auto tone3000ModelArea = ToIRECT(layout.searchTone3000);
    const auto tone3000IrArea = ToIRECT(layout.findTone3000Ir);

    const auto inputMeterArea = ToIRECT(layout.inputMeter);
    const auto outputMeterArea = ToIRECT(layout.outputMeter);

    const auto settingsButtonArea = ToIRECT(layout.settings);

    RemoteProviderUIActions providerActions;
    providerActions.information = [this] {
      const auto registration =
        mTone3000Registration.state ==
          amphibia::providers::tone3000::Tone3000RegistrationState::ProductionConfigured ?
        "Production" :
        mTone3000Registration.state ==
          amphibia::providers::tone3000::Tone3000RegistrationState::DevelopmentConfigured ?
        "Development" : "Not configured";
      std::string connection = "Disconnected";
      amphibia::credentials::SecureCredential credential;
      const amphibia::credentials::CredentialKey key{
        mTone3000Configuration.credentialServiceNamespace, mTone3000Configuration.credentialAccount};
      if (mTone3000Credentials.Load(key, credential) && credential.username && !credential.username->empty())
        connection = "Connected as @" + *credential.username;
      return std::string("Application registration: ") + registration +
        "\nCallback: Fixed local callback\nStatus: " + connection;
    };
    providerActions.connect = [this](IControl* caller) {
      if (!mTone3000Provider) return;
      const auto start = [this] {
        return mTone3000CallbackReceiver.Start(
          mTone3000Configuration.redirectUri, std::chrono::steady_clock::now() + std::chrono::minutes(5),
          [this](amphibia::providers::ProviderResult<std::string> callback) {
            if (!mTone3000Provider) return;
            if (callback) mTone3000Provider->CompleteAuthorizationCallback(*callback.value);
            else mTone3000Provider->CancelAuthorization();
          });
      };
      auto error = start();
      while (error.code == amphibia::providers::ProviderErrorCode::LocalPortInUse &&
             _ShowMessageBox(caller->GetUI(), error.userMessage.c_str(), "TONE3000", kMB_RETRYCANCEL) == kRETRY)
        error = start();
      if (error)
      {
        if (error.code != amphibia::providers::ProviderErrorCode::LocalPortInUse)
          _ShowMessageBox(caller->GetUI(), error.userMessage.c_str(), "TONE3000", kMB_OK);
        return;
      }
      amphibia::providers::AuthorizationRequest request;
      request.purpose = amphibia::providers::AuthorizationPurpose::ConnectAccount;
      mTone3000Provider->BeginAuthorization(request, [this](auto) {
        mTone3000CallbackReceiver.Cancel();
      });
    };
    providerActions.disconnect = [this](IControl*) {
      if (mTone3000Provider) mTone3000Provider->Disconnect();
    };
    providerActions.testConnection = [this](IControl*) {
      if (mTone3000Provider) mTone3000Provider->FetchCurrentUser([](auto) {});
    };
    providerActions.cancel = [this](IControl*) {
      if (mTone3000Selection) mTone3000Selection->Cancel();
      else if (mTone3000Provider) mTone3000Provider->CancelCurrentOperation();
      mTone3000CallbackReceiver.Cancel();
    };
    providerActions.open = [this](IControl* caller) {
      if (const auto error = mTone3000Browser.OpenExplicit("https://www.tone3000.com"))
        _ShowMessageBox(caller->GetUI(), error.userMessage.c_str(), "TONE3000", kMB_OK);
    };
    providerActions.connectEnabled = mTone3000Provider != nullptr;
    providerActions.disconnectEnabled = mTone3000Provider != nullptr;
    providerActions.testEnabled = mTone3000Provider != nullptr;
    providerActions.cancelEnabled = mTone3000Provider != nullptr;
    providerActions.openEnabled = true;

    auto startHostedSelection = [this](amphibia::providers::HostedSelectionKind kind) {
      if (!mTone3000Selection) return;
      auto error = mTone3000Selection->Begin(kind);
      while (error.code == amphibia::providers::ProviderErrorCode::LocalPortInUse && GetUI() &&
             _ShowMessageBox(GetUI(), error.userMessage.c_str(), "TONE3000", kMB_RETRYCANCEL) == kRETRY)
        error = mTone3000Selection->Begin(kind);
      if (auto* graphics = GetUI())
        if (auto* panel = graphics->GetControlWithTag(kCtrlTagTone3000Panel))
          panel->As<Tone3000SelectionPanelControl>()->Update(mTone3000Selection->Snapshot());
    };

    LibraryUIActions libraryActions;
    libraryActions.information = [this] { return ManagedLibraryConfiguration().information; };
    libraryActions.openFolder = [this](IControl* caller) {
      if (!mLibrary) return;
      WDL_String path(amphibia::library::PathToUtf8(mLibrary->Root()).c_str());
      caller->GetUI()->RevealPathInExplorerOrFinder(path, false);
    };
    libraryActions.verify = [this](IControl*) { VerifyManagedLibrary(); };
    libraryActions.clearStaging = [this](IControl*) { ClearManagedLibraryStaging(); };
    libraryActions.removeUnused = [this](IControl*) {
      if (!mImportWorker) return;
      std::vector<std::string> protectedHashes;
      {
        std::lock_guard<std::mutex> lock(mPathMutex);
        if (mActiveManagedModel) protectedHashes.push_back(amphibia::library::ToHex(*mActiveManagedModel));
        if (mActiveManagedIR) protectedHashes.push_back(amphibia::library::ToHex(*mActiveManagedIR));
      }
      mImportTarget = ImportTarget::None;
      mCompletedImportTask = 0;
      mImportWorker->RemoveUnused(std::move(protectedHashes));
    };
    const auto startManagedImport = [this](std::vector<std::filesystem::path> sources) {
      ScanManagedLibrarySources(std::move(sources));
    };
    libraryActions.importFiles = [startManagedImport](IControl* caller) {
      WDL_String file, directory;
      caller->GetUI()->PromptForFile(file, directory, EFileAction::Open, "nam wav",
        [startManagedImport](const WDL_String& selected, const WDL_String&) {
          if (selected.GetLength()) startManagedImport({std::filesystem::u8path(selected.Get())});
        });
    };
    libraryActions.importFolder = [startManagedImport](IControl* caller) {
      WDL_String directory;
      caller->GetUI()->PromptForDirectory(directory,
        [startManagedImport](const WDL_String&, const WDL_String& selected) {
          if (selected.GetLength()) startManagedImport({std::filesystem::u8path(selected.Get())});
        });
    };
    libraryActions.importZip = [startManagedImport](IControl* caller) {
      WDL_String file, directory;
      caller->GetUI()->PromptForFile(file, directory, EFileAction::Open, "zip",
        [startManagedImport](const WDL_String& selected, const WDL_String&) {
          if (selected.GetLength()) startManagedImport({std::filesystem::u8path(selected.Get())});
        });
    };

    LocalFolderUIActions folderActions;
    folderActions.information = [] {
      const auto loaded = amphibia::preferences::LoadSharedSettings();
      const auto nam = loaded.settings.namFolder ? *loaded.settings.namFolder : "Not configured";
      const auto ir = loaded.settings.irFolder ? *loaded.settings.irFolder : "Not configured";
      return std::string("Referenced local folders (shared by standalone and plug-in) | NAM: ") + nam + "\nIR: " + ir;
    };
    const auto chooseSharedFolder = [](bool nam, IControl* caller) {
      WDL_String directory;
      caller->GetUI()->PromptForDirectory(directory, [nam, caller](const WDL_String&, const WDL_String& selected) {
        if (!selected.GetLength()) return;
        std::error_code code;
        if (!std::filesystem::is_directory(std::filesystem::u8path(selected.Get()), code) || code)
        {
          _ShowMessageBox(caller->GetUI(), "The selected folder is unavailable.", "Amphibia Configuration", kMB_OK);
          return;
        }
        auto loaded = amphibia::preferences::LoadSharedSettings();
        if (!loaded.available)
        {
          _ShowMessageBox(caller->GetUI(), loaded.message.c_str(), "Amphibia Configuration", kMB_OK);
          return;
        }
        if (nam) loaded.settings.namFolder = selected.Get(); else loaded.settings.irFolder = selected.Get();
        std::string error;
        if (!amphibia::preferences::UpdateSharedSettings(loaded.settings, error))
        {
          _ShowMessageBox(caller->GetUI(), error.c_str(), "Amphibia Configuration", kMB_OK);
          return;
        }
        const auto tag = nam ? kCtrlTagModelFileBrowser : kCtrlTagIRFileBrowser;
        if (auto* browser = caller->GetUI()->GetControlWithTag(tag))
          browser->As<NAMFileBrowserControl>()->SetPreferredDirectory(selected.Get());
        if (auto* settings = caller->GetUI()->GetControlWithTag(kCtrlTagSettingsBox))
          settings->As<NAMSettingsPageControl>()->ShowSection(amphibia::ui::SettingsSection::LocalFiles);
      });
    };
    folderActions.chooseNamFolder = [chooseSharedFolder](IControl* caller) { chooseSharedFolder(true, caller); };
    folderActions.chooseIrFolder = [chooseSharedFolder](IControl* caller) { chooseSharedFolder(false, caller); };
    // Model loader button
    auto loadModelCompletionHandler = [&](const WDL_String& fileName, const WDL_String& path) {
      if (fileName.GetLength())
      {
        _RequestModel(fileName);
      }
    };

    // IR loader button
    auto loadIRCompletionHandler = [&](const WDL_String& fileName, const WDL_String& path) {
      if (fileName.GetLength())
      {
        _RequestIR(fileName);
      }
    };

    pGraphics->AttachBackground(BACKGROUND_FN);
    pGraphics->AttachControl(new IBitmapControl(b, linesBitmap));
    pGraphics->AttachControl(new IVLabelControl(titleArea, "AMPHIBIA", titleStyle));
    pGraphics->AttachControl(new ISVGControl(modelIconArea, modelIconSVG));

#ifdef NAM_PICK_DIRECTORY
    const std::string defaultNamFileString = "Select model directory...";
    const std::string defaultIRString = "Select IR directory...";
#else
    const std::string defaultNamFileString = "Select model...";
    const std::string defaultIRString = "Select IR...";
#endif
    // Getting started page listing additional resources
    const char* const getUrl = "https://www.neuralampmodeler.com/users#comp-marb84o5";
    pGraphics->AttachControl(
      new NAMFileBrowserControl(modelArea, kMsgTagClearModel, defaultNamFileString.c_str(), "nam",
                                loadModelCompletionHandler, style, fileSVG, crossSVG, leftArrowSVG, rightArrowSVG,
                                fileBackgroundBitmap, globeSVG, "Get NAM Models", getUrl,
                                [this](const std::vector<std::string>& paths) { _HandleDroppedPaths(paths, true); }),
      kCtrlTagModelFileBrowser);

    auto hideSlimOverlay = [](IControl* pCaller) {
      IGraphics* ui = pCaller->GetUI();
      if (auto* backdrop = ui->GetControlWithTag(kCtrlTagSlimOverlayBackdrop))
        backdrop->Hide(true);
      if (auto* knob = ui->GetControlWithTag(kCtrlTagSlimKnob))
        knob->Hide(true);
      ui->SetAllControlsDirty();
    };
    auto showSlimOverlay = [](IControl* pCaller) {
      IGraphics* ui = pCaller->GetUI();
      if (auto* backdrop = ui->GetControlWithTag(kCtrlTagSlimOverlayBackdrop))
        backdrop->Hide(false);
      if (auto* knob = ui->GetControlWithTag(kCtrlTagSlimKnob))
        knob->Hide(false);
      ui->SetAllControlsDirty();
    };

    pGraphics
      ->AttachControl(
        new NAMSquareButtonControl(slimIconArea, DefaultClickActionFunc, slimIconSVG), kCtrlTagSlimmableIcon)
      ->SetAnimationEndActionFunction(showSlimOverlay)
      ->Hide(true);

    pGraphics->AttachControl(new ISVGSwitchControl(irSwitchArea, {irIconOffSVG, irIconOnSVG}, kIRToggle));
    pGraphics->AttachControl(
      new NAMFileBrowserControl(irArea, kMsgTagClearIR, defaultIRString.c_str(), "wav", loadIRCompletionHandler, style,
                                fileSVG, crossSVG, leftArrowSVG, rightArrowSVG, fileBackgroundBitmap, globeSVG,
                                "Get IRs", getUrl,
                                [this](const std::vector<std::string>& paths) { _HandleDroppedPaths(paths, false); }),
      kCtrlTagIRFileBrowser);
    const auto providerButtonStyle = style.WithShowLabel(true).WithDrawFrame(true)
      .WithLabelText(IText(9.f, PluginColors::NAM_THEMEFONTCOLOR, "Roboto-Regular"));
    auto* searchTone3000 = pGraphics->AttachControl(new IVButtonControl(
      tone3000ModelArea, [](IControl* caller) {
        if (auto* panel = caller->GetUI()->GetControlWithTag(kCtrlTagTone3000Panel))
          panel->As<Tone3000SelectionPanelControl>()->ShowArchitectureChoice();
      }, amphibia::ui::kSearchTone3000Caption.data(), providerButtonStyle), kCtrlTagTone3000Search);
    searchTone3000->SetDisabled(!mTone3000Registration.providerUsable || !mTone3000Selection);
    searchTone3000->SetTooltip(mTone3000Registration.providerUsable ?
      "Open TONE3000 hosted selection" : "TONE3000 integration is not configured in this build.");
    auto* findTone3000Ir = pGraphics->AttachControl(new IVButtonControl(
      tone3000IrArea, [startHostedSelection](IControl*) {
        startHostedSelection(amphibia::providers::HostedSelectionKind::ImpulseResponse);
      }, amphibia::ui::kFindTone3000IrCaption.data(), providerButtonStyle), kCtrlTagTone3000FindIR);
    findTone3000Ir->SetDisabled(!mTone3000Registration.providerUsable || !mTone3000Selection);
    findTone3000Ir->SetTooltip(mTone3000Registration.providerUsable ?
      "Open TONE3000 hosted cabinet/IR selection" : "TONE3000 integration is not configured in this build.");
    pGraphics->AttachControl(
      new NAMSwitchControl(ngToggleArea, kNoiseGateActive, "Noise Gate", style, switchHandleBitmap));
    pGraphics->AttachControl(new NAMSwitchControl(eqToggleArea, kEQActive, "EQ", style, switchHandleBitmap));

    // The knobs
    pGraphics->AttachControl(new NAMKnobControl(inputKnobArea, kInputLevel, "", style, knobBackgroundBitmap));
    pGraphics->AttachControl(new NAMKnobControl(noiseGateArea, kNoiseGateThreshold, "", style, knobBackgroundBitmap));
    pGraphics->AttachControl(
      new NAMKnobControl(bassKnobArea, kToneBass, "", style, knobBackgroundBitmap), -1, "EQ_KNOBS");
    pGraphics->AttachControl(
      new NAMKnobControl(midKnobArea, kToneMid, "", style, knobBackgroundBitmap), -1, "EQ_KNOBS");
    pGraphics->AttachControl(
      new NAMKnobControl(trebleKnobArea, kToneTreble, "", style, knobBackgroundBitmap), -1, "EQ_KNOBS");
    pGraphics->AttachControl(new NAMKnobControl(outputKnobArea, kOutputLevel, "", style, knobBackgroundBitmap));

    // The meters
    pGraphics->AttachControl(new NAMMeterControl(inputMeterArea, meterBackgroundBitmap, style), kCtrlTagInputMeter);
    pGraphics->AttachControl(new NAMMeterControl(outputMeterArea, meterBackgroundBitmap, style), kCtrlTagOutputMeter);

    // Settings/help/about box
    pGraphics->AttachControl(new NAMCircleButtonControl(
      settingsButtonArea,
      [pGraphics](IControl* pCaller) {
        auto* settings = pGraphics->GetControlWithTag(kCtrlTagSettingsBox)->As<NAMSettingsPageControl>();
        settings->ShowSection(amphibia::ui::SettingsSection::General);
      },
      gearSVG));

    pGraphics
      ->AttachControl(new NAMSettingsPageControl(b, backgroundBitmap, inputLevelBackgroundBitmap, switchHandleBitmap,
                                                 crossSVG, style, radioButtonStyle, std::move(folderActions),
                                                 std::move(libraryActions),
                                                 std::move(providerActions)),
                      kCtrlTagSettingsBox)
      ->Hide(true);

    Tone3000PanelActions tone3000PanelActions;
    tone3000PanelActions.begin = startHostedSelection;
    tone3000PanelActions.reopenBrowser = [this] {
      if (mTone3000Selection) (void)mTone3000Selection->ReopenBrowser();
    };
    tone3000PanelActions.cancel = [this] {
      if (mTone3000Selection) mTone3000Selection->Cancel();
    };
    tone3000PanelActions.selectVariant = [this](std::size_t index) {
      if (mTone3000Selection) (void)mTone3000Selection->SelectVariant(index);
    };
    tone3000PanelActions.download = [this](bool audition) {
      if (mTone3000Selection) (void)mTone3000Selection->DownloadSelected(audition);
    };
    tone3000PanelActions.adjacentDownloaded = [this](int direction) {
      if (mTone3000Selection) (void)mTone3000Selection->SelectAdjacentDownloaded(direction);
    };
    tone3000PanelActions.removeLocalCopy = [this] {
      if (mTone3000Selection) (void)mTone3000Selection->RemoveSelectedLocalCopy();
    };
    tone3000PanelActions.openSource = [this](const std::string& url) {
      (void)mTone3000Browser.OpenExplicit(url);
    };
    pGraphics->AttachControl(new Tone3000SelectionPanelControl(b, std::move(tone3000PanelActions)),
                             kCtrlTagTone3000Panel)->Hide(true);

    const auto slimKnobArea = b.GetCentredInside(100.f, NAM_KNOB_HEIGHT + 24.f);
    pGraphics->AttachControl(new NAMSlimOverlayBackdropControl(b, hideSlimOverlay), kCtrlTagSlimOverlayBackdrop)
      ->Hide(true);
    pGraphics
      ->AttachControl(new NAMKnobControl(slimKnobArea, kSlim, "Slim", style, knobBackgroundBitmap), kCtrlTagSlimKnob)
      ->Hide(true);

    pGraphics->ForAllControlsFunc([](IControl* pControl) {
      pControl->SetMouseEventsWhenDisabled(true);
      pControl->SetMouseOverWhenDisabled(true);
    });

    // pGraphics->GetControlWithTag(kCtrlTagOutNorm)->SetMouseEventsWhenDisabled(false);
    // pGraphics->GetControlWithTag(kCtrlTagCalibrateInput)->SetMouseEventsWhenDisabled(false);
  };
}

Amphibia::~Amphibia()
{
  if (mTone3000Selection) mTone3000Selection->Shutdown();
  if (mTone3000Provider) mTone3000Provider->Shutdown();
  mTone3000CallbackReceiver.Shutdown();
  mTone3000Transport.Shutdown();
  // Factories capture this plug-in, so join them while the complete owner is
  // still alive. Shutdown is idempotent for member teardown.
  if (mImportWorker) mImportWorker->Shutdown();
  mModelLoader.Shutdown();
  mIRLoader.Shutdown();
  _DeallocateIOPointers();
}

void Amphibia::ProcessBlock(iplug::sample** inputs, iplug::sample** outputs, int nFrames)
{
  const size_t numChannelsExternalIn = (size_t)NInChansConnected();
  const size_t numChannelsExternalOut = (size_t)NOutChansConnected();
  const size_t numChannelsInternal = kNumChannelsInternal;
  const size_t numFrames = (size_t)nFrames;
  const double sampleRate = GetSampleRate();

  // Disable floating point denormals
  std::fenv_t fe_state;
  std::feholdexcept(&fe_state);
  disable_denormals();

  _PrepareBuffers(numChannelsInternal, numFrames);
  // Input is collapsed to mono in preparation for the NAM.
  _ProcessInput(inputs, numFrames, numChannelsExternalIn, numChannelsInternal);
  _ApplyDSPStaging();
  const bool noiseGateActive = GetParam(kNoiseGateActive)->Value();
  const bool toneStackActive = GetParam(kEQActive)->Value();

  // Noise gate trigger
  sample** triggerOutput = mInputPointers;
  if (noiseGateActive)
  {
    const double time = 0.01;
    const double threshold = GetParam(kNoiseGateThreshold)->Value(); // GetParam...
    const double ratio = 0.1; // Quadratic...
    const double openTime = 0.005;
    const double holdTime = 0.01;
    const double closeTime = 0.05;
    const dsp::noise_gate::TriggerParams triggerParams(time, threshold, ratio, openTime, holdTime, closeTime);
    mNoiseGateTrigger.SetParams(triggerParams);
    mNoiseGateTrigger.SetSampleRate(sampleRate);
    triggerOutput = mNoiseGateTrigger.Process(mInputPointers, numChannelsInternal, numFrames);
  }

  if (auto* model = mModelLoader.ActiveForAudio())
  {
    model->process(triggerOutput, mOutputPointers, nFrames);
  }
  else
  {
    _FallbackDSP(triggerOutput, mOutputPointers, numChannelsInternal, numFrames);
  }
  // Apply the noise gate after the NAM
  sample** gateGainOutput =
    noiseGateActive ? mNoiseGateGain.Process(mOutputPointers, numChannelsInternal, numFrames) : mOutputPointers;

  sample** toneStackOutPointers = (toneStackActive && mToneStack != nullptr)
                                    ? mToneStack->Process(gateGainOutput, numChannelsInternal, nFrames)
                                    : gateGainOutput;

  sample** irPointers = toneStackOutPointers;
  if (auto* ir = mIRLoader.ActiveForAudio(); ir != nullptr && GetParam(kIRToggle)->Value())
    irPointers = ir->Process(toneStackOutPointers, numChannelsInternal, numFrames);

  // And the HPF for DC offset (Issue 271)
  const double highPassCutoffFreq = kDCBlockerFrequency;
  // const double lowPassCutoffFreq = 20000.0;
  const recursive_linear_filter::HighPassParams highPassParams(sampleRate, highPassCutoffFreq);
  // const recursive_linear_filter::LowPassParams lowPassParams(sampleRate, lowPassCutoffFreq);
  mHighPass.SetParams(highPassParams);
  // mLowPass.SetParams(lowPassParams);
  sample** hpfPointers = mHighPass.Process(irPointers, numChannelsInternal, numFrames);
  // sample** lpfPointers = mLowPass.Process(hpfPointers, numChannelsInternal, numFrames);

  // restore previous floating point state
  std::feupdateenv(&fe_state);

  // Let's get outta here
  // This is where we exit mono for whatever the output requires.
  _ProcessOutput(hpfPointers, outputs, numFrames, numChannelsInternal, numChannelsExternalOut);
  // _ProcessOutput(lpfPointers, outputs, numFrames, numChannelsInternal, numChannelsExternalOut);
  // * Output of input leveling (inputs -> mInputPointers),
  // * Output of output leveling (mOutputPointers -> outputs)
  _UpdateMeters(mInputPointers, outputs, numFrames, numChannelsInternal, numChannelsExternalOut);
}

void Amphibia::OnReset()
{
  const auto sampleRate = GetSampleRate();
  const int maxBlockSize = GetBlockSize();

  // Tail is because the HPF DC blocker has a decay.
  // 10 cycles should be enough to pass the VST3 tests checking tail behavior.
  // I'm ignoring the model & IR, but it's not the end of the world.
  const int tailCycles = 10;
  SetTailSize(tailCycles * (int)(sampleRate / kDCBlockerFrequency));
  mInputSender.Reset(sampleRate);
  mOutputSender.Reset(sampleRate);
  _PrepareBuffers(kNumChannelsInternal, static_cast<size_t>(std::max(1, maxBlockSize)));
  amphibia::loading::ProcessingConfiguration configuration;
  configuration.sampleRate = sampleRate;
  configuration.maximumBlockSize = std::max(1, maxBlockSize);
  configuration.inputChannels = NInChansConnected();
  configuration.outputChannels = NOutChansConnected();
  configuration.generation = mProcessingGeneration.fetch_add(1, std::memory_order_acq_rel) + 1;
  {
    std::lock_guard<std::mutex> lock(mProcessingConfigurationMutex);
    mProcessingConfiguration = configuration;
    mModelLoader.Reconfigure(configuration, GetParam(kSlim)->Value());
    mIRLoader.Reconfigure(configuration);
  }
  mToneStack->Reset(sampleRate, maxBlockSize);
  mPendingLatency.store(0, std::memory_order_release);
}

void Amphibia::OnIdle()
{
  mInputSender.TransmitData(*this);
  mOutputSender.TransmitData(*this);
  const auto slimRevision = mSlimRevision.load(std::memory_order_acquire);
  if (slimRevision != mSubmittedSlimRevision)
  {
    std::lock_guard<std::mutex> lock(mProcessingConfigurationMutex);
    mModelLoader.Reprepare(mProcessingConfiguration, GetParam(kSlim)->Value());
    mSubmittedSlimRevision = slimRevision;
  }
  _PollLoadingStatus();
  _PollImportStatus();
  if (mTone3000Selection)
  {
    const auto snapshot = mTone3000Selection->Snapshot();
    const auto selected = snapshot.selectedVariant ? std::to_string(*snapshot.selectedVariant) : "-";
    const auto signature = std::to_string(snapshot.taskId) + ":" +
      std::to_string(static_cast<int>(snapshot.stage)) + ":" +
      std::to_string(snapshot.variants.size()) + ":" + selected + ":" + snapshot.statusText;
    if (snapshot.stage != amphibia::providers::HostedSelectionStage::Idle &&
        signature != mLastTone3000SnapshotSignature)
    {
      mLastTone3000SnapshotSignature = signature;
      if (auto* graphics = GetUI())
        if (auto* panel = graphics->GetControlWithTag(kCtrlTagTone3000Panel))
          panel->As<Tone3000SelectionPanelControl>()->Update(snapshot);
    }
  }
  const int latency = mPendingLatency.exchange(-1, std::memory_order_acq_rel);
  if (latency >= 0 && GetLatency() != latency)
    SetLatency(latency);

  if (mNewModelLoadedInDSP)
  {
    if (auto* pGraphics = GetUI())
    {
      _UpdateControlsFromModel();
      mNewModelLoadedInDSP = false;
    }
  }
  if (mModelCleared)
  {
    if (auto* pGraphics = GetUI())
    {
      // FIXME -- need to disable only the "normalized" model
      // pGraphics->GetControlWithTag(kCtrlTagOutputMode)->SetDisabled(false);
      static_cast<NAMSettingsPageControl*>(pGraphics->GetControlWithTag(kCtrlTagSettingsBox))->ClearModelInfo();
      if (auto* p = pGraphics->GetControlWithTag(kCtrlTagSlimmableIcon))
        p->Hide(true);
      if (auto* p = pGraphics->GetControlWithTag(kCtrlTagSlimOverlayBackdrop))
        p->Hide(true);
      if (auto* p = pGraphics->GetControlWithTag(kCtrlTagSlimKnob))
        p->Hide(true);
      pGraphics->SetAllControlsDirty();
      mModelCleared = false;
    }
  }
}

bool Amphibia::SerializeState(IByteChunk& chunk) const
{
  // Amphibia has an independent state identity while retaining the inherited
  // payload layout and a legacy NeuralAmpModeler reader below.
  WDL_String header("###Amphibia###");
  chunk.PutStr(header.Get());
  // State-layout version is independent from Amphibia's product SemVer.
  WDL_String version(AMPHIBIA_STATE_VERSION);
  chunk.PutStr(version.Get());
  nlohmann::json references = nlohmann::json::object();
  // Model directory (don't serialize the model itself; we'll just load it again
  // when we unserialize)
  {
    std::lock_guard<std::mutex> lock(mPathMutex);
    chunk.PutStr(mNAMPath.Get());
    chunk.PutStr(mIRPath.Get());
    references["schema"] = 1;
    references["model"] = mActiveManagedModel ?
      nlohmann::json{{"kind", "managed"}, {"sha256", amphibia::library::ToHex(*mActiveManagedModel)}} :
      nlohmann::json{{"kind", mNAMPath.GetLength() ? "referenced" : "clear"}};
    references["ir"] = mActiveManagedIR ?
      nlohmann::json{{"kind", "managed"}, {"sha256", amphibia::library::ToHex(*mActiveManagedIR)}} :
      nlohmann::json{{"kind", mIRPath.GetLength() ? "referenced" : "clear"}};
  }
  const bool serialized = SerializeParams(chunk);
  chunk.PutStr("###AmphibiaLocalReferences###");
  const auto encoded = references.dump();
  chunk.PutStr(encoded.c_str());
  return serialized;
}

int Amphibia::UnserializeState(const IByteChunk& chunk, int startPos)
{
  try
  {
    // Look for the expected header. If it's there, then we'll know what to do.
    WDL_String header;
    int pos = startPos;
    pos = _GetStateString(chunk, pos, header);

    const char* kAmphibiaHeader = "###Amphibia###";
    const char* kLegacyNamHeader = "###NeuralAmpModeler###";
    if (strcmp(header.Get(), kAmphibiaHeader) == 0 || strcmp(header.Get(), kLegacyNamHeader) == 0)
    {
      const bool amphibiaState = strcmp(header.Get(), kAmphibiaHeader) == 0;
      pos = _UnserializeStateWithKnownVersion(chunk, pos);
      if (amphibiaState && pos < chunk.Size())
      {
        const int compatibilityEnd = pos;
        try
        {
          WDL_String marker;
          pos = _GetStateString(chunk, pos, marker);
          if (strcmp(marker.Get(), "###AmphibiaLocalReferences###") == 0 && pos < chunk.Size())
          {
            WDL_String payload;
            pos = _GetStateString(chunk, pos, payload);
            if (payload.GetLength() > 65536) throw std::runtime_error("Managed state reference is too large");
            const auto references = nlohmann::json::parse(payload.Get());
            if (references.value("schema", 0) == 1)
            {
              const auto restore = [this, &references](const char* key, amphibia::library::ManagedObjectType type) {
                if (!references.contains(key)) return;
                const auto kind = references[key].value("kind", "");
                if (kind == "managed")
                {
                  const auto hash = amphibia::library::ContentHashFromHex(references[key].value("sha256", ""));
                  _RequestManaged(hash, type);
                }
                else if (kind == "clear")
                {
                  std::lock_guard<std::mutex> lock(mProcessingConfigurationMutex);
                  if (type == amphibia::library::ManagedObjectType::NamModel)
                  {
                    mModelLoader.SubmitClear(mProcessingConfiguration);
                    mPendingManagedModel.reset();
                  }
                  else
                  {
                    mIRLoader.SubmitClear(mProcessingConfiguration);
                    mPendingManagedIR.reset();
                  }
                }
              };
              restore("model", amphibia::library::ManagedObjectType::NamModel);
              restore("ir", amphibia::library::ManagedObjectType::WaveImpulseResponse);
            }
          }
        }
        catch (...) { return compatibilityEnd; }
      }
      return pos;
    }
    return _UnserializeStateWithUnknownVersion(chunk, startPos);
  }
  catch (...)
  {
    // Host-provided state is untrusted. Parameters and active DSP remain as
    // they were when a chunk cannot be decoded completely.
    return -1;
  }
}

ManagedLibraryConfigurationStatus Amphibia::ManagedLibraryConfiguration() const
{
  ManagedLibraryConfigurationStatus result;
  if (!mLibrary)
  {
    result.information = "Managed library unavailable; referenced local files remain available.";
    return result;
  }

  try
  {
    const auto snapshot = mLibrary->Snapshot();
    const auto statistics = mLibrary->Statistics();
    const auto progress = mImportWorker ? mImportWorker->Progress() : amphibia::library::ImportProgress{};
    const auto verification = mImportWorker ? mImportWorker->Verification() : std::nullopt;
    result.available = true;
    result.rootPath = amphibia::library::PathToUtf8(mLibrary->Root());
    result.taskActive = IsLibraryTaskActive(progress.state);
    result.verificationComplete = verification.has_value();
    result.healthy = verification && verification->issues.empty() && verification->orphanedFiles == 0;

    std::ostringstream stream;
    stream << "Path: " << result.rootPath << "\nSchema: " << snapshot.schemaVersion
           << " | Objects: " << snapshot.objects.size() << " (" << statistics.namObjects << " NAM, "
           << statistics.irObjects << " IR) | Packs: " << statistics.packs
           << " | Size: " << statistics.managedBytes << " bytes\n"
           << "Write access: initialized | Safe staging: available | Task: " << LibraryTaskName(progress.state);
    if (progress.message) stream << " - " << *progress.message;
    if (verification)
      stream << "\nVerification: " << verification->checkedObjects << " checked, "
             << verification->issues.size() << " issue(s), " << verification->orphanedFiles << " orphan(s)";
    else
      stream << "\nVerification: not run in this session";
    stream << "\nRelocation is not supported in Milestone 6. Cleanup is always explicit.";
    result.information = stream.str();
  }
  catch (const std::exception& exception)
  {
    result.information = std::string("Managed library error: ") + exception.what() +
                         ". Referenced local files remain available.";
  }
  return result;
}

void Amphibia::VerifyManagedLibrary()
{
  if (!mImportWorker) return;
  mImportTarget = ImportTarget::None;
  mCompletedImportTask = 0;
  mImportWorker->VerifyLibrary();
}

void Amphibia::ClearManagedLibraryStaging()
{
  if (!mImportWorker) return;
  mImportTarget = ImportTarget::None;
  mCompletedImportTask = 0;
  mImportWorker->ClearStaging();
}

void Amphibia::ScanManagedLibrarySources(std::vector<std::filesystem::path> sources)
{
  if (!mImportWorker || sources.empty()) return;
  mImportTarget = ImportTarget::None;
  mReviewedImportTask = 0;
  mCompletedImportTask = 0;
  mImportWorker->Scan(std::move(sources));
}

void Amphibia::CancelManagedLibraryTask()
{
  if (mImportWorker) mImportWorker->Cancel();
}

#ifdef APP_API
ManagedLibraryConfigurationStatus amphibia::configuration::ManagedLibrary(iplug::IPlugAPP& plug)
{
  return static_cast<Amphibia&>(plug).ManagedLibraryConfiguration();
}

void amphibia::configuration::VerifyManagedLibrary(iplug::IPlugAPP& plug)
{
  static_cast<Amphibia&>(plug).VerifyManagedLibrary();
}

void amphibia::configuration::ClearManagedLibraryStaging(iplug::IPlugAPP& plug)
{
  static_cast<Amphibia&>(plug).ClearManagedLibraryStaging();
}

void amphibia::configuration::ScanManagedLibrarySources(iplug::IPlugAPP& plug,
                                                        std::vector<std::filesystem::path> sources)
{
  static_cast<Amphibia&>(plug).ScanManagedLibrarySources(std::move(sources));
}

void amphibia::configuration::CancelManagedLibraryTask(iplug::IPlugAPP& plug)
{
  static_cast<Amphibia&>(plug).CancelManagedLibraryTask();
}
#endif

void Amphibia::OnUIOpen()
{
  Plugin::OnUIOpen();

  // Shared preferences are read only when the editor opens. Never touch the
  // settings file from ProcessBlock() or the block-boundary staging path.
  const auto shared = amphibia::preferences::LoadSharedSettings();
  std::lock_guard<std::mutex> pathLock(mPathMutex);
  if (mNAMPath.GetLength())
  {
    SendControlMsgFromDelegate(kCtrlTagModelFileBrowser, kMsgTagLoadedModel, mNAMPath.GetLength(), mNAMPath.Get());
    // If it's not loaded yet, then mark as failed.
    // If it's yet to be loaded, then the completion handler will set us straight once it runs.
    if (!mModelLoader.HasRetainedProcessor())
      SendControlMsgFromDelegate(kCtrlTagModelFileBrowser, kMsgTagLoadFailed);
  }

  if (mIRPath.GetLength())
  {
    SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadedIR, mIRPath.GetLength(), mIRPath.Get());
    if (!mIRLoader.HasRetainedProcessor())
      SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadFailed);
  }

  if (mModelLoader.HasRetainedProcessor())
  {
    _UpdateControlsFromModel();
  }

  if (shared.available)
  {
    if (!mNAMPath.GetLength() && shared.settings.namFolder)
      if (auto* browser = GetUI()->GetControlWithTag(kCtrlTagModelFileBrowser))
        browser->As<NAMFileBrowserControl>()->SetPreferredDirectory(shared.settings.namFolder->c_str());
    if (!mIRPath.GetLength() && shared.settings.irFolder)
      if (auto* browser = GetUI()->GetControlWithTag(kCtrlTagIRFileBrowser))
        browser->As<NAMFileBrowserControl>()->SetPreferredDirectory(shared.settings.irFolder->c_str());
  }
}

void Amphibia::OnParamChange(int paramIdx)
{
  switch (paramIdx)
  {
    // Changes to the input gain
    case kCalibrateInput:
    case kInputCalibrationLevel:
    case kInputLevel: _SetInputGain(); break;
    // Changes to the output gain
    case kOutputLevel:
    case kOutputMode: _SetOutputGain(); break;
    // Tone stack:
    case kToneBass: mToneStack->SetParam("bass", GetParam(paramIdx)->Value()); break;
    case kToneMid: mToneStack->SetParam("middle", GetParam(paramIdx)->Value()); break;
    case kToneTreble: mToneStack->SetParam("treble", GetParam(paramIdx)->Value()); break;
    case kSlim: _ApplySlimParamToLoadedNAMs(); break;
    default: break;
  }
}

void Amphibia::OnParamChangeUI(int paramIdx, EParamSource source)
{
  if (auto pGraphics = GetUI())
  {
    bool active = GetParam(paramIdx)->Bool();

    switch (paramIdx)
    {
      case kNoiseGateActive: pGraphics->GetControlWithParamIdx(kNoiseGateThreshold)->SetDisabled(!active); break;
      case kEQActive:
        pGraphics->ForControlInGroup("EQ_KNOBS", [active](IControl* pControl) { pControl->SetDisabled(!active); });
        break;
      case kIRToggle: pGraphics->GetControlWithTag(kCtrlTagIRFileBrowser)->SetDisabled(!active); break;
      default: break;
    }
  }
}

bool Amphibia::OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData)
{
  switch (msgTag)
  {
    case kMsgTagClearModel:
    {
      std::lock_guard<std::mutex> lock(mProcessingConfigurationMutex);
      mModelLoader.SubmitClear(mProcessingConfiguration);
      mPendingManagedModel.reset();
      return true;
    }
    case kMsgTagClearIR:
    {
      std::lock_guard<std::mutex> lock(mProcessingConfigurationMutex);
      mIRLoader.SubmitClear(mProcessingConfiguration);
      mPendingManagedIR.reset();
      return true;
    }
    case kMsgTagHighlightColor:
    {
      mHighLightColor.Set((const char*)pData);

      if (GetUI())
      {
        GetUI()->ForStandardControlsFunc([&](IControl* pControl) {
          if (auto* pVectorBase = pControl->As<IVectorBase>())
          {
            IColor color = IColor::FromColorCodeStr(mHighLightColor.Get());

            pVectorBase->SetColor(kX1, color);
            pVectorBase->SetColor(kPR, color.WithOpacity(0.3f));
            pVectorBase->SetColor(kFR, color.WithOpacity(0.4f));
            pVectorBase->SetColor(kX3, color.WithContrast(0.1f));
          }
          pControl->GetUI()->SetAllControlsDirty();
        });
      }

      return true;
    }
    default: return false;
  }
}

// Private methods ============================================================

void Amphibia::_AllocateIOPointers(const size_t nChans)
{
  if (mInputPointers != nullptr)
    throw std::runtime_error("Tried to re-allocate mInputPointers without freeing");
  mInputPointers = new sample*[nChans];
  if (mInputPointers == nullptr)
    throw std::runtime_error("Failed to allocate pointer to input buffer!\n");
  if (mOutputPointers != nullptr)
    throw std::runtime_error("Tried to re-allocate mOutputPointers without freeing");
  mOutputPointers = new sample*[nChans];
  if (mOutputPointers == nullptr)
    throw std::runtime_error("Failed to allocate pointer to output buffer!\n");
}

void Amphibia::_ApplyDSPStaging()
{
  const auto modelActivation = mModelLoader.ActivateAtBlockBoundary();
  if (modelActivation.activated)
  {
    const auto& metadata = modelActivation.metadata;
    mModelHasLoudness.store(!modelActivation.cleared && metadata.hasLoudness, std::memory_order_release);
    mModelHasInputLevel.store(!modelActivation.cleared && metadata.hasInputLevel, std::memory_order_release);
    mModelHasOutputLevel.store(!modelActivation.cleared && metadata.hasOutputLevel, std::memory_order_release);
    mModelLoudness.store(metadata.loudness, std::memory_order_release);
    mModelInputLevel.store(metadata.inputLevel, std::memory_order_release);
    mModelOutputLevel.store(metadata.outputLevel, std::memory_order_release);
    mModelSampleRate.store(metadata.sampleRate, std::memory_order_release);
    mModelSlimmable.store(!modelActivation.cleared && metadata.slimmable, std::memory_order_release);
    mPendingLatency.store(modelActivation.cleared ? 0 : metadata.latency, std::memory_order_release);
    mNewModelLoadedInDSP.store(!modelActivation.cleared, std::memory_order_release);
    mModelCleared.store(modelActivation.cleared, std::memory_order_release);
    _SetInputGain();
    _SetOutputGain();
  }

  (void)mIRLoader.ActivateAtBlockBoundary();
}

void Amphibia::_DeallocateIOPointers()
{
  if (mInputPointers != nullptr)
  {
    delete[] mInputPointers;
    mInputPointers = nullptr;
  }
  if (mInputPointers != nullptr)
    throw std::runtime_error("Failed to deallocate pointer to input buffer!\n");
  if (mOutputPointers != nullptr)
  {
    delete[] mOutputPointers;
    mOutputPointers = nullptr;
  }
  if (mOutputPointers != nullptr)
    throw std::runtime_error("Failed to deallocate pointer to output buffer!\n");
}

void Amphibia::_FallbackDSP(iplug::sample** inputs, iplug::sample** outputs, const size_t numChannels,
                                    const size_t numFrames)
{
  for (auto c = 0; c < numChannels; c++)
    for (auto s = 0; s < numFrames; s++)
      mOutputArray[c][s] = mInputArray[c][s];
}

void Amphibia::_SetInputGain()
{
  iplug::sample inputGainDB = GetParam(kInputLevel)->Value();
  // Input calibration
  if (mModelHasInputLevel.load(std::memory_order_acquire) && GetParam(kCalibrateInput)->Bool())
  {
    inputGainDB += GetParam(kInputCalibrationLevel)->Value() - mModelInputLevel.load(std::memory_order_acquire);
  }
  mInputGain = DBToAmp(inputGainDB);
}

void Amphibia::_SetOutputGain()
{
  double gainDB = GetParam(kOutputLevel)->Value();
  if (mModelLoader.HasRetainedProcessor())
  {
    const int outputMode = GetParam(kOutputMode)->Int();
    switch (outputMode)
    {
      case 1: // Normalized
        if (mModelHasLoudness.load(std::memory_order_acquire))
        {
          const double loudness = mModelLoudness.load(std::memory_order_acquire);
          const double targetLoudness = -18.0;
          gainDB += (targetLoudness - loudness);
        }
        break;
      case 2: // Calibrated
        if (mModelHasOutputLevel.load(std::memory_order_acquire))
        {
          const double inputLevel = GetParam(kInputCalibrationLevel)->Value();
          const double outputLevel = mModelOutputLevel.load(std::memory_order_acquire);
          gainDB += (outputLevel - inputLevel);
        }
        break;
      case 0: // Raw
      default: break;
    }
  }
  mOutputGain = DBToAmp(gainDB);
}

void Amphibia::_ApplySlimParamToLoadedNAMs()
{
  mSlimRevision.fetch_add(1, std::memory_order_release);
}

void Amphibia::_RequestModel(const WDL_String& modelPath)
{
  mPendingManagedModel.reset();
  std::lock_guard<std::mutex> lock(mProcessingConfigurationMutex);
  mModelLoader.Submit(modelPath.Get(), mProcessingConfiguration, GetParam(kSlim)->Value());
}

void Amphibia::_RequestIR(const WDL_String& irPath)
{
  mPendingManagedIR.reset();
  std::lock_guard<std::mutex> lock(mProcessingConfigurationMutex);
  mIRLoader.Submit(irPath.Get(), mProcessingConfiguration);
}

void Amphibia::_RequestManaged(const amphibia::library::ContentHash& hash,
                               amphibia::library::ManagedObjectType type)
{
  if (!mLibrary) return;
  const auto path = mLibrary->Resolve(hash, type);
  if (!path) return;
  WDL_String value(amphibia::library::PathToUtf8(*path).c_str());
  if (type == amphibia::library::ManagedObjectType::NamModel)
  {
    _RequestModel(value);
    mPendingManagedModel = hash;
  }
  else
  {
    _RequestIR(value);
    mPendingManagedIR = hash;
  }
}

void Amphibia::_HandleDroppedPaths(const std::vector<std::string>& paths, bool modelSlot)
{
  if (paths.empty()) return;
  if (paths.size() == 1)
  {
    const auto path = std::filesystem::u8path(paths.front());
    auto extension = amphibia::library::PathToUtf8(path.extension());
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
    });
    if (std::filesystem::is_regular_file(path) &&
        ((modelSlot && extension == ".nam") || (!modelSlot && extension == ".wav")))
    {
      WDL_String value(paths.front().c_str());
      if (modelSlot) _RequestModel(value); else _RequestIR(value);
      return;
    }
  }
  if (!mImportWorker)
  {
    const std::string message = "Managed library is unavailable";
    SendControlMsgFromDelegate(modelSlot ? kCtrlTagModelFileBrowser : kCtrlTagIRFileBrowser,
                               kMsgTagLoadFailed, static_cast<int>(message.size()), message.c_str());
    return;
  }
  std::vector<std::filesystem::path> sources;
  sources.reserve(paths.size());
  for (const auto& path : paths) sources.push_back(std::filesystem::u8path(path));
  mImportTarget = modelSlot ? ImportTarget::Model : ImportTarget::IR;
  mReviewedImportTask = 0;
  mCompletedImportTask = 0;
  mImportWorker->Scan(std::move(sources));
}

void Amphibia::_PollImportStatus()
{
  if (!mImportWorker) return;
  const auto progress = mImportWorker->Progress();
  const auto targetTag = mImportTarget == ImportTarget::IR ? kCtrlTagIRFileBrowser : kCtrlTagModelFileBrowser;
  if (progress.state == amphibia::library::ImportTaskState::AwaitingSelection &&
      progress.taskId != mReviewedImportTask)
  {
    const auto plan = mImportWorker->Plan();
    if (!plan) return;
    std::vector<std::size_t> selected;
    for (std::size_t index = 0; index < plan->entries.size(); ++index)
      if (plan->entries[index].selected &&
          plan->entries[index].status == amphibia::library::ImportEntryStatus::Candidate)
        selected.push_back(index);
    mReviewedImportTask = progress.taskId;
    std::string review = "Found " + std::to_string(selected.size()) + " supported candidate item(s), " +
                         std::to_string(plan->unsafeEntries) + " unsafe and " +
                         std::to_string(plan->unsupportedEntries) + " unsupported. Import selected items?";
    const auto previewCount = std::min<std::size_t>(selected.size(), 8);
    for (std::size_t preview = 0; preview < previewCount; ++preview)
    {
      const auto& entry = plan->entries[selected[preview]];
      review += "\n" + entry.displayName + " (" +
                (entry.fileType == amphibia::library::ImportedFileType::NamModel ? "NAM" : "WAV") + ", " +
                std::to_string(entry.uncompressedSize) + " bytes)";
    }
    if (selected.size() > previewCount)
      review += "\n...and " + std::to_string(selected.size() - previewCount) + " more";
    if (auto* graphics = GetUI())
    {
      const auto result = _ShowMessageBox(graphics, review.c_str(), "Review local import", kMB_YESNO);
      if (result == kYES) mImportWorker->ImportSelected(selected);
      else if (result != kNoResult) mImportWorker->Cancel();
    }
  }
  else if (progress.state == amphibia::library::ImportTaskState::Completed &&
           progress.taskId != mCompletedImportTask)
  {
    mCompletedImportTask = progress.taskId;
    if (const auto summary = mImportWorker->Summary(); summary && mImportTarget != ImportTarget::None)
    {
      const auto type = mImportTarget == ImportTarget::IR ?
        amphibia::library::ManagedObjectType::WaveImpulseResponse :
        amphibia::library::ManagedObjectType::NamModel;
      for (const auto& hash : summary->objects)
      {
        if (mLibrary && mLibrary->Resolve(hash, type))
        {
          _RequestManaged(hash, type);
          break;
        }
      }
    }
    if (auto* graphics = GetUI())
      if (auto* settings = graphics->GetControlWithTag(kCtrlTagSettingsBox))
        settings->As<NAMSettingsPageControl>()->RefreshLibraryInfo();
  }
  else if (progress.state == amphibia::library::ImportTaskState::Failed && progress.message)
  {
    if (progress.taskId == mCompletedImportTask) return;
    mCompletedImportTask = progress.taskId;
    SendControlMsgFromDelegate(targetTag, kMsgTagLoadFailed, static_cast<int>(progress.message->size()),
                               progress.message->c_str());
  }
  else if (progress.state != amphibia::library::ImportTaskState::Idle &&
           progress.state != amphibia::library::ImportTaskState::Cancelled)
  {
    const std::string text = progress.currentDisplayName.empty() ? "Importing local content..." :
      "Importing: " + progress.currentDisplayName;
    SendControlMsgFromDelegate(targetTag, kMsgTagLoadStatus, static_cast<int>(text.size()), text.c_str());
  }
}

amphibia::loading::PreparedDSP<ResamplingNAM, ModelLoadMetadata>
Amphibia::_PrepareModel(const amphibia::loading::LoadRequest& request,
                        const decltype(mModelLoader)::CancelCheck& cancelled,
                        const decltype(mModelLoader)::ProgressCallback& progress)
{
  using namespace amphibia::loading;
  const auto path = std::filesystem::u8path(request.path);
  InspectRegularFile(path, ".nam", kMaximumNamBytes);
  if (cancelled())
    throw LoadException(LoadErrorCode::Superseded, "Model load was superseded");

  nlohmann::json config;
  try
  {
    std::ifstream stream(path);
    if (!stream)
      throw LoadException(LoadErrorCode::FileUnreadable, "The model file could not be opened");
    stream >> config;
  }
  catch (const LoadException&)
  {
    throw;
  }
  catch (...)
  {
    throw LoadException(LoadErrorCode::InvalidModel, "The NAM file is malformed");
  }

  if (!config.is_object() || !config.contains("version") || !config.contains("architecture")
      || !config.contains("config") || !config.contains("weights"))
    throw LoadException(LoadErrorCode::InvalidModel, "The NAM file is missing required model data");

  ModelLoadMetadata metadata;
  try
  {
    const std::string architecture = config.at("architecture").get<std::string>();
    metadata.family = NamModelFamily::LegacyOrCustom;
#if defined(NAM_ENABLE_A2_FAST)
    if (architecture == "WaveNet")
    {
      int channels = 0;
      if (nam::wavenet::a2_fast::is_a2_shape(config.at("config"), &channels))
        metadata.family = channels == 3 ? NamModelFamily::A2Nano : NamModelFamily::A2Standard;
    }
#endif
  }
  catch (...)
  {
    throw LoadException(LoadErrorCode::InvalidModel, "The NAM architecture description is invalid");
  }

  progress(LoadState::Preparing, 0.35f);
  if (cancelled())
    throw LoadException(LoadErrorCode::Superseded, "Model load was superseded");

  std::unique_ptr<nam::DSP> model;
  try
  {
    model = nam::get_dsp(config);
  }
  catch (...)
  {
    throw LoadException(LoadErrorCode::InvalidModel, "The NAM model could not be constructed");
  }

  if (cancelled())
    throw LoadException(LoadErrorCode::Superseded, "Model load was superseded");
  if (model->NumInputChannels() != 1 || model->NumOutputChannels() != 1)
    throw LoadException(LoadErrorCode::UnsupportedArchitecture, "The model must have one input and one output");
  if (request.configuration.sampleRate <= 0.0 || request.configuration.maximumBlockSize <= 0)
    throw LoadException(LoadErrorCode::StaleProcessingConfiguration, "The audio configuration is not ready");

  progress(LoadState::Preparing, 0.75f);
  auto prepared = std::make_unique<ResamplingNAM>(std::move(model), request.configuration.sampleRate);
  prepared->Reset(request.configuration.sampleRate, request.configuration.maximumBlockSize);
  if (auto* slimmable = prepared->GetSlimmableModel())
    slimmable->SetSlimmableSize(request.userValue);

  metadata.sampleRate = prepared->GetEncapsulatedSampleRate();
  metadata.hasLoudness = prepared->HasLoudness();
  metadata.hasInputLevel = prepared->HasInputLevel();
  metadata.hasOutputLevel = prepared->HasOutputLevel();
  metadata.loudness = metadata.hasLoudness ? prepared->GetLoudness() : 0.0;
  metadata.inputLevel = metadata.hasInputLevel ? prepared->GetInputLevel() : 0.0;
  metadata.outputLevel = metadata.hasOutputLevel ? prepared->GetOutputLevel() : 0.0;
  metadata.slimmable = prepared->GetSlimmableModel() != nullptr;
  metadata.latency = prepared->GetLatency();
  return {std::move(prepared), metadata};
}

amphibia::loading::PreparedDSP<dsp::ImpulseResponse, IRLoadMetadata>
Amphibia::_PrepareIR(const amphibia::loading::LoadRequest& request,
                     const decltype(mIRLoader)::CancelCheck& cancelled,
                     const decltype(mIRLoader)::ProgressCallback& progress)
{
  using namespace amphibia::loading;
  const auto path = std::filesystem::u8path(request.path);
  const auto inspection = InspectWaveFile(path);
  if (cancelled())
    throw LoadException(LoadErrorCode::Superseded, "IR load was superseded");
  if (request.configuration.sampleRate <= 0.0 || request.configuration.maximumBlockSize <= 0)
    throw LoadException(LoadErrorCode::StaleProcessingConfiguration, "The audio configuration is not ready");

  progress(LoadState::Preparing, 0.45f);
  std::unique_ptr<dsp::ImpulseResponse> prepared;
  try
  {
    prepared = std::make_unique<dsp::ImpulseResponse>(amphibia::library::PathToUtf8(path).c_str(),
                                                      request.configuration.sampleRate);
  }
  catch (...)
  {
    throw LoadException(LoadErrorCode::PreparationFailed, "The IR could not be decoded or resampled");
  }
  if (prepared->GetWavState() != dsp::wav::LoadReturnCode::SUCCESS)
    throw LoadException(LoadErrorCode::InvalidWave, "The IR WAV could not be decoded");
  if (cancelled())
    throw LoadException(LoadErrorCode::Superseded, "IR load was superseded");

  // Exercise the maximum configured block with silence on the worker so the
  // convolution history/output buffers have allocated before publication.
  progress(LoadState::Preparing, 0.8f);
  std::vector<double> silence(static_cast<size_t>(request.configuration.maximumBlockSize), 0.0);
  double* channel = silence.data();
  prepared->Process(&channel, 1, silence.size());

  IRLoadMetadata metadata;
  metadata.sourceSampleRate = inspection.sampleRate;
  metadata.preparedSampleRate = request.configuration.sampleRate;
  metadata.dataBytes = inspection.dataBytes;
  return {std::move(prepared), metadata};
}

void Amphibia::_PollLoadingStatus()
{
  auto basename = [](const std::string& path) {
    if (path.empty())
      return std::string("None");
    try
    {
      return amphibia::library::PathToUtf8(std::filesystem::u8path(path).filename());
    }
    catch (...)
    {
      return std::string("active file");
    }
  };

  auto modelStatus = mModelLoader.Status();
  if (modelStatus.requestId != mLastModelStatusRequest || modelStatus.state != mLastModelLoadState)
  {
    mLastModelStatusRequest = modelStatus.requestId;
    mLastModelLoadState = modelStatus.state;
    if (modelStatus.state == amphibia::loading::LoadState::Active)
    {
      const auto path = mModelLoader.ActivePath();
      {
        std::lock_guard<std::mutex> lock(mPathMutex);
        mNAMPath.Set(path.c_str());
      }
      if (path.empty())
        SendControlMsgFromDelegate(kCtrlTagModelFileBrowser, kMsgTagLoadStatus, 7, "No model");
      else
        SendControlMsgFromDelegate(kCtrlTagModelFileBrowser, kMsgTagLoadedModel, static_cast<int>(path.size()), path.c_str());
      if (!path.empty() && mPendingManagedModel)
      {
        { std::lock_guard<std::mutex> lock(mPathMutex); mActiveManagedModel = mPendingManagedModel; }
        try { if (mLibrary) mLibrary->MarkUsed(*mPendingManagedModel); } catch (...) {}
        if (mPendingProviderActivation &&
            mPendingProviderActivation->kind != amphibia::providers::HostedSelectionKind::ImpulseResponse &&
            mPendingProviderActivation->managedHash == *mPendingManagedModel && mTone3000Selection)
        {
          mTone3000Selection->NotifyActivation(mPendingProviderActivation->taskId,
                                               mPendingProviderActivation->providerModelId, true);
          mPendingProviderActivation.reset();
        }
        mPendingManagedModel.reset();
      }
      else
      {
        std::lock_guard<std::mutex> lock(mPathMutex);
        mActiveManagedModel.reset();
      }
    }
    else if (modelStatus.state == amphibia::loading::LoadState::Failed)
    {
      mPendingManagedModel.reset();
      if (mPendingProviderActivation &&
          mPendingProviderActivation->kind != amphibia::providers::HostedSelectionKind::ImpulseResponse &&
          mTone3000Selection)
      {
        mTone3000Selection->NotifyActivation(mPendingProviderActivation->taskId,
                                             mPendingProviderActivation->providerModelId, false,
                                             "The downloaded NAM variant could not be prepared.");
        mPendingProviderActivation.reset();
      }
      const std::string text = "Failed: " + modelStatus.displayName + " (active: " + basename(mModelLoader.ActivePath()) + ")";
      SendControlMsgFromDelegate(kCtrlTagModelFileBrowser, kMsgTagLoadFailed, static_cast<int>(text.size()), text.c_str());
    }
    else if (modelStatus.state == amphibia::loading::LoadState::Inspecting
             || modelStatus.state == amphibia::loading::LoadState::Preparing
             || modelStatus.state == amphibia::loading::LoadState::ReadyToActivate)
    {
      const std::string text = "Loading: " + modelStatus.displayName + " (active: " + basename(mModelLoader.ActivePath()) + ")";
      SendControlMsgFromDelegate(kCtrlTagModelFileBrowser, kMsgTagLoadStatus, static_cast<int>(text.size()), text.c_str());
    }
  }

  auto irStatus = mIRLoader.Status();
  if (irStatus.requestId != mLastIRStatusRequest || irStatus.state != mLastIRLoadState)
  {
    mLastIRStatusRequest = irStatus.requestId;
    mLastIRLoadState = irStatus.state;
    if (irStatus.state == amphibia::loading::LoadState::Active)
    {
      const auto path = mIRLoader.ActivePath();
      {
        std::lock_guard<std::mutex> lock(mPathMutex);
        mIRPath.Set(path.c_str());
      }
      if (path.empty())
        SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadStatus, 5, "No IR");
      else
        SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadedIR, static_cast<int>(path.size()), path.c_str());
      if (!path.empty() && mPendingManagedIR)
      {
        { std::lock_guard<std::mutex> lock(mPathMutex); mActiveManagedIR = mPendingManagedIR; }
        try { if (mLibrary) mLibrary->MarkUsed(*mPendingManagedIR); } catch (...) {}
        if (mPendingProviderActivation &&
            mPendingProviderActivation->kind == amphibia::providers::HostedSelectionKind::ImpulseResponse &&
            mPendingProviderActivation->managedHash == *mPendingManagedIR && mTone3000Selection)
        {
          mTone3000Selection->NotifyActivation(mPendingProviderActivation->taskId,
                                               mPendingProviderActivation->providerModelId, true);
          mPendingProviderActivation.reset();
        }
        mPendingManagedIR.reset();
      }
      else
      {
        std::lock_guard<std::mutex> lock(mPathMutex);
        mActiveManagedIR.reset();
      }
    }
    else if (irStatus.state == amphibia::loading::LoadState::Failed)
    {
      mPendingManagedIR.reset();
      if (mPendingProviderActivation &&
          mPendingProviderActivation->kind == amphibia::providers::HostedSelectionKind::ImpulseResponse &&
          mTone3000Selection)
      {
        mTone3000Selection->NotifyActivation(mPendingProviderActivation->taskId,
                                             mPendingProviderActivation->providerModelId, false,
                                             "The downloaded IR variant could not be prepared.");
        mPendingProviderActivation.reset();
      }
      const std::string text = "Failed: " + irStatus.displayName + " (active: " + basename(mIRLoader.ActivePath()) + ")";
      SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadFailed, static_cast<int>(text.size()), text.c_str());
    }
    else if (irStatus.state == amphibia::loading::LoadState::Inspecting
             || irStatus.state == amphibia::loading::LoadState::Preparing
             || irStatus.state == amphibia::loading::LoadState::ReadyToActivate)
    {
      const std::string text = "Loading: " + irStatus.displayName + " (active: " + basename(mIRLoader.ActivePath()) + ")";
      SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadStatus, static_cast<int>(text.size()), text.c_str());
    }
  }
}

size_t Amphibia::_GetBufferNumChannels() const
{
  // Assumes input=output (no mono->stereo effects)
  return mInputArray.size();
}

size_t Amphibia::_GetBufferNumFrames() const
{
  if (_GetBufferNumChannels() == 0)
    return 0;
  return mInputArray[0].size();
}

void Amphibia::_InitToneStack()
{
  // If you want to customize the tone stack, then put it here!
  mToneStack = std::make_unique<dsp::tone_stack::BasicNamToneStack>();
}
void Amphibia::_PrepareBuffers(const size_t numChannels, const size_t numFrames)
{
  const bool updateChannels = numChannels != _GetBufferNumChannels();
  // OnReset reserves the advertised maximum. Variable smaller host blocks keep
  // the same storage, avoiding resize/allocation churn in ProcessBlock(). A
  // host violating its advertised maximum can still force one defensive grow.
  const bool updateFrames = updateChannels || (_GetBufferNumFrames() < numFrames);
  //  if (!updateChannels && !updateFrames)  // Could we do this?
  //    return;

  if (updateChannels)
  {
    _PrepareIOPointers(numChannels);
    mInputArray.resize(numChannels);
    mOutputArray.resize(numChannels);
  }
  if (updateFrames)
  {
    for (auto c = 0; c < mInputArray.size(); c++)
    {
      mInputArray[c].resize(numFrames);
      std::fill(mInputArray[c].begin(), mInputArray[c].end(), 0.0);
    }
    for (auto c = 0; c < mOutputArray.size(); c++)
    {
      mOutputArray[c].resize(numFrames);
      std::fill(mOutputArray[c].begin(), mOutputArray[c].end(), 0.0);
    }
  }
  // Would these ever get changed by something?
  for (auto c = 0; c < mInputArray.size(); c++)
    mInputPointers[c] = mInputArray[c].data();
  for (auto c = 0; c < mOutputArray.size(); c++)
    mOutputPointers[c] = mOutputArray[c].data();
}

void Amphibia::_PrepareIOPointers(const size_t numChannels)
{
  _DeallocateIOPointers();
  _AllocateIOPointers(numChannels);
}

void Amphibia::_ProcessInput(iplug::sample** inputs, const size_t nFrames, const size_t nChansIn,
                                     const size_t nChansOut)
{
  // We'll assume that the main processing is mono for now. We'll handle dual amps later.
  if (nChansOut != 1)
  {
    std::stringstream ss;
    ss << "Expected mono output, but " << nChansOut << " output channels are requested!";
    throw std::runtime_error(ss.str());
  }

  // On the standalone, we can probably assume that the user has plugged into only one input and they expect it to be
  // carried straight through. Don't apply any division over nChansIn because we're just "catching anything out there."
  // However, in a DAW, it's probably something providing stereo, and we want to take the average in order to avoid
  // doubling the loudness. (This would change w/ double mono processing)
  double gain = mInputGain;
#ifndef APP_API
  gain /= (float)nChansIn;
#endif
  // Assume _PrepareBuffers() was already called
  for (size_t c = 0; c < nChansIn; c++)
    for (size_t s = 0; s < nFrames; s++)
      if (c == 0)
        mInputArray[0][s] = gain * inputs[c][s];
      else
        mInputArray[0][s] += gain * inputs[c][s];
}

void Amphibia::_ProcessOutput(iplug::sample** inputs, iplug::sample** outputs, const size_t nFrames,
                                      const size_t nChansIn, const size_t nChansOut)
{
  const double gain = mOutputGain;
  // Assume _PrepareBuffers() was already called
  if (nChansIn != 1)
    throw std::runtime_error("Plugin is supposed to process in mono.");
  // Broadcast the internal mono stream to all output channels.
  const size_t cin = 0;
  for (auto cout = 0; cout < nChansOut; cout++)
    for (auto s = 0; s < nFrames; s++)
#ifdef APP_API // Ensure valid output to interface
      outputs[cout][s] = std::clamp(gain * inputs[cin][s], -1.0, 1.0);
#else // In a DAW, other things may come next and should be able to handle large
      // values.
      outputs[cout][s] = gain * inputs[cin][s];
#endif
}

void Amphibia::_UpdateControlsFromModel()
{
  if (!mModelLoader.HasRetainedProcessor())
  {
    return;
  }
  if (auto* pGraphics = GetUI())
  {
    ModelInfo modelInfo;
    modelInfo.sampleRate.known = true;
    modelInfo.sampleRate.value = mModelSampleRate.load(std::memory_order_acquire);
    modelInfo.inputCalibrationLevel.known = mModelHasInputLevel.load(std::memory_order_acquire);
    modelInfo.inputCalibrationLevel.value = mModelInputLevel.load(std::memory_order_acquire);
    modelInfo.outputCalibrationLevel.known = mModelHasOutputLevel.load(std::memory_order_acquire);
    modelInfo.outputCalibrationLevel.value = mModelOutputLevel.load(std::memory_order_acquire);

    static_cast<NAMSettingsPageControl*>(pGraphics->GetControlWithTag(kCtrlTagSettingsBox))->SetModelInfo(modelInfo);

    const bool disableInputCalibrationControls = !mModelHasInputLevel.load(std::memory_order_acquire);
    pGraphics->GetControlWithTag(kCtrlTagCalibrateInput)->SetDisabled(disableInputCalibrationControls);
    pGraphics->GetControlWithTag(kCtrlTagInputCalibrationLevel)->SetDisabled(disableInputCalibrationControls);
    {
      auto* c = static_cast<OutputModeControl*>(pGraphics->GetControlWithTag(kCtrlTagOutputMode));
      c->SetNormalizedDisable(!mModelHasLoudness.load(std::memory_order_acquire));
      c->SetCalibratedDisable(!mModelHasOutputLevel.load(std::memory_order_acquire));
    }

    if (auto* pSlimIcon = pGraphics->GetControlWithTag(kCtrlTagSlimmableIcon))
    {
      pSlimIcon->Hide(!mModelSlimmable.load(std::memory_order_acquire));
    }
  }
}

void Amphibia::_UpdateMeters(sample** inputPointer, sample** outputPointer, const size_t nFrames,
                                     const size_t nChansIn, const size_t nChansOut)
{
  // Right now, we didn't specify MAXNC when we initialized these, so it's 1.
  const int nChansHack = 1;
  mInputSender.ProcessBlock(inputPointer, (int)nFrames, kCtrlTagInputMeter, nChansHack);
  mOutputSender.ProcessBlock(outputPointer, (int)nFrames, kCtrlTagOutputMeter, nChansHack);
}

// HACK
#include "Loading/FileInspection.cpp"
#include "Library/ContentHash.cpp"
#include "Library/PathSafety.cpp"
#include "Library/FolderScanner.cpp"
#include "Library/ZipArchive.cpp"
#include "Library/LibraryIndex.cpp"
#include "Library/ManagedLibrary.cpp"
#include "Library/ImportWorker.cpp"
#include "Library/LibraryPaths.cpp"
#include "Unserialization.cpp"
