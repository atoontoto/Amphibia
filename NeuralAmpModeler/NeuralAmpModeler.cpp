#include <algorithm> // std::clamp, std::min
#include <cmath> // pow
#include <filesystem>
#include <fstream>
#include <iostream>
#include <utility>

#include "Colors.h"
#include "Loading/FileInspection.h"
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

using namespace iplug;
using namespace igraphics;

const double kDCBlockerFrequency = 5.0;

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
    const auto mainArea = b.GetPadded(-20);
    const auto contentArea = mainArea.GetPadded(-10);
    const auto titleHeight = 50.0f;
    const auto titleArea = contentArea.GetFromTop(titleHeight);

    // Areas for knobs
    const auto knobsPad = 20.0f;
    const auto knobsExtraSpaceBelowTitle = 25.0f;
    const auto singleKnobPad = -2.0f;
    const auto knobsArea = contentArea.GetFromTop(NAM_KNOB_HEIGHT)
                             .GetReducedFromLeft(knobsPad)
                             .GetReducedFromRight(knobsPad)
                             .GetVShifted(titleHeight + knobsExtraSpaceBelowTitle);
    const auto inputKnobArea = knobsArea.GetGridCell(0, kInputLevel, 1, numKnobs).GetPadded(-singleKnobPad);
    const auto noiseGateArea = knobsArea.GetGridCell(0, kNoiseGateThreshold, 1, numKnobs).GetPadded(-singleKnobPad);
    const auto bassKnobArea = knobsArea.GetGridCell(0, kToneBass, 1, numKnobs).GetPadded(-singleKnobPad);
    const auto midKnobArea = knobsArea.GetGridCell(0, kToneMid, 1, numKnobs).GetPadded(-singleKnobPad);
    const auto trebleKnobArea = knobsArea.GetGridCell(0, kToneTreble, 1, numKnobs).GetPadded(-singleKnobPad);
    const auto outputKnobArea = knobsArea.GetGridCell(0, kOutputLevel, 1, numKnobs).GetPadded(-singleKnobPad);

    const auto ngToggleArea =
      noiseGateArea.GetVShifted(noiseGateArea.H()).SubRectVertical(2, 0).GetReducedFromTop(10.0f);
    const auto eqToggleArea = midKnobArea.GetVShifted(midKnobArea.H()).SubRectVertical(2, 0).GetReducedFromTop(10.0f);

    // Areas for model and IR
    const auto fileWidth = 200.0f;
    const auto fileHeight = 30.0f;
    const auto irYOffset = 38.0f;
    const auto modelArea =
      contentArea.GetFromBottom((2.0f * fileHeight)).GetFromTop(fileHeight).GetMidHPadded(fileWidth).GetVShifted(-1);
    const auto slimIconArea =
      IRECT(modelArea.R + 6.f, modelArea.MH() - 14.f, modelArea.R + 6.f + 2.f * 28.f, modelArea.MH() + 14.f);
    const auto modelIconArea = modelArea.GetFromLeft(30).GetTranslated(-40, 10);
    const auto irArea = modelArea.GetVShifted(irYOffset);
    const auto irSwitchArea = irArea.GetFromLeft(30.0f).GetHShifted(-40.0f).GetScaledAboutCentre(0.6f);

    // Areas for meters
    const auto inputMeterArea = contentArea.GetFromLeft(30).GetHShifted(-20).GetMidVPadded(100).GetVShifted(-25);
    const auto outputMeterArea = contentArea.GetFromRight(30).GetHShifted(20).GetMidVPadded(100).GetVShifted(-25);

    // Misc Areas
    const auto settingsButtonArea = CornerButtonArea(b);

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
                                fileBackgroundBitmap, globeSVG, "Get NAM Models", getUrl),
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
                                "Get IRs", getUrl),
      kCtrlTagIRFileBrowser);
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
        pGraphics->GetControlWithTag(kCtrlTagSettingsBox)->As<NAMSettingsPageControl>()->HideAnimated(false);
      },
      gearSVG));

    pGraphics
      ->AttachControl(new NAMSettingsPageControl(b, backgroundBitmap, inputLevelBackgroundBitmap, switchHandleBitmap,
                                                 crossSVG, style, radioButtonStyle),
                      kCtrlTagSettingsBox)
      ->Hide(true);

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
  // Factories capture this plug-in, so join them while the complete owner is
  // still alive. Shutdown is idempotent for member teardown.
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
  // Model directory (don't serialize the model itself; we'll just load it again
  // when we unserialize)
  {
    std::lock_guard<std::mutex> lock(mPathMutex);
    chunk.PutStr(mNAMPath.Get());
    chunk.PutStr(mIRPath.Get());
  }
  return SerializeParams(chunk);
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
      return _UnserializeStateWithKnownVersion(chunk, pos);
    return _UnserializeStateWithUnknownVersion(chunk, startPos);
  }
  catch (...)
  {
    // Host-provided state is untrusted. Parameters and active DSP remain as
    // they were when a chunk cannot be decoded completely.
    return -1;
  }
}

void Amphibia::OnUIOpen()
{
  Plugin::OnUIOpen();

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
      return true;
    }
    case kMsgTagClearIR:
    {
      std::lock_guard<std::mutex> lock(mProcessingConfigurationMutex);
      mIRLoader.SubmitClear(mProcessingConfiguration);
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
  std::lock_guard<std::mutex> lock(mProcessingConfigurationMutex);
  mModelLoader.Submit(modelPath.Get(), mProcessingConfiguration, GetParam(kSlim)->Value());
}

void Amphibia::_RequestIR(const WDL_String& irPath)
{
  std::lock_guard<std::mutex> lock(mProcessingConfigurationMutex);
  mIRLoader.Submit(irPath.Get(), mProcessingConfiguration);
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
    prepared = std::make_unique<dsp::ImpulseResponse>(path.string().c_str(), request.configuration.sampleRate);
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
      return std::filesystem::u8path(path).filename().string();
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
    }
    else if (modelStatus.state == amphibia::loading::LoadState::Failed)
    {
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
    }
    else if (irStatus.state == amphibia::loading::LoadState::Failed)
    {
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
#include "Unserialization.cpp"
