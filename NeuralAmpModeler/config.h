// Amphibia product metadata. This file is the authoritative metadata source for
// iPlug projects, resource-generation scripts, About text, and packaging.
#define PLUG_NAME "Amphibia"
#define PLUG_MFR "Amphibia Project"
#define PLUG_VERSION_HEX 0x00000100
#define PLUG_VERSION_STR "0.1.0"
#define PLUG_UNIQUE_ID 'AmPh'
#define PLUG_MFR_ID 'AmBi'
#define PLUG_URL_STR ""
#define PLUG_EMAIL_STR ""
#define PLUG_COPYRIGHT_STR "Copyright 2022 Steven Atkinson; Amphibia modifications 2026 Amphibia contributors"
#define PLUG_CLASS_NAME Amphibia
#define BUNDLE_NAME "Amphibia"
#define BUNDLE_MFR "amphibiaaudio"
#define BUNDLE_DOMAIN "org"

#define SHARED_RESOURCES_SUBPATH "Amphibia"

// Explicit VST3 identities. These UUIDv4 values are independent of the
// processor/controller IDs derived by the official NeuralAmpModeler plugin.
#define VST3_PROCESSOR_UID 0x7DBF8585, 0x2FC54817, 0xAE21F791, 0x0D0330C1
#define VST3_CONTROLLER_UID 0x893C1354, 0xA5D5416D, 0xB84CCA8A, 0xE0C27034

// Amphibia-owned metadata not consumed directly by iPlug's parse_config.py.
#define AMPHIBIA_PRODUCT_NAME PLUG_NAME
#define AMPHIBIA_PLUGIN_NAME PLUG_NAME
#define AMPHIBIA_STANDALONE_NAME BUNDLE_NAME
#define AMPHIBIA_MANUFACTURER_NAME PLUG_MFR
#define AMPHIBIA_VERSION PLUG_VERSION_STR
#define AMPHIBIA_STATE_VERSION "1.0.0" // Current path + parameter layout; independent of product SemVer.
#define AMPHIBIA_INSTALLER_NAME "Amphibia Setup"
#define AMPHIBIA_SETTINGS_NAMESPACE BUNDLE_NAME
#define AMPHIBIA_MANAGED_LIBRARY_NAMESPACE BUNDLE_NAME
#define AMPHIBIA_CACHE_NAMESPACE BUNDLE_NAME
#define AMPHIBIA_GITHUB_OWNER "amphibia-project" // Placeholder; not asserted to be an existing account.
#define AMPHIBIA_GITHUB_REPOSITORY_URL "" // Set only after a real repository exists.
#define AMPHIBIA_WEBSITE_URL "" // Set only after an owner-controlled website exists.
#define AMPHIBIA_TEMPORARY_NAMESPACE "org.amphibiaaudio"
#define AMPHIBIA_UPSTREAM_PLUGIN_VERSION "0.7.15"
#define AMPHIBIA_UPSTREAM_PLUGIN_COMMIT "96337e9"
#define AMPHIBIA_NAM_CORE_VERSION "0.5.3"
#define AMPHIBIA_NAM_CORE_COMMIT "9c7b185"
#define AMPHIBIA_IPLUG2_COMMIT "66f9060"

#ifndef AMPHIBIA_BUILD_COMMIT
  #define AMPHIBIA_BUILD_COMMIT "unknown"
#endif

#ifndef AMPHIBIA_BUILD_TYPE
  #if defined(_DEBUG) || !defined(NDEBUG)
    #define AMPHIBIA_BUILD_TYPE "Debug"
  #elif defined(TRACER_BUILD)
    #define AMPHIBIA_BUILD_TYPE "Tracer"
  #else
    #define AMPHIBIA_BUILD_TYPE "Release"
  #endif
#endif

#ifdef APP_API
  #define PLUG_CHANNEL_IO "1-2"
#else
  #define PLUG_CHANNEL_IO "1-1 1-2 2-2"
#endif

#define PLUG_LATENCY 0
#define PLUG_TYPE 0
#define PLUG_DOES_MIDI_IN 0
#define PLUG_DOES_MIDI_OUT 0
#define PLUG_DOES_MPE 0
#define PLUG_DOES_STATE_CHUNKS 0
#define PLUG_HAS_UI 1
#define PLUG_WIDTH 600
#define PLUG_HEIGHT 400
#define PLUG_FPS 60
#define PLUG_SHARED_RESOURCES 0
#define PLUG_HOST_RESIZE 0
#define PLUG_MAX_WIDTH PLUG_WIDTH * 4
#define PLUG_MAX_HEIGHT PLUG_HEIGHT * 4

#define AUV2_ENTRY Amphibia_Entry
#define AUV2_ENTRY_STR "Amphibia_Entry"
#define AUV2_FACTORY Amphibia_Factory
#define AUV2_VIEW_CLASS Amphibia_View
#define AUV2_VIEW_CLASS_STR "Amphibia_View"

#define AAX_TYPE_IDS 'AmP1'
#define AAX_TYPE_IDS_AUDIOSUITE 'AmA1'
#define AAX_PLUG_MFR_STR "Amphibia Project"
#define AAX_PLUG_NAME_STR "Amphibia\nAmPh"
#define AAX_PLUG_CATEGORY_STR "Effect"
#define AAX_DOES_AUDIOSUITE 1

#define VST3_SUBCATEGORY "Fx"

#define APP_NUM_CHANNELS 2
#define APP_N_VECTOR_WAIT 0
#define APP_MULT 1
#define APP_COPY_AUV3 0
#define APP_SIGNAL_VECTOR_SIZE 64

#define ROBOTO_FN "Roboto-Regular.ttf"
#define MICHROMA_FN "Michroma-Regular.ttf"

#define GEAR_FN "Gear.svg"
#define FILE_FN "File.svg"
#define CLOSE_BUTTON_FN "Cross.svg"
#define LEFT_ARROW_FN "ArrowLeft.svg"
#define RIGHT_ARROW_FN "ArrowRight.svg"
#define MODEL_ICON_FN "ModelIcon.svg"
#define IR_ICON_ON_FN "IRIconOn.svg"
#define IR_ICON_OFF_FN "IRIconOff.svg"
#define GLOBE_ICON_FN "Globe.svg"
#define SLIMMABLE_ICON_FN "SlimmableIcon.svg"

#define BACKGROUND_FN "Background.jpg"
#define BACKGROUND2X_FN "Background@2x.jpg"
#define BACKGROUND3X_FN "Background@3x.jpg"
#define KNOBBACKGROUND_FN "KnobBackground.png"
#define KNOBBACKGROUND2X_FN "KnobBackground@2x.png"
#define KNOBBACKGROUND3X_FN "KnobBackground@3x.png"
#define FILEBACKGROUND_FN "FileBackground.png"
#define FILEBACKGROUND2X_FN "FileBackground@2x.png"
#define FILEBACKGROUND3X_FN "FileBackground@3x.png"
#define INPUTLEVELBACKGROUND_FN "InputLevelBackground.png"
#define INPUTLEVELBACKGROUND2X_FN "InputLevelBackground@2x.png"
#define INPUTLEVELBACKGROUND3X_FN "InputLevelBackground@3x.png"
#define LINES_FN "Lines.png"
#define LINES2X_FN "Lines@2x.png"
#define LINES3X_FN "Lines@3x.png"
#define SLIDESWITCHHANDLE_FN "SlideSwitchHandle.png"
#define SLIDESWITCHHANDLE2X_FN "SlideSwitchHandle@2x.png"
#define SLIDESWITCHHANDLE3X_FN "SlideSwitchHandle@3x.png"

#define METERBACKGROUND_FN "MeterBackground.png"
#define METERBACKGROUND2X_FN "MeterBackground@2x.png"
#define METERBACKGROUND3X_FN "MeterBackground@3x.png"

// Issue 291
// On the macOS standalone, we might not have permissions to traverse the file directory, so we have the app ask the
// user to pick a directory instead of the file in the directory.
// Everyone else is fine though.
#if defined(APP_API) && defined(__APPLE__)
  #define NAM_PICK_DIRECTORY
#endif
