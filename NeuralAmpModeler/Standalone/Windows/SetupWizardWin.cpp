#include "SetupWizardWin.h"

#include "AmphibiaAPPHost.h"
#include "../AudioConfiguration.h"
#include "../FolderValidation.h"
#include "../SetupController.h"
#include "../../config.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <future>
#include <memory>
#include <sstream>
#include <string>

#include <commctrl.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shellapi.h>

namespace amphibia::setup
{
namespace
{
constexpr int kBack = 51001;
constexpr int kNext = 51002;
constexpr int kSkip = 51003;
constexpr int kCancel = 51004;
constexpr int kAction = 51005;
constexpr int kRefresh = 51006;
constexpr int kBackend = 51010;
constexpr int kInput = 51011;
constexpr int kOutput = 51012;
constexpr int kInputChannel = 51013;
constexpr int kOutputChannels = 51014;
constexpr int kSampleRate = 51015;
constexpr int kBufferSize = 51016;
constexpr int kTitle = 51020;
constexpr int kBody = 51021;
constexpr int kStatus = 51022;
constexpr int kInputMeter = 51023;
constexpr int kOutputMeter = 51024;
constexpr int kLibraryOpen = 51030;
constexpr int kLibraryVerify = 51031;
constexpr int kLibraryClean = 51032;
constexpr int kLibraryFiles = 51033;
constexpr int kLibraryFolder = 51034;
constexpr int kLibraryZip = 51035;

std::wstring Wide(const std::string& value)
{
  if (value.empty()) return {};
  const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                                        nullptr, 0);
  if (count <= 0) return L"(invalid text)";
  std::wstring result(static_cast<std::size_t>(count), L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), count);
  return result;
}

std::string Utf8(const std::wstring& value)
{
  if (value.empty()) return {};
  const int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                                        nullptr, 0, nullptr, nullptr);
  if (count <= 0) return {};
  std::string result(static_cast<std::size_t>(count), '\0');
  WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), count,
                      nullptr, nullptr);
  return result;
}

void SetText(HWND window, const std::wstring& text) { SetWindowTextW(window, text.c_str()); }

int Selection(HWND combo) { return static_cast<int>(SendMessageW(combo, CB_GETCURSEL, 0, 0)); }

void AddCombo(HWND combo, const std::wstring& text)
{
  SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
}

std::wstring ChooseFolder(HWND owner)
{
  IFileDialog* dialog{};
  if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) return {};
  DWORD options{};
  dialog->GetOptions(&options);
  dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_DONTADDTORECENT);
  std::wstring path;
  if (SUCCEEDED(dialog->Show(owner)))
  {
    IShellItem* item{};
    if (SUCCEEDED(dialog->GetResult(&item)))
    {
      PWSTR display{};
      if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &display)))
      {
        path = display;
        CoTaskMemFree(display);
      }
      item->Release();
    }
  }
  dialog->Release();
  return path;
}

std::vector<std::filesystem::path> ChooseFiles(HWND owner, const wchar_t* label, const wchar_t* pattern,
                                               bool multiple)
{
  IFileOpenDialog* dialog{};
  if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) return {};
  DWORD options{};
  dialog->GetOptions(&options);
  dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST |
                     FOS_DONTADDTORECENT | (multiple ? FOS_ALLOWMULTISELECT : 0));
  const COMDLG_FILTERSPEC filter{label, pattern};
  dialog->SetFileTypes(1, &filter);
  std::vector<std::filesystem::path> paths;
  if (SUCCEEDED(dialog->Show(owner)))
  {
    IShellItemArray* items{};
    if (SUCCEEDED(dialog->GetResults(&items)))
    {
      DWORD count{};
      items->GetCount(&count);
      paths.reserve(count);
      for (DWORD index = 0; index < count; ++index)
      {
        IShellItem* item{};
        if (FAILED(items->GetItemAt(index, &item))) continue;
        PWSTR display{};
        if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &display)))
        {
          paths.emplace_back(display);
          CoTaskMemFree(display);
        }
        item->Release();
      }
      items->Release();
    }
  }
  dialog->Release();
  return paths;
}

class SetupWizard
{
public:
  SetupWizard(HWND parent, iplug::IPlugAPPHost& host)
  : mParent(parent), mHost(host), mTransaction(host), mController(LoadDocument(), AMPHIBIA_VERSION)
  {
  }

  bool Show(WizardPage initialPage)
  {
    mController.GoTo(initialPage);
    WNDCLASSEXW windowClass{sizeof(windowClass)};
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = L"AmphibiaSetupWizardV1";
    RegisterClassExW(&windowClass);
    mWindow = CreateWindowExW(WS_EX_DLGMODALFRAME, windowClass.lpszClassName, L"Amphibia Setup",
                              WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                              CW_USEDEFAULT, CW_USEDEFAULT, 780, 650, mParent, nullptr,
                              windowClass.hInstance, this);
    if (!mWindow) return false;
    EnableWindow(mParent, FALSE);
    ShowWindow(mWindow, SW_SHOW);
    UpdateWindow(mWindow);
    MSG message{};
    while (IsWindow(mWindow) && GetMessageW(&message, nullptr, 0, 0) > 0)
    {
      if (!IsDialogMessageW(mWindow, &message))
      {
        TranslateMessage(&message);
        DispatchMessageW(&message);
      }
    }
    EnableWindow(mParent, TRUE);
    SetForegroundWindow(mParent);
    return mCompleted;
  }

private:
  SettingsDocument LoadDocument()
  {
    if (!mHost.HasSetupSettings()) return {};
    const auto loaded = mHost.SetupSettings().Load();
    if (loaded.status == SettingsLoadStatus::Loaded || loaded.status == SettingsLoadStatus::RecoveredFromBackup)
      return loaded.document;
    return {};
  }

  static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
  {
    SetupWizard* self = reinterpret_cast<SetupWizard*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE)
    {
      self = static_cast<SetupWizard*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
      SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
      self->mWindow = window;
    }
    return self ? self->Handle(message, wParam, lParam) : DefWindowProcW(window, message, wParam, lParam);
  }

  HWND Child(const wchar_t* type, const wchar_t* text, DWORD style, int x, int y, int width, int height, int id)
  {
    auto control = CreateWindowExW(0, type, text, WS_CHILD | style, x, y, width, height, mWindow,
                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(mFont), TRUE);
    return control;
  }

  void CreateControls()
  {
    mFont = CreateFontW(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    mTitleFont = CreateFontW(-29, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    mTitle = Child(L"STATIC", L"", WS_VISIBLE, 28, 20, 710, 42, kTitle);
    SendMessageW(mTitle, WM_SETFONT, reinterpret_cast<WPARAM>(mTitleFont), TRUE);
    mBody = Child(L"EDIT", L"", WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
                  28, 70, 710, 170, kBody);
    mStatus = Child(L"STATIC", L"", WS_VISIBLE, 28, 510, 710, 45, kStatus);
    mBack = Child(L"BUTTON", L"Back", WS_VISIBLE | WS_TABSTOP, 28, 565, 100, 34, kBack);
    mSkip = Child(L"BUTTON", L"Skip", WS_VISIBLE | WS_TABSTOP, 138, 565, 100, 34, kSkip);
    mCancel = Child(L"BUTTON", L"Cancel", WS_VISIBLE | WS_TABSTOP, 528, 565, 100, 34, kCancel);
    mNext = Child(L"BUTTON", L"Next", WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 638, 565, 100, 34, kNext);
    mAction = Child(L"BUTTON", L"", WS_TABSTOP, 28, 460, 235, 34, kAction);
    mRefresh = Child(L"BUTTON", L"Refresh devices", WS_TABSTOP, 275, 460, 160, 34, kRefresh);
    mBackend = Child(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 210, 255, 340, 200, kBackend);
    mInput = Child(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 210, 290, 510, 250, kInput);
    mOutput = Child(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 210, 325, 510, 250, kOutput);
    mInputChannel = Child(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 210, 360, 220, 220, kInputChannel);
    mOutputChannels = Child(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 500, 360, 220, 220, kOutputChannels);
    mSampleRate = Child(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 210, 395, 220, 220, kSampleRate);
    mBufferSize = Child(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 500, 395, 220, 220, kBufferSize);
    mInputMeter = Child(L"STATIC", L"Input: no activity", 0, 28, 285, 710, 28, kInputMeter);
    mOutputMeter = Child(L"STATIC", L"Output: stopped", 0, 28, 320, 710, 28, kOutputMeter);
    constexpr const wchar_t* libraryLabels[6]{L"Open Library Folder", L"Verify Library", L"Clean Staging",
                                               L"Import Files", L"Import Folder", L"Import ZIP"};
    constexpr int libraryIds[6]{kLibraryOpen, kLibraryVerify, kLibraryClean, kLibraryFiles, kLibraryFolder, kLibraryZip};
    for (int index = 0; index < 6; ++index)
      mLibraryActions[index] = Child(L"BUTTON", libraryLabels[index], WS_TABSTOP,
                                     28 + (index % 3) * 235, 280 + (index / 3) * 42, 220, 34,
                                     libraryIds[index]);
    RefreshDevices();
    SetTimer(mWindow, 1, 100, nullptr);
    Render();
  }

  void ShowControl(HWND control, bool visible) { ShowWindow(control, visible ? SW_SHOW : SW_HIDE); }

  void AddLabels(bool show)
  {
    if (!mAudioLabelsCreated)
    {
      mAudioLabels[0] = Child(L"STATIC", L"Driver", 0, 28, 258, 170, 24, 51100);
      mAudioLabels[1] = Child(L"STATIC", L"Input device", 0, 28, 293, 170, 24, 51101);
      mAudioLabels[2] = Child(L"STATIC", L"Output device", 0, 28, 328, 170, 24, 51102);
      mAudioLabels[3] = Child(L"STATIC", L"Input channel", 0, 28, 363, 170, 24, 51103);
      mAudioLabels[4] = Child(L"STATIC", L"Output channels", 0, 445, 363, 150, 24, 51104);
      mAudioLabels[5] = Child(L"STATIC", L"Sample rate", 0, 28, 398, 170, 24, 51105);
      mAudioLabels[6] = Child(L"STATIC", L"Buffer size", 0, 445, 398, 150, 24, 51106);
      mAudioLabelsCreated = true;
    }
    for (auto label : mAudioLabels) ShowControl(label, show);
  }

  void Render()
  {
    const auto page = mController.Page();
    SetText(mTitle, Wide(std::string(ToString(page)) + "  (" + std::to_string(static_cast<int>(page) + 1) + " of 9)"));
    std::wstring body;
    switch (page)
    {
      case WizardPage::Welcome:
        body = L"Welcome to Amphibia\r\n\r\nAmphibia loads Neural Amp Modeler captures and cabinet impulse responses. "
               L"Use your own local NAM and IR files, import tone packs, or optionally connect TONE3000.\r\n\r\n"
               L"Amphibia is free and open source. Local operation needs no account. TONE3000 is optional. "
               L"Amphibia is independent from the official Neural Amp Modeler application. No audio, local NAM files, "
               L"or local IR files are uploaded.";
        break;
      case WizardPage::AudioDevice:
        body = L"Choose the actual driver, input, guitar channel, output pair, sample rate, and buffer. "
               L"ASIO is preferred on Windows when a suitable driver is installed, but driver quality varies.\r\n"
               L"48 kHz is a good default for many NAM captures; 44.1 and 48 kHz are both supported. "
               L"Higher rates use more CPU. 64-128 samples favors latency, 256 is balanced, and 512+ is safer for CPU.";
        break;
      case WizardPage::InputOutputTest:
        body = L"Play your guitar and confirm the input meter moves. Input audio is not recorded, stored, logged, or uploaded.\r\n\r\n"
               L"Before starting the output test, lower speaker or headphone volume. The tone starts only after the explicit button, "
               L"is conservatively limited, fades in/out, and stops automatically.";
        break;
      case WizardPage::NamFolder:
        body = L"Choose Existing NAM Folder, keep the current folder, or Skip for Now. The folder is referenced in place. "
               L"It is not copied, modified, uploaded, recursively scanned, or automatically imported. Previous/Next remains same-directory only. "
               L"Recursive managed import is available separately.";
        break;
      case WizardPage::IrFolder:
        body = L"IRs are optional for amp+cab captures. Amp-head captures usually need a cabinet IR. The selected folder is referenced in place, "
               L"not recursively imported or modified; compatible counts describe only this directory.";
        break;
      case WizardPage::ManagedLibrary:
        body = L"TONE3000 downloads and imported ZIP contents are stored in Amphibia's managed library. Referenced local folders remain separate.\r\n\r\n" +
               Wide(LibraryStatus());
        break;
      case WizardPage::Tone3000:
        body = L"TONE3000 is optional. Use its hosted browser to select community NAM captures and IRs. Entering this page does not connect or open a browser.\r\n\r\n"
               L"TONE3000 integration is not configured in this build. A registered publishable client ID and approved callback are required. "
               L"Local NAM and IR functionality is unaffected.";
        break;
      case WizardPage::Summary: body = Wide(Summary()); break;
      case WizardPage::Finish:
        body = L"Setup complete\r\n\r\nYou can change these settings at any time.\r\n\r\n"
               L"Choose Start Using Amphibia, open Audio Settings, choose a NAM folder, or open the local documentation from the app menu.";
        break;
    }
    SetText(mBody, body);
    SetText(mStatus, L"");
    const bool audioPage = page == WizardPage::AudioDevice;
    AddLabels(audioPage);
    for (auto control : {mBackend, mInput, mOutput, mInputChannel, mOutputChannels, mSampleRate, mBufferSize})
      ShowControl(control, audioPage);
    ShowControl(mRefresh, audioPage);
    ShowControl(mInputMeter, page == WizardPage::InputOutputTest);
    ShowControl(mOutputMeter, page == WizardPage::InputOutputTest);
    for (auto control : mLibraryActions) ShowControl(control, page == WizardPage::ManagedLibrary);
    const bool action = page == WizardPage::InputOutputTest || page == WizardPage::NamFolder ||
                        page == WizardPage::IrFolder || page == WizardPage::Finish;
    ShowControl(mAction, action);
    if (page == WizardPage::InputOutputTest) SetText(mAction, mHost.CallbackActive() ? L"Start safe output test" : L"Audio unavailable");
    else if (page == WizardPage::NamFolder) SetText(mAction, L"Choose Existing NAM Folder");
    else if (page == WizardPage::IrFolder) SetText(mAction, L"Choose Existing IR Folder");
    else if (page == WizardPage::Finish) SetText(mAction, L"Open Audio Settings");
    if (page == WizardPage::ManagedLibrary)
    {
      const auto library = mHost.ManagedLibraryConfiguration();
      mController.Draft().libraryAvailable = library.available;
      mController.Draft().libraryValidated = library.verificationComplete && library.healthy;
      SetText(mLibraryActions[1], library.taskActive ? L"Cancel Library Task" : L"Verify Library");
      for (std::size_t index = 0; index < 6; ++index)
        EnableWindow(mLibraryActions[index], !library.taskActive || index == 1);
    }
    EnableWindow(mBack, page != WizardPage::Welcome && page != WizardPage::Finish);
    ShowControl(mSkip, page == WizardPage::NamFolder || page == WizardPage::IrFolder ||
                       page == WizardPage::ManagedLibrary || page == WizardPage::Tone3000);
    ShowControl(mCancel, page != WizardPage::Finish);
    SetText(mNext, page == WizardPage::Summary ? L"Finish" : page == WizardPage::Finish ? L"Start Using Amphibia" : L"Next");
    SetFocus(page == WizardPage::Welcome ? mNext : page == WizardPage::AudioDevice ? mBackend : mNext);
  }

  std::string LibraryStatus() const
  {
    return mHost.ManagedLibraryConfiguration().information;
  }

  void RefreshLibraryPage()
  {
    const auto library = mHost.ManagedLibraryConfiguration();
    mController.Draft().libraryAvailable = library.available;
    mController.Draft().libraryValidated = library.verificationComplete && library.healthy;
    SetText(mBody, L"TONE3000 downloads and imported ZIP contents are stored in Amphibia's managed library. "
                   L"Referenced local folders remain separate.\r\n\r\n" + Wide(library.information));
    SetText(mLibraryActions[1], library.taskActive ? L"Cancel Library Task" : L"Verify Library");
    for (std::size_t index = 0; index < 6; ++index)
      EnableWindow(mLibraryActions[index], !library.taskActive || index == 1);
  }

  std::string Summary() const
  {
    const auto& draft = mController.Draft();
    std::ostringstream stream;
    if (draft.audio)
      stream << "Audio driver: " << draft.audio->backendName << "\r\nInput: " << draft.audio->inputDeviceName
             << " / channel " << draft.audio->inputChannel << "\r\nOutput: " << draft.audio->outputDeviceName
             << " / channels " << draft.audio->outputFirstChannel << '-' <<
                (draft.audio->outputFirstChannel + draft.audio->outputChannelCount - 1)
             << "\r\nSample rate: " << draft.audio->sampleRate << " Hz\r\nBuffer size: " << draft.audio->bufferSize << " samples\r\n";
    else stream << "Audio driver: Not configured (warning)\r\n";
    stream << "\r\nNAM folder: " << (draft.namFolder ? *draft.namFolder : draft.namFolderSkipped ? "Skipped" : "Not configured")
           << "\r\nIR folder: " << (draft.irFolder ? *draft.irFolder : draft.irFolderSkipped ? "Skipped" : "Not configured")
           << "\r\nManaged library: " << (draft.libraryAvailable ? "Available" : "Unavailable; local references still work")
           << "\r\nTONE3000: " << (draft.tone3000Configured ? "Configured" : draft.tone3000Skipped ? "Skipped" : "Unavailable in this build");
    return stream.str();
  }

  void RefreshDevices()
  {
    mBackends = mHost.Enumerate();
    SendMessageW(mBackend, CB_RESETCONTENT, 0, 0);
    for (const auto& backend : mBackends) AddCombo(mBackend, Wide(backend.name));
    int selected = 0;
    const auto preferred = std::find_if(mBackends.begin(), mBackends.end(), [](const auto& value) { return value.preferred; });
    if (preferred != mBackends.end()) selected = static_cast<int>(std::distance(mBackends.begin(), preferred));
    if (mController.Draft().audio)
      for (std::size_t index = 0; index < mBackends.size(); ++index)
        if (mBackends[index].id == mController.Draft().audio->backendId) selected = static_cast<int>(index);
    SendMessageW(mBackend, CB_SETCURSEL, selected, 0);
    PopulateDevices();
  }

  const AudioBackendDescriptor* Backend() const
  {
    const auto index = Selection(mBackend);
    return index >= 0 && static_cast<std::size_t>(index) < mBackends.size() ? &mBackends[static_cast<std::size_t>(index)] : nullptr;
  }

  void PopulateDevices()
  {
    SendMessageW(mInput, CB_RESETCONTENT, 0, 0);
    SendMessageW(mOutput, CB_RESETCONTENT, 0, 0);
    mInputDevices.clear();
    mOutputDevices.clear();
    mInputDevices.push_back(nullptr);
    AddCombo(mInput, L"No input (output only)");
    if (const auto* backend = Backend())
      for (const auto& device : backend->devices)
      {
        if (!device.inputChannelNames.empty()) { mInputDevices.push_back(&device); AddCombo(mInput, Wide(device.name)); }
        if (!device.outputChannelNames.empty()) { mOutputDevices.push_back(&device); AddCombo(mOutput, Wide(device.name)); }
      }
    auto choose = [&](HWND combo, const auto& values, bool input) {
      int selection = values.empty() ? -1 : 0;
      if (mController.Draft().audio)
        for (std::size_t index = 0; index < values.size(); ++index)
          if ((!values[index] && input && mController.Draft().audio->inputDeviceId.empty()) ||
              (values[index] && values[index]->id == (input ? mController.Draft().audio->inputDeviceId : mController.Draft().audio->outputDeviceId)))
            selection = static_cast<int>(index);
      SendMessageW(combo, CB_SETCURSEL, selection, 0);
    };
    choose(mInput, mInputDevices, true);
    choose(mOutput, mOutputDevices, false);
    PopulateCapabilities();
  }

  void PopulateCapabilities()
  {
    SendMessageW(mInputChannel, CB_RESETCONTENT, 0, 0);
    SendMessageW(mOutputChannels, CB_RESETCONTENT, 0, 0);
    SendMessageW(mSampleRate, CB_RESETCONTENT, 0, 0);
    SendMessageW(mBufferSize, CB_RESETCONTENT, 0, 0);
    const auto inputSelection = Selection(mInput);
    const auto outputSelection = Selection(mOutput);
    if (inputSelection < 0 || outputSelection < 0 || static_cast<std::size_t>(inputSelection) >= mInputDevices.size() ||
        static_cast<std::size_t>(outputSelection) >= mOutputDevices.size()) return;
    const auto* input = mInputDevices[static_cast<std::size_t>(inputSelection)];
    const auto& output = *mOutputDevices[static_cast<std::size_t>(outputSelection)];
    if (input) for (const auto& label : input->inputChannelNames) AddCombo(mInputChannel, Wide(label));
    else AddCombo(mInputChannel, L"No input");
    for (std::size_t first = 0; first + 1 < output.outputChannelNames.size(); first += 2)
      AddCombo(mOutputChannels, Wide(output.outputChannelNames[first] + " + " + output.outputChannelNames[first + 1]));
    for (const auto rate : output.sampleRates)
      if (!input || std::find(input->sampleRates.begin(), input->sampleRates.end(), rate) != input->sampleRates.end())
        AddCombo(mSampleRate, std::to_wstring(rate));
    for (const auto size : output.bufferSizes) AddCombo(mBufferSize, std::to_wstring(size));
    SendMessageW(mInputChannel, CB_SETCURSEL, 0, 0);
    SendMessageW(mOutputChannels, CB_SETCURSEL, 0, 0);
    auto selectText = [](HWND combo, const wchar_t* preferred, int fallback) {
      const auto found = SendMessageW(combo, CB_FINDSTRINGEXACT, -1, reinterpret_cast<LPARAM>(preferred));
      SendMessageW(combo, CB_SETCURSEL, found >= 0 ? found : fallback, 0);
    };
    selectText(mSampleRate, L"48000", 0);
    selectText(mBufferSize, L"256", 0);
  }

  std::optional<AudioConfiguration> RequestedAudio() const
  {
    const auto* backend = Backend();
    const auto input = Selection(mInput);
    const auto output = Selection(mOutput);
    const auto inputChannel = Selection(mInputChannel);
    const auto outputPair = Selection(mOutputChannels);
    const auto rate = Selection(mSampleRate);
    const auto buffer = Selection(mBufferSize);
    if (!backend || input < 0 || output < 0 || inputChannel < 0 || outputPair < 0 || rate < 0 || buffer < 0 ||
        static_cast<std::size_t>(input) >= mInputDevices.size() || static_cast<std::size_t>(output) >= mOutputDevices.size())
      return std::nullopt;
    wchar_t rateText[32]{}, bufferText[32]{};
    SendMessageW(mSampleRate, CB_GETLBTEXT, rate, reinterpret_cast<LPARAM>(rateText));
    SendMessageW(mBufferSize, CB_GETLBTEXT, buffer, reinterpret_cast<LPARAM>(bufferText));
    AudioConfiguration value;
    value.backendId = backend->id;
    value.backendName = backend->name;
    const auto* inputDevice = mInputDevices[static_cast<std::size_t>(input)];
    value.inputDeviceId = inputDevice ? inputDevice->id : std::string{};
    value.inputDeviceName = inputDevice ? inputDevice->name : "No input";
    value.outputDeviceId = mOutputDevices[static_cast<std::size_t>(output)]->id;
    value.outputDeviceName = mOutputDevices[static_cast<std::size_t>(output)]->name;
    value.inputChannel = inputDevice ? static_cast<std::uint32_t>(inputChannel + 1) : 0;
    value.outputFirstChannel = static_cast<std::uint32_t>(outputPair * 2 + 1);
    value.outputChannelCount = 2;
    value.sampleRate = static_cast<std::uint32_t>(std::wcstoul(rateText, nullptr, 10));
    value.bufferSize = static_cast<std::uint32_t>(std::wcstoul(bufferText, nullptr, 10));
    return value;
  }

  bool ApplyAudio()
  {
    const auto requested = RequestedAudio();
    if (!requested)
    {
      if (MessageBoxW(mWindow,
                      L"No usable output configuration is selected. Continue setup without audio? You can repair this later.",
                      L"Continue Without Audio", MB_YESNO | MB_ICONWARNING) == IDYES)
      {
        mHost.Close();
        mController.Draft().audio.reset();
        return true;
      }
      SetText(mStatus, L"Choose a valid output configuration or explicitly continue without audio.");
      return false;
    }
    const auto result = mTransaction.Apply(*requested);
    SetText(mStatus, Wide(result.message));
    if (result.committed)
    {
      mController.Draft().audio = result.active;
      return true;
    }
    if (result.noAudio && MessageBoxW(mWindow,
                                      L"The requested device could not be opened. Continue setup without audio?",
                                      L"Audio Unavailable", MB_YESNO | MB_ICONWARNING) == IDYES)
    {
      mController.Draft().audio.reset();
      return true;
    }
    return false;
  }

  void ChooseLocalFolder(bool nam)
  {
    const auto selected = ChooseFolder(mWindow);
    if (selected.empty()) return;
    if (mFolderValidation.valid()) { SetText(mStatus, L"Folder validation is already running."); return; }
    mPendingFolder = selected;
    mPendingFolderIsNam = nam;
    SetText(mStatus, L"Validating the selected folder without recursion...");
    mFolderValidation = std::async(std::launch::async, [selected, nam] {
      return ValidateLocalFolder(std::filesystem::path(selected), nam ? ".nam" : ".wav");
    });
  }

  void CompleteFolderValidation()
  {
    if (!mFolderValidation.valid() ||
        mFolderValidation.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    const auto validation = mFolderValidation.get();
    if (validation.status != FolderValidationStatus::Available)
    {
      SetText(mStatus, Wide(validation.message));
      return;
    }
    if (mPendingFolderIsNam) { mController.Draft().namFolder = Utf8(mPendingFolder); mController.Draft().namFolderSkipped = false; }
    else { mController.Draft().irFolder = Utf8(mPendingFolder); mController.Draft().irFolderSkipped = false; }
    std::ostringstream message;
    message << "Folder selected. Compatible same-directory files: " << validation.compatibleFiles
            << (validation.countTruncated ? "+" : "");
    SetText(mStatus, Wide(message.str()));
  }

  void SkipPage()
  {
    if (mController.Page() == WizardPage::NamFolder) { mController.Draft().namFolder.reset(); mController.Draft().namFolderSkipped = true; }
    if (mController.Page() == WizardPage::IrFolder) { mController.Draft().irFolder.reset(); mController.Draft().irFolderSkipped = true; }
    if (mController.Page() == WizardPage::Tone3000) mController.Draft().tone3000Skipped = true;
    std::string error;
    mController.Next(error);
    Render();
  }

  void Cancel(bool explicitButton)
  {
    mHost.StopTestTone();
    if (mHost.ManagedLibraryConfiguration().taskActive) mHost.CancelManagedLibraryTask();
    mController.Cancel(explicitButton);
    if (mHost.HasSetupSettings())
    {
      std::string ignored;
      mHost.SetupSettings().Save(mController.Document(), ignored);
    }
    DestroyWindow(mWindow);
  }

  void Next()
  {
    if (mController.Page() == WizardPage::AudioDevice && !ApplyAudio()) return;
    if (mController.Page() == WizardPage::InputOutputTest) mHost.StopTestTone();
    if (mController.Page() == WizardPage::Summary)
    {
      std::string error;
      const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                         std::chrono::system_clock::now().time_since_epoch()).count();
      if (!mController.Finish(mHost.SetupSettings(), now, error)) { SetText(mStatus, Wide(error)); return; }
      if (const auto audio = mController.Draft().audio) mHost.UpdateINI();
      mCompleted = true;
      mController.GoTo(WizardPage::Finish);
      Render();
      return;
    }
    if (mController.Page() == WizardPage::Finish) { DestroyWindow(mWindow); return; }
    std::string error;
    if (!mController.Next(error)) { SetText(mStatus, Wide(error)); return; }
    Render();
  }

  void Action()
  {
    switch (mController.Page())
    {
      case WizardPage::InputOutputTest:
        if (!mHost.CallbackActive()) { SetText(mStatus, L"Audio is unavailable. Configure an output first."); return; }
        if (MessageBoxW(mWindow, L"Lower speaker or headphone volume before the bounded output test.",
                        L"Amphibia Output Test", MB_OKCANCEL | MB_ICONWARNING) != IDOK) return;
        if (mHost.StartTestTone())
          SetText(mStatus, L"Output test started at a conservative level and will stop automatically.");
        else { mHost.StopTestTone(); SetText(mStatus, L"Output test is stopping."); }
        break;
      case WizardPage::NamFolder: ChooseLocalFolder(true); break;
      case WizardPage::IrFolder: ChooseLocalFolder(false); break;
      case WizardPage::Finish:
        mController.GoTo(WizardPage::AudioDevice);
        Render();
        break;
      default: break;
    }
  }

  void LibraryAction(int action)
  {
    const auto current = mHost.ManagedLibraryConfiguration();
    switch (action)
    {
      case kLibraryOpen:
        if (!current.rootPath.empty())
          ShellExecuteW(mWindow, L"open", Wide(current.rootPath).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        break;
      case kLibraryVerify:
        if (current.taskActive) { mHost.CancelManagedLibraryTask(); SetText(mStatus, L"Library task cancelled."); }
        else { mHost.VerifyManagedLibrary(); SetText(mStatus, L"Library verification started in the background."); }
        break;
      case kLibraryClean:
        if (MessageBoxW(mWindow, L"Remove only abandoned files from Amphibia's safe staging area? Managed objects are not deleted.",
                        L"Clean Library Staging", MB_OKCANCEL | MB_ICONQUESTION) == IDOK)
        {
          mHost.ClearManagedLibraryStaging();
          SetText(mStatus, L"Safe staging cleanup started in the background.");
        }
        break;
      case kLibraryFiles:
      {
        auto paths = ChooseFiles(mWindow, L"NAM models and WAV impulse responses", L"*.nam;*.wav", true);
        if (!paths.empty()) { mHost.ScanManagedLibrarySources(std::move(paths)); SetText(mStatus, L"Import scan started."); }
        break;
      }
      case kLibraryFolder:
      {
        const auto path = ChooseFolder(mWindow);
        if (!path.empty()) { mHost.ScanManagedLibrarySources({std::filesystem::path(path)}); SetText(mStatus, L"Folder import scan started."); }
        break;
      }
      case kLibraryZip:
      {
        auto paths = ChooseFiles(mWindow, L"ZIP tone packs", L"*.zip", false);
        if (!paths.empty()) { mHost.ScanManagedLibrarySources(std::move(paths)); SetText(mStatus, L"ZIP safety scan started."); }
        break;
      }
    }
    RefreshLibraryPage();
  }

  LRESULT Handle(UINT message, WPARAM wParam, LPARAM lParam)
  {
    switch (message)
    {
      case WM_CREATE: CreateControls(); return 0;
      case WM_TIMER:
        CompleteFolderValidation();
        if (mController.Page() == WizardPage::ManagedLibrary && ++mLibraryRefreshTicks >= 5)
        {
          mLibraryRefreshTicks = 0;
          RefreshLibraryPage();
        }
        if (mController.Page() == WizardPage::InputOutputTest)
        {
          std::wostringstream input, output;
          input << L"Input activity: " << static_cast<int>(mHost.InputPeak() * 100.0) << L"%";
          output << L"Output level: " << static_cast<int>(mHost.OutputPeak() * 100.0) << L"%";
          SetText(mInputMeter, input.str()); SetText(mOutputMeter, output.str());
        }
        return 0;
      case WM_COMMAND:
        if (HIWORD(wParam) == CBN_SELCHANGE)
        {
          if (LOWORD(wParam) == kBackend) PopulateDevices();
          else if (LOWORD(wParam) == kInput || LOWORD(wParam) == kOutput) PopulateCapabilities();
          return 0;
        }
        if (HIWORD(wParam) == BN_CLICKED)
        {
          switch (LOWORD(wParam))
          {
            case kBack: mHost.StopTestTone(); if (mController.Back()) Render(); return 0;
            case kNext: Next(); return 0;
            case kSkip: SkipPage(); return 0;
            case kCancel: Cancel(true); return 0;
            case kAction: Action(); return 0;
            case kRefresh: RefreshDevices(); SetText(mStatus, L"Device list refreshed."); return 0;
            case kLibraryOpen:
            case kLibraryVerify:
            case kLibraryClean:
            case kLibraryFiles:
            case kLibraryFolder:
            case kLibraryZip: LibraryAction(LOWORD(wParam)); return 0;
          }
        }
        break;
      case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) { Cancel(false); return 0; }
        break;
      case WM_CLOSE: Cancel(false); return 0;
      case WM_DESTROY:
        KillTimer(mWindow, 1);
        mHost.StopTestTone();
        if (mFont) DeleteObject(mFont);
        if (mTitleFont) DeleteObject(mTitleFont);
        mWindow = nullptr;
        return 0;
    }
    return DefWindowProcW(mWindow, message, wParam, lParam);
  }

  HWND mParent{};
  HWND mWindow{};
  HWND mTitle{}, mBody{}, mStatus{}, mBack{}, mNext{}, mSkip{}, mCancel{}, mAction{}, mRefresh{};
  HWND mBackend{}, mInput{}, mOutput{}, mInputChannel{}, mOutputChannels{}, mSampleRate{}, mBufferSize{};
  HWND mInputMeter{}, mOutputMeter{};
  HWND mLibraryActions[6]{};
  HWND mAudioLabels[7]{};
  bool mAudioLabelsCreated{};
  HFONT mFont{}, mTitleFont{};
  iplug::IPlugAPPHost& mHost;
  AudioConfigurationTransaction mTransaction;
  SetupController mController;
  std::vector<AudioBackendDescriptor> mBackends;
  std::vector<const AudioDeviceDescriptor*> mInputDevices;
  std::vector<const AudioDeviceDescriptor*> mOutputDevices;
  std::future<FolderValidationResult> mFolderValidation;
  std::wstring mPendingFolder;
  bool mPendingFolderIsNam{};
  int mLibraryRefreshTicks{};
  bool mCompleted{};
};
} // namespace

bool ShowSetupWizardWin(HWND parent, iplug::IPlugAPPHost& host, WizardPage initialPage)
{
  SetupWizard wizard(parent, host);
  return wizard.Show(initialPage);
}
} // namespace amphibia::setup
