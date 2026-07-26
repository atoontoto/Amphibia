/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#include <memory>
#include <filesystem>
#include <string>
#include <vector>
#include "wdltypes.h"
#include "wdlstring.h"

#include "IPlugPlatform.h"
#include "AmphibiaAPPHost.h"
#include "SetupWizardWin.h"

#include "../CommandLine.h"
#include "../SetupController.h"

#include "config.h"
#include "resource.h"

using namespace iplug;

#pragma mark - WINDOWS
#if defined OS_WIN
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>

extern WDL_DLGRET MainDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

HWND gHWND;
extern HINSTANCE gHINSTANCE;
UINT gScrollMessage;
bool gAmphibiaSafeMode = false;
HANDLE gAmphibiaInstanceMutex = nullptr;

namespace
{
std::string Utf8Argument(const wchar_t* value)
{
  if (!value || !*value) return {};
  const auto length = static_cast<int>(wcslen(value));
  const int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, length, nullptr, 0, nullptr, nullptr);
  if (count <= 0) return {};
  std::string result(static_cast<std::size_t>(count), '\0');
  WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, length, result.data(), count, nullptr, nullptr);
  return result;
}

std::wstring WideMessage(const std::string& value)
{
  if (value.empty()) return {};
  const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
  if (count <= 0) return L"Invalid command-line text.";
  std::wstring result(static_cast<std::size_t>(count), L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), count);
  return result;
}

amphibia::setup::CommandLineOptions ReadOptions()
{
  int count{};
  auto** values = CommandLineToArgvW(GetCommandLineW(), &count);
  std::vector<std::string> arguments;
  if (values)
  {
    for (int index = 1; index < count; ++index) arguments.push_back(Utf8Argument(values[index]));
    LocalFree(values);
  }
  return amphibia::setup::ParseCommandLine(arguments);
}

bool LegacySettingsExist()
{
  wchar_t local[MAX_PATH]{};
  if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, local))) return false;
  std::error_code error;
  return std::filesystem::is_regular_file(std::filesystem::path(local) / L"Amphibia" / L"settings.ini", error) && !error;
}
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdParam, int nShowCmd)
{
  try
  {
    const auto options = ReadOptions();
    if (options.showHelp)
    {
      MessageBoxW(nullptr, WideMessage(amphibia::setup::CommandLineHelp()).c_str(), L"Amphibia Help", MB_OK | MB_ICONINFORMATION);
      return 0;
    }
    if (options.showVersion)
    {
      MessageBoxW(nullptr, WideMessage(amphibia::setup::VersionText(AMPHIBIA_VERSION, AMPHIBIA_BUILD_COMMIT)).c_str(),
                  L"Amphibia Version", MB_OK | MB_ICONINFORMATION);
      return 0;
    }
    if (!options.unknownOptions.empty())
    {
      const auto message = std::string("Unknown option: ") + options.unknownOptions.front() + "\n\n" +
                           amphibia::setup::CommandLineHelp();
      MessageBoxW(nullptr, WideMessage(message).c_str(), L"Amphibia", MB_OK | MB_ICONERROR);
      return 2;
    }
    gAmphibiaSafeMode = options.safeMode;
    const bool legacySettingsExisted = LegacySettingsExist();
#ifndef APP_ALLOW_MULTIPLE_INSTANCES
    HANDLE hMutex = OpenMutex(MUTEX_ALL_ACCESS, 0, BUNDLE_NAME); // BUNDLE_NAME used because it won't have spaces in it

    if (!hMutex)
      hMutex = CreateMutex(0, 0, BUNDLE_NAME);
    else
    {
      HWND hWnd = FindWindow(0, BUNDLE_NAME);
      SetForegroundWindow(hWnd);
      return 0;
    }
    gAmphibiaInstanceMutex = hMutex;
#endif
    gHINSTANCE = hInstance;

    InitCommonControls();
    const auto comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    gScrollMessage = RegisterWindowMessage("MSWHEEL_ROLLMSG");

    IPlugAPPHost* pAppHost = IPlugAPPHost::Create();
    if (!pAppHost->Init())
    {
      MessageBoxW(nullptr, L"Amphibia could not initialize its application state.", L"Amphibia", MB_OK | MB_ICONERROR);
      return 1;
    }

    const auto settings = pAppHost->SetupSettings().Load();
    const bool freshInstall = settings.status == amphibia::setup::SettingsLoadStatus::Missing && !legacySettingsExisted;
    const bool upgradeWithoutSetupState = settings.status == amphibia::setup::SettingsLoadStatus::Missing && legacySettingsExisted;
    const bool existingAudioOpened = !freshInstall && pAppHost->TryToChangeAudio();

    HACCEL hAccel = LoadAccelerators(gHINSTANCE, MAKEINTRESOURCE(IDR_ACCELERATOR1));

    static UINT(WINAPI *__SetProcessDpiAwarenessContext)(DPI_AWARENESS_CONTEXT);

    double scale = 1.;

    if (!__SetProcessDpiAwarenessContext)
    {
      HINSTANCE h = LoadLibrary("user32.dll");
      if (h) *(void **)&__SetProcessDpiAwarenessContext = GetProcAddress(h, "SetProcessDpiAwarenessContext");
      if (!__SetProcessDpiAwarenessContext)
        *(void **)&__SetProcessDpiAwarenessContext = (void*)(INT_PTR)1;
    }
    if ((UINT_PTR)__SetProcessDpiAwarenessContext > (UINT_PTR)1)
    {
      __SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    }

    CreateDialog(gHINSTANCE, MAKEINTRESOURCE(IDD_DIALOG_MAIN), GetDesktopWindow(), IPlugAPPHost::MainDlgProc);

    bool showWizard = options.forceFirstRun || options.openSetup || options.openAudioSettings || freshInstall;
    if (!showWizard && upgradeWithoutSetupState)
      showWizard = MessageBoxW(gHWND,
                              L"Amphibia has a new guided setup for audio, local folders, and optional services.\n\nRun it now?",
                              L"Amphibia Setup Available", MB_YESNO | MB_ICONINFORMATION) == IDYES;
    if (!showWizard && settings.status == amphibia::setup::SettingsLoadStatus::Loaded &&
        settings.document.standalone.setup.completion == amphibia::setup::SetupCompletionState::InProgress)
      showWizard = MessageBoxW(gHWND, L"Setup was not completed last time. Resume it now?", L"Resume Amphibia Setup",
                              MB_YESNO | MB_ICONQUESTION) == IDYES;
    if (!showWizard && settings.status == amphibia::setup::SettingsLoadStatus::Loaded &&
        settings.document.standalone.setup.audioConfigured && !existingAudioOpened)
      showWizard = MessageBoxW(gHWND,
                              L"The saved audio configuration is unavailable. Open Audio Settings to repair it?",
                              L"Amphibia Audio", MB_YESNO | MB_ICONWARNING) == IDYES;
    if (showWizard)
      amphibia::setup::ShowSetupWizardWin(gHWND, *pAppHost,
        options.openAudioSettings ? amphibia::setup::WizardPage::AudioDevice : amphibia::setup::WizardPage::Welcome);

#if !defined _DEBUG || defined NO_IGRAPHICS
    HMENU menu = GetMenu(gHWND);
    RemoveMenu(menu, 1, MF_BYPOSITION);
    DrawMenuBar(gHWND);
#endif

    for(;;)
    {
      MSG msg= {0,};
      int vvv = GetMessage(&msg, NULL, 0, 0);

      if (!vvv)
        break;

      if (vvv < 0)
      {
        Sleep(10);
        continue;
      }

      if (!msg.hwnd)
      {
        DispatchMessage(&msg);
        continue;
      }

      if (gHWND && (TranslateAccelerator(gHWND, hAccel, &msg) || IsDialogMessage(gHWND, &msg)))
        continue;

      // default processing for other dialogs
      HWND hWndParent = NULL;
      HWND temphwnd = msg.hwnd;

      do
      {
        if (GetClassLong(temphwnd, GCW_ATOM) == (INT)32770)
        {
          hWndParent = temphwnd;
          if (!(GetWindowLong(temphwnd, GWL_STYLE) & WS_CHILD))
            break; // not a child, exit
        }
      }
      while (temphwnd = GetParent(temphwnd));

      if (hWndParent && IsDialogMessage(hWndParent,&msg))
        continue;

      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    // in case gHWND didnt get destroyed -- this corresponds to SWELLAPP_DESTROY roughly
    if (gHWND)
      DestroyWindow(gHWND);

    if (SUCCEEDED(comResult)) CoUninitialize();

#ifndef APP_ALLOW_MULTIPLE_INSTANCES
    if (gAmphibiaInstanceMutex)
    {
      ReleaseMutex(gAmphibiaInstanceMutex);
      CloseHandle(gAmphibiaInstanceMutex);
      gAmphibiaInstanceMutex = nullptr;
    }
#endif
  }
  catch(std::exception e)
  {
    DBGMSG("Exception: %s", e.what());
    return 1;
  }
  return 0;
}
#pragma mark - MAC
#elif defined(OS_MAC)
#import <Cocoa/Cocoa.h>
#include "IPlugSWELL.h"
#include "IPlugPaths.h"

HWND gHWND;
extern HMENU SWELL_app_stocksysmenu;

int main(int argc, char *argv[])
{
#if APP_COPY_AUV3
  //if invoked with an argument registerauv3 use plug-in kit to explicitly register auv3 app extension (doesn't happen from debugger)
  if(strcmp(argv[2], "registerauv3"))
  {
    WDL_String appexPath;
    appexPath.SetFormatted(1024, "pluginkit -a %s%s%s.appex", argv[0], "/../../Plugins/", appexPath.get_filepart());
    if(system(appexPath.Get()) > -1)
      NSLog(@"Registered audiounit app extension\n");
    else
      NSLog(@"Failed to register audiounit app extension\n");
  }
#endif

  if(AppIsSandboxed())
    DBGMSG("App is sandboxed, file system access etc restricted!\n");

  return NSApplicationMain(argc,  (const char **) argv);
}

INT_PTR SWELLAppMain(int msg, INT_PTR parm1, INT_PTR parm2)
{
  IPlugAPPHost* pAppHost = nullptr;

  switch (msg)
  {
    case SWELLAPP_ONLOAD:
      pAppHost = IPlugAPPHost::Create();
      pAppHost->Init();
      pAppHost->TryToChangeAudio();
      break;
    case SWELLAPP_LOADED:
    {
      pAppHost = IPlugAPPHost::sInstance.get();

      HMENU menu = SWELL_GetCurrentMenu();

      if (menu)
      {
        // work on a new menu
        menu = SWELL_DuplicateMenu(menu);
        HMENU src = LoadMenu(NULL, MAKEINTRESOURCE(IDR_MENU1));

        for (int x = 0; x < GetMenuItemCount(src)-1; x++)
        {
          HMENU sm = GetSubMenu(src,x);

          if (sm)
          {
            char str[1024];
            MENUITEMINFO mii = {sizeof(mii), MIIM_TYPE};
            mii.dwTypeData = str;
            mii.cch = sizeof(str);
            str[0] = 0;
            GetMenuItemInfo(src, x, TRUE, &mii);
            MENUITEMINFO mi= {sizeof(mi), MIIM_STATE|MIIM_SUBMENU|MIIM_TYPE,MFT_STRING, 0, 0, SWELL_DuplicateMenu(sm), NULL, NULL, 0, str};
            InsertMenuItem(menu, x+1, TRUE, &mi);
          }
        }
      }

      if (menu)
      {
        HMENU sm = GetSubMenu(menu, 1);
        DeleteMenu(sm, ID_QUIT, MF_BYCOMMAND); // remove QUIT from our file menu, since it is in the system menu on OSX
        DeleteMenu(sm, ID_PREFERENCES, MF_BYCOMMAND); // remove PREFERENCES from the file menu, since it is in the system menu on OSX

        // remove any trailing separators
        int a = GetMenuItemCount(sm);

        while (a > 0 && GetMenuItemID(sm, a-1) == 0)
          DeleteMenu(sm, --a, MF_BYPOSITION);

        DeleteMenu(menu, 1, MF_BYPOSITION); // delete file menu
      }
#if !defined _DEBUG || defined NO_IGRAPHICS
      if (menu)
      {
        HMENU sm = GetSubMenu(menu, 1);
        DeleteMenu(sm, ID_LIVE_EDIT, MF_BYCOMMAND);
        DeleteMenu(sm, ID_SHOW_DRAWN, MF_BYCOMMAND);
        DeleteMenu(sm, ID_SHOW_FPS, MF_BYCOMMAND);

        // remove any trailing separators
        int a = GetMenuItemCount(sm);

        while (a > 0 && GetMenuItemID(sm, a-1) == 0)
          DeleteMenu(sm, --a, MF_BYPOSITION);

        DeleteMenu(menu, 1, MF_BYPOSITION); // delete debug menu
      }
#else
      SetMenuItemModifier(menu, ID_LIVE_EDIT, MF_BYCOMMAND, 'E', FCONTROL);
      SetMenuItemModifier(menu, ID_SHOW_DRAWN, MF_BYCOMMAND, 'D', FCONTROL);
      SetMenuItemModifier(menu, ID_SHOW_BOUNDS, MF_BYCOMMAND, 'B', FCONTROL);
      SetMenuItemModifier(menu, ID_SHOW_FPS, MF_BYCOMMAND, 'F', FCONTROL);
#endif

      HWND hwnd = CreateDialog(gHINST, MAKEINTRESOURCE(IDD_DIALOG_MAIN), NULL, IPlugAPPHost::MainDlgProc);

      if (menu)
      {
        SetMenu(hwnd, menu); // set the menu for the dialog to our menu (on Windows that menu is set from the .rc, but on SWELL
        SWELL_SetDefaultModalWindowMenu(menu); // other windows will get the stock (bundle) menus
      }

      break;
    }
    case SWELLAPP_ONCOMMAND:
      // this is to catch commands coming from the system menu etc
      if (gHWND && (parm1&0xffff))
        SendMessage(gHWND, WM_COMMAND, parm1 & 0xffff, 0);
      break;
    case SWELLAPP_DESTROY:
      if (gHWND)
        DestroyWindow(gHWND);
      break;
    case SWELLAPP_PROCESSMESSAGE:
      MSG* pMSG = (MSG*) parm1;
      NSView* pContentView = (NSView*) pMSG->hwnd;
      NSEvent* pEvent = (NSEvent*) parm2;
      int etype = (int) [pEvent type];

      bool textField = [pContentView isKindOfClass:[NSText class]];

      if (!textField && etype == NSKeyDown)
      {
        int flag, code = SWELL_MacKeyToWindowsKey(pEvent, &flag);

        if (!(flag&~FVIRTKEY) && (code == VK_RETURN || code == VK_ESCAPE))
        {
          [pContentView keyDown: pEvent];
          return 1;
        }
      }
      break;
  }
  return 0;
}

#define CBS_HASSTRINGS 0
#define SWELL_DLG_SCALE_AUTOGEN 1
#define SET_IDD_DIALOG_PREF_SCALE 1.5
#if PLUG_HOST_RESIZE
#define SWELL_DLG_FLAGS_AUTOGEN SWELL_DLG_WS_FLIPPED|SWELL_DLG_WS_RESIZABLE
#endif
#include "swell-dlggen.h"
#include "resources/main.rc_mac_dlg"
#include "swell-menugen.h"
#include "resources/main.rc_mac_menu"

#pragma mark - LINUX
#elif defined(OS_LINUX)
//#include <IPlugSWELL.h>
//#include "swell-internal.h" // fixes problem with HWND forward decl
//
//HWND gHWND;
//UINT gScrollMessage;
//extern HMENU SWELL_app_stocksysmenu;
//
//int main(int argc, char **argv)
//{
//  SWELL_initargs(&argc, &argv);
//  SWELL_Internal_PostMessage_Init();
//  SWELL_ExtendedAPI("APPNAME", (void*) "IGraphics Test");
//
//  HMENU menu = LoadMenu(NULL, MAKEINTRESOURCE(IDR_MENU1));
//  CreateDialog(gHINSTANCE, MAKEINTRESOURCE(IDD_DIALOG_MAIN), NULL, MainDlgProc);
//  SetMenu(gHWND, menu);
//
//  while (!gHWND->m_hashaddestroy)
//  {
//    SWELL_RunMessageLoop();
//    Sleep(10);
//  };
//
//  if (gHWND)
//    DestroyWindow(gHWND);
//
//  return 0;
//}
//
//INT_PTR SWELLAppMain(int msg, INT_PTR parm1, INT_PTR parm2)
//{
//  switch (msg)
//  {
//    case SWELLAPP_ONLOAD:
//      break;
//    case SWELLAPP_LOADED:
//    {
//      HMENU menu = SWELL_GetCurrentMenu();
//
//      if (menu)
//      {
//        // work on a new menu
//        menu = SWELL_DuplicateMenu(menu);
//        HMENU src = LoadMenu(NULL, MAKEINTRESOURCE(IDR_MENU1));
//
//        for (auto x = 0; x < GetMenuItemCount(src)-1; x++)
//        {
//          HMENU sm = GetSubMenu(src,x);
//          if (sm)
//          {
//            char str[1024];
//            MENUITEMINFO mii = {sizeof(mii), MIIM_TYPE};
//            mii.dwTypeData = str;
//            mii.cch = sizeof(str);
//            str[0] = 0;
//            GetMenuItemInfo(src, x, TRUE, &mii);
//            MENUITEMINFO mi= {sizeof(mi), MIIM_STATE|MIIM_SUBMENU|MIIM_TYPE,MFT_STRING, 0, 0, SWELL_DuplicateMenu(sm), NULL, NULL, 0, str};
//            InsertMenuItem(menu, x+1, TRUE, &mi);
//          }
//        }
//      }
//
//      if (menu)
//      {
//        HMENU sm = GetSubMenu(menu, 1);
//        DeleteMenu(sm, ID_QUIT, MF_BYCOMMAND); // remove QUIT from our file menu, since it is in the system menu on OSX
//        DeleteMenu(sm, ID_PREFERENCES, MF_BYCOMMAND); // remove PREFERENCES from the file menu, since it is in the system menu on OSX
//
//        // remove any trailing separators
//        int a = GetMenuItemCount(sm);
//
//        while (a > 0 && GetMenuItemID(sm, a-1) == 0)
//          DeleteMenu(sm, --a, MF_BYPOSITION);
//
//        DeleteMenu(menu, 1, MF_BYPOSITION); // delete file menu
//      }
//
//      // if we want to set any default modifiers for items in the menus, we can use:
//      // SetMenuItemModifier(menu,commandID,MF_BYCOMMAND,'A',FCONTROL) etc.
//
//      HWND hwnd = CreateDialog(gHINST,MAKEINTRESOURCE(IDD_DIALOG_MAIN), NULL, MainDlgProc);
//
//      if (menu)
//      {
//        SetMenu(hwnd, menu); // set the menu for the dialog to our menu (on Windows that menu is set from the .rc, but on SWELL
//        SWELL_SetDefaultModalWindowMenu(menu); // other windows will get the stock (bundle) menus
//      }
//
//      break;
//    }
//    case SWELLAPP_ONCOMMAND:
//      // this is to catch commands coming from the system menu etc
//      if (gHWND && (parm1&0xffff))
//        SendMessage(gHWND, WM_COMMAND, parm1 & 0xffff, 0);
//      break;
//    case SWELLAPP_DESTROY:
//      if (gHWND)
//        DestroyWindow(gHWND);
//      break;
//    case SWELLAPP_PROCESSMESSAGE: // can hook keyboard input here
//      // parm1 = (MSG*), should we want it -- look in swell.h to see what the return values refer to
//      break;
//  }
//  return 0;
//}
//
//#define CBS_HASSTRINGS 0
//#define SWELL_DLG_SCALE_AUTOGEN 1
//#define SET_IDD_DIALOG_PREF_SCALE 1.5
//#include "swell-dlggen.h"
//#include "resources/main.rc_mac_dlg"
//#include "swell-menugen.h"
//#include "resources/main.rc_mac_menu"
#endif
