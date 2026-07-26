/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#include "AmphibiaAPPHost.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <sstream>
#include <thread>

#ifdef OS_WIN
#include <sys/stat.h>
#endif

#include "IPlugLogger.h"

using namespace iplug;

#ifndef MAX_PATH_LEN
#define MAX_PATH_LEN 2048
#endif

#define STRBUFSZ 100

namespace
{
constexpr std::uint32_t kNoInputDevice = std::numeric_limits<std::uint32_t>::max();
uint32_t ClampFirstChannel(uint32_t firstChannel, uint32_t availableChannels, uint32_t streamChannels)
{
  if (availableChannels == 0 || streamChannels == 0)
    return 0;

  const uint32_t maxFirstChannel = availableChannels > streamChannels ? availableChannels - streamChannels + 1 : 1;
  return std::min(std::max(firstChannel, uint32_t(1)), maxFirstChannel);
}

std::optional<std::uint32_t> DeviceIndex(const std::string& id, const std::string& backend)
{
  const auto prefix = backend + ':';
  if (id.rfind(prefix, 0) != 0 || id.size() > prefix.size() + 10) return std::nullopt;
  try
  {
    std::size_t consumed{};
    const auto value = std::stoul(id.substr(prefix.size()), &consumed);
    if (consumed != id.size() - prefix.size()) return std::nullopt;
    return static_cast<std::uint32_t>(value);
  }
  catch (...) { return std::nullopt; }
}

RtAudio::Api BackendApi(const std::string& backend)
{
  return backend == "asio" ? RtAudio::WINDOWS_ASIO : RtAudio::WINDOWS_DS;
}
}

std::unique_ptr<IPlugAPPHost> IPlugAPPHost::sInstance;
UINT gSCROLLMSG;

IPlugAPPHost::IPlugAPPHost()
: mIPlug(MakePlug(InstanceInfo{this}))
{
}

IPlugAPPHost::~IPlugAPPHost()
{
  mExiting = true;

  CloseAudio();

  if(mMidiIn)
    mMidiIn->cancelCallback();

  if(mMidiOut)
    mMidiOut->closePort();
}

//static
IPlugAPPHost* IPlugAPPHost::Create()
{
  sInstance = std::make_unique<IPlugAPPHost>();
  return sInstance.get();
}

bool IPlugAPPHost::Init()
{
  mIPlug->SetHost("standalone", mIPlug->GetPluginVersion(false));

  if (!InitState())
    return false;

  TryToChangeAudioDriverType(); // will init RTAudio with an API type based on gState->mAudioDriverType
  ProbeAudioIO(); // find out what audio IO devs are available and put their IDs in the global variables gAudioInputDevs / gAudioOutputDevs
  InitMidi(); // creates RTMidiIn and RTMidiOut objects
  ProbeMidiIO(); // find out what midi IO devs are available and put their names in the global variables gMidiInputDevs / gMidiOutputDevs
  SelectMIDIDevice(ERoute::kInput, mState.mMidiInDev.Get());
  SelectMIDIDevice(ERoute::kOutput, mState.mMidiOutDev.Get());

  mIPlug->OnParamReset(kReset);
  mIPlug->OnActivate(true);

  return true;
}

bool IPlugAPPHost::OpenWindow(HWND pParent)
{
  return mIPlug->OpenWindow(pParent) != nullptr;
}

void IPlugAPPHost::CloseWindow()
{
  mIPlug->CloseWindow();
}

bool IPlugAPPHost::InitState()
{
#if defined OS_WIN
  TCHAR strPath[MAX_PATH_LEN];
  SHGetFolderPathA( NULL, CSIDL_LOCAL_APPDATA, NULL, 0, strPath );
  mINIPath.SetFormatted(MAX_PATH_LEN, "%s\\%s\\", strPath, BUNDLE_NAME);
#elif defined OS_MAC
  mINIPath.SetFormatted(MAX_PATH_LEN, "%s/Library/Application Support/%s/", getenv("HOME"), BUNDLE_NAME);
#else
  #error NOT IMPLEMENTED
#endif

  struct stat st;

  if(stat(mINIPath.Get(), &st) == 0) // if directory exists
  {
    mINIPath.Append("settings.ini"); // add file name to path

    char buf[STRBUFSZ];

    if(stat(mINIPath.Get(), &st) == 0) // if settings file exists read values into state
    {
      DBGMSG("Reading ini file from %s\n", mINIPath.Get());

      mState.mAudioDriverType = GetPrivateProfileInt("audio", "driver", 0, mINIPath.Get());

      GetPrivateProfileString("audio", "indev", "Built-in Input", buf, STRBUFSZ, mINIPath.Get()); mState.mAudioInDev.Set(buf);
      GetPrivateProfileString("audio", "outdev", "Built-in Output", buf, STRBUFSZ, mINIPath.Get()); mState.mAudioOutDev.Set(buf);

      //audio
      mState.mAudioInChanL = GetPrivateProfileInt("audio", "in1", 1, mINIPath.Get()); // 1 is first audio input
      mState.mAudioInChanR = GetPrivateProfileInt("audio", "in2", 2, mINIPath.Get());
      mState.mAudioOutChanL = GetPrivateProfileInt("audio", "out1", 1, mINIPath.Get()); // 1 is first audio output
      mState.mAudioOutChanR = GetPrivateProfileInt("audio", "out2", 2, mINIPath.Get());
      //mState.mAudioInIsMono = GetPrivateProfileInt("audio", "monoinput", 0, mINIPath.Get());

      mState.mBufferSize = GetPrivateProfileInt("audio", "buffer", 512, mINIPath.Get());
      mState.mAudioSR = GetPrivateProfileInt("audio", "sr", 44100, mINIPath.Get());

      //midi
      GetPrivateProfileString("midi", "indev", "no input", buf, STRBUFSZ, mINIPath.Get()); mState.mMidiInDev.Set(buf);
      GetPrivateProfileString("midi", "outdev", "no output", buf, STRBUFSZ, mINIPath.Get()); mState.mMidiOutDev.Set(buf);

      mState.mMidiInChan = GetPrivateProfileInt("midi", "inchan", 0, mINIPath.Get()); // 0 is any
      mState.mMidiOutChan = GetPrivateProfileInt("midi", "outchan", 0, mINIPath.Get()); // 1 is first chan
    }

    // if settings file doesn't exist, populate with default values, otherwise overrwrite
    UpdateINI();
  }
  else   // folder doesn't exist - make folder and make file
  {
#if defined OS_WIN
    // folder doesn't exist - make folder and make file
    CreateDirectory(mINIPath.Get(), NULL);
    mINIPath.Append("settings.ini");
    UpdateINI(); // will write file if doesn't exist
#elif defined OS_MAC
    mode_t process_mask = umask(0);
    int result_code = mkdir(mINIPath.Get(), S_IRWXU | S_IRWXG | S_IRWXO);
    umask(process_mask);

    if(!result_code)
    {
      mINIPath.Append("\\settings.ini");
      UpdateINI(); // will write file if doesn't exist
    }
    else
    {
      return false;
    }
#else
  #error NOT IMPLEMENTED
#endif
  }

  const auto settingsPath = std::filesystem::u8path(mINIPath.Get()).parent_path() / "settings-v1.json";
  mSetupSettings = std::make_unique<amphibia::setup::SettingsStore>(settingsPath);
  return true;
}

void IPlugAPPHost::UpdateINI()
{
  char buf[STRBUFSZ]; // temp buffer for writing integers to profile strings
  const char* ini = mINIPath.Get();

  sprintf(buf, "%u", mState.mAudioDriverType);
  WritePrivateProfileString("audio", "driver", buf, ini);

  WritePrivateProfileString("audio", "indev", mState.mAudioInDev.Get(), ini);
  WritePrivateProfileString("audio", "outdev", mState.mAudioOutDev.Get(), ini);

  sprintf(buf, "%u", mState.mAudioInChanL);
  WritePrivateProfileString("audio", "in1", buf, ini);
  sprintf(buf, "%u", mState.mAudioInChanR);
  WritePrivateProfileString("audio", "in2", buf, ini);
  sprintf(buf, "%u", mState.mAudioOutChanL);
  WritePrivateProfileString("audio", "out1", buf, ini);
  sprintf(buf, "%u", mState.mAudioOutChanR);
  WritePrivateProfileString("audio", "out2", buf, ini);
  //sprintf(buf, "%u", mState.mAudioInIsMono);
  //WritePrivateProfileString("audio", "monoinput", buf, ini);

  WDL_String str;
  str.SetFormatted(32, "%i", mState.mBufferSize);
  WritePrivateProfileString("audio", "buffer", str.Get(), ini);

  str.SetFormatted(32, "%i", mState.mAudioSR);
  WritePrivateProfileString("audio", "sr", str.Get(), ini);

  WritePrivateProfileString("midi", "indev", mState.mMidiInDev.Get(), ini);
  WritePrivateProfileString("midi", "outdev", mState.mMidiOutDev.Get(), ini);

  sprintf(buf, "%u", mState.mMidiInChan);
  WritePrivateProfileString("midi", "inchan", buf, ini);
  sprintf(buf, "%u", mState.mMidiOutChan);
  WritePrivateProfileString("midi", "outchan", buf, ini);
}

std::string IPlugAPPHost::GetAudioDeviceName(int idx) const
{
  return mAudioIDDevNames.at(idx);
}

int IPlugAPPHost::GetAudioDeviceIdx(const char* deviceNameToTest) const
{
  for(int i = 0; i < mAudioIDDevNames.size(); i++)
  {
    if(!strcmp(deviceNameToTest, mAudioIDDevNames.at(i).c_str() ))
      return i;
  }

  return -1;
}

int IPlugAPPHost::GetMIDIPortNumber(ERoute direction, const char* nameToTest) const
{
  int start = 1;

  if(direction == ERoute::kInput)
  {
    if(!strcmp(nameToTest, OFF_TEXT)) return 0;

  #ifdef OS_MAC
    start = 2;
    if(!strcmp(nameToTest, "virtual input")) return 1;
  #endif

    for (int i = 0; i < mMidiIn->getPortCount(); i++)
    {
      if(!strcmp(nameToTest, mMidiIn->getPortName(i).c_str()))
        return (i + start);
    }
  }
  else
  {
    if(!strcmp(nameToTest, OFF_TEXT)) return 0;

  #ifdef OS_MAC
    start = 2;
    if(!strcmp(nameToTest, "virtual output")) return 1;
  #endif

    for (int i = 0; i < mMidiOut->getPortCount(); i++)
    {
      if(!strcmp(nameToTest, mMidiOut->getPortName(i).c_str()))
        return (i + start);
    }
  }

  return -1;
}

void IPlugAPPHost::ProbeAudioIO()
{
  std::cout << "\nRtAudio Version " << RtAudio::getVersion() << std::endl;

  RtAudio::DeviceInfo info;

  mAudioInputDevs.clear();
  mAudioOutputDevs.clear();
  mAudioIDDevNames.clear();

  uint32_t nDevices = mDAC->getDeviceCount();

  for (int i=0; i<nDevices; i++)
  {
    info = mDAC->getDeviceInfo(i);
    std::string deviceName = info.name;

#ifdef OS_MAC
    size_t colonIdx = deviceName.rfind(": ");

    if(colonIdx != std::string::npos && deviceName.length() >= 2)
      deviceName = deviceName.substr(colonIdx + 2, deviceName.length() - colonIdx - 2);

#endif

    mAudioIDDevNames.push_back(deviceName);

    if ( info.probed == false )
      std::cout << deviceName << ": Probe Status = Unsuccessful\n";
    else if ( !strcmp("Generic Low Latency ASIO Driver", deviceName.c_str() ))
      std::cout << deviceName << ": Probe Status = Unsuccessful\n";
    else
    {
      if(info.inputChannels > 0)
        mAudioInputDevs.push_back(i);

      if(info.outputChannels > 0)
        mAudioOutputDevs.push_back(i);

      if (info.isDefaultInput)
        mDefaultInputDev = i;

      if (info.isDefaultOutput)
        mDefaultOutputDev = i;
    }
  }
}

void IPlugAPPHost::ProbeMidiIO()
{
  if ( !mMidiIn || !mMidiOut )
    return;
  else
  {
    int nInputPorts = mMidiIn->getPortCount();

    mMidiInputDevNames.push_back(OFF_TEXT);

#ifdef OS_MAC
    mMidiInputDevNames.push_back("virtual input");
#endif

    for (int i=0; i<nInputPorts; i++ )
    {
      mMidiInputDevNames.push_back(mMidiIn->getPortName(i));
    }

    int nOutputPorts = mMidiOut->getPortCount();

    mMidiOutputDevNames.push_back(OFF_TEXT);

#ifdef OS_MAC
    mMidiOutputDevNames.push_back("virtual output");
#endif

    for (int i=0; i<nOutputPorts; i++ )
    {
      mMidiOutputDevNames.push_back(mMidiOut->getPortName(i));
      //This means the virtual output port wont be added as an input
    }
  }
}

bool IPlugAPPHost::AudioSettingsInStateAreEqual(AppState& os, AppState& ns)
{
  if (os.mAudioDriverType != ns.mAudioDriverType) return false;
  if (strcmp(os.mAudioInDev.Get(), ns.mAudioInDev.Get())) return false;
  if (strcmp(os.mAudioOutDev.Get(), ns.mAudioOutDev.Get())) return false;
  if (os.mAudioSR != ns.mAudioSR) return false;
  if (os.mBufferSize != ns.mBufferSize) return false;
  if (os.mAudioInChanL != ns.mAudioInChanL) return false;
  if (os.mAudioInChanR != ns.mAudioInChanR) return false;
  if (os.mAudioOutChanL != ns.mAudioOutChanL) return false;
  if (os.mAudioOutChanR != ns.mAudioOutChanR) return false;
//  if (os.mAudioInIsMono != ns.mAudioInIsMono) return false;

  return true;
}

bool IPlugAPPHost::MIDISettingsInStateAreEqual(AppState& os, AppState& ns)
{
  if (strcmp(os.mMidiInDev.Get(), ns.mMidiInDev.Get())) return false;
  if (strcmp(os.mMidiOutDev.Get(), ns.mMidiOutDev.Get())) return false;
  if (os.mMidiInChan != ns.mMidiInChan) return false;
  if (os.mMidiOutChan != ns.mMidiOutChan) return false;

  return true;
}

bool IPlugAPPHost::TryToChangeAudioDriverType()
{
  CloseAudio();

  if (mDAC)
  {
    mDAC = nullptr;
  }

#if defined OS_WIN
  if(mState.mAudioDriverType == kDeviceASIO)
    mDAC = std::make_unique<RtAudio>(RtAudio::WINDOWS_ASIO);
  else
    mDAC = std::make_unique<RtAudio>(RtAudio::WINDOWS_DS);
#elif defined OS_MAC
  if(mState.mAudioDriverType == kDeviceCoreAudio)
    mDAC = std::make_unique<RtAudio>(RtAudio::MACOSX_CORE);
  //else
  //mDAC = std::make_unique<RtAudio>(RtAudio::UNIX_JACK);
#else
  #error NOT IMPLEMENTED
#endif

  if(mDAC)
    return true;
  else
    return false;
}

bool IPlugAPPHost::TryToChangeAudio()
{
  int inputID = -1;
  int outputID = -1;

#if defined OS_WIN
  if(mState.mAudioDriverType == kDeviceASIO)
    inputID = GetAudioDeviceIdx(mState.mAudioOutDev.Get());
  else
    inputID = GetAudioDeviceIdx(mState.mAudioInDev.Get());
#elif defined OS_MAC
  inputID = GetAudioDeviceIdx(mState.mAudioInDev.Get());
#else
  #error NOT IMPLEMENTED
#endif
  outputID = GetAudioDeviceIdx(mState.mAudioOutDev.Get());

  bool failedToFindDevice = false;
  if (!strcmp(mState.mAudioInDev.Get(), "No input"))
    inputID = static_cast<int>(kNoInputDevice);
  else if (inputID == -1)
  {
    failedToFindDevice = true;
  }

  if (outputID == -1)
  {
    failedToFindDevice = true;
  }

  if (failedToFindDevice)
  {
    mLastAudioError = "Previously selected audio device is unavailable.";
    return false;
  }

  if (inputID != -1 && outputID != -1)
  {
    return InitAudio(inputID, outputID, mState.mAudioSR, mState.mBufferSize);
  }

  return false;
}

bool IPlugAPPHost::SelectMIDIDevice(ERoute direction, const char* pPortName)
{
  int port = GetMIDIPortNumber(direction, pPortName);

  if(direction == ERoute::kInput)
  {
    if(port == -1)
    {
      mState.mMidiInDev.Set(OFF_TEXT);
      UpdateINI();
      port = 0;
    }

    //TODO: send all notes off?
    if (mMidiIn)
    {
      mMidiIn->closePort();

      if (port == 0)
      {
        return true;
      }
  #if defined OS_WIN
      else
      {
        mMidiIn->openPort(port-1);
        return true;
      }
  #elif defined OS_MAC
      else if(port == 1)
      {
        std::string virtualMidiInputName = "To ";
        virtualMidiInputName += BUNDLE_NAME;
        mMidiIn->openVirtualPort(virtualMidiInputName);
        return true;
      }
      else
      {
        mMidiIn->openPort(port-2);
        return true;
      }
  #else
   #error NOT IMPLEMENTED
  #endif
    }
  }
  else
  {
    if(port == -1)
    {
      mState.mMidiOutDev.Set(OFF_TEXT);
      UpdateINI();
      port = 0;
    }

    if (mMidiOut)
    {
      //TODO: send all notes off?
      mMidiOut->closePort();

      if (port == 0)
        return true;
#if defined OS_WIN
      else
      {
        mMidiOut->openPort(port-1);
        return true;
      }
#elif defined OS_MAC
      else if(port == 1)
      {
        std::string virtualMidiOutputName = "From ";
        virtualMidiOutputName += BUNDLE_NAME;
        mMidiOut->openVirtualPort(virtualMidiOutputName);
        return true;
      }
      else
      {
        mMidiOut->openPort(port-2);
        return true;
      }
#else
  #error NOT IMPLEMENTED
#endif
    }
  }

  return false;
}

void IPlugAPPHost::CloseAudio()
{
  mTestTone.Stop();
  if (mDAC && mDAC->isStreamOpen())
  {
    if (mDAC->isStreamRunning())
    {
      mAudioEnding = true;

      for (int attempt = 0; attempt < 200 && !mAudioDone; ++attempt)
        Sleep(10);

      try
      {
        mDAC->abortStream();
      }
      catch (RtAudioError& e)
      {
        e.printMessage();
      }
    }

    mDAC->closeStream();
  }
  mCallbackActive.store(false, std::memory_order_release);
  mInputBufPtrs.Empty();
  mOutputBufPtrs.Empty();
}

bool IPlugAPPHost::InitAudio(uint32_t inId, uint32_t outId, uint32_t sr, uint32_t iovs)
{
  CloseAudio();
  mLastAudioError.clear();
  mCallbackActive.store(false, std::memory_order_release);

  RtAudio::StreamParameters iParams, oParams;
  RtAudio::DeviceInfo inputInfo;
  const bool noInput = inId == kNoInputDevice;
  if (!noInput) inputInfo = mDAC->getDeviceInfo(inId);
  RtAudio::DeviceInfo outputInfo = mDAC->getDeviceInfo(outId);

  iParams.deviceId = noInput ? 0 : inId;
  iParams.nChannels = noInput ? 0 : GetPlug()->MaxNChannels(ERoute::kInput);
  if (iParams.nChannels > 0)
  {
    mState.mAudioInChanL = ClampFirstChannel(mState.mAudioInChanL, inputInfo.inputChannels, iParams.nChannels);
    mState.mAudioInChanR =
      iParams.nChannels > 1 ? std::min(mState.mAudioInChanL + 1, inputInfo.inputChannels) : mState.mAudioInChanL;
    iParams.firstChannel = mState.mAudioInChanL - 1;
  }

  oParams.deviceId = outId;
  oParams.nChannels = GetPlug()->MaxNChannels(ERoute::kOutput);
  if (oParams.nChannels > 0)
  {
    mState.mAudioOutChanL = ClampFirstChannel(mState.mAudioOutChanL, outputInfo.outputChannels, oParams.nChannels);
    mState.mAudioOutChanR =
      oParams.nChannels > 1 ? std::min(mState.mAudioOutChanL + 1, outputInfo.outputChannels) : mState.mAudioOutChanL;
    oParams.firstChannel = mState.mAudioOutChanL - 1;
  }

  mBufferSize = iovs; // mBufferSize may get changed by stream

  const auto inputName = noInput ? std::string("No input") : GetAudioDeviceName(inId);
  DBGMSG("\ntrying to start audio stream @ %i sr, %i buffer size\nindev = %i:%s\noutdev = %i:%s\ninputs = %i\noutputs = %i\n",
         sr, mBufferSize, inId, inputName.c_str(), outId, GetAudioDeviceName(outId).c_str(), iParams.nChannels, oParams.nChannels);

  RtAudio::StreamOptions options;
  options.flags = RTAUDIO_NONINTERLEAVED;
  // options.streamName = BUNDLE_NAME; // JACK stream name, not used on other streams

  mBufIndex = 0;
  mSamplesElapsed = 0;
  mSampleRate = (double) sr;
  mVecWait = 0;
  mAudioEnding = false;
  mAudioDone = false;
  mNoInputActive = noInput;

  mIPlug->SetBlockSize(APP_SIGNAL_VECTOR_SIZE);
  mIPlug->SetSampleRate(mSampleRate);
  mIPlug->OnReset();

  try
  {
    mDAC->openStream(&oParams, iParams.nChannels > 0 ? &iParams : nullptr, RTAUDIO_FLOAT64, sr, &mBufferSize, &AudioCallback, this, &options /*, &ErrorCallback */);

    for (int i = 0; i < GetPlug()->MaxNChannels(ERoute::kInput); i++)
    {
      mInputBufPtrs.Add(nullptr); //will be set in callback
    }

    for (int i = 0; i < oParams.nChannels; i++)
    {
      mOutputBufPtrs.Add(nullptr); //will be set in callback
    }

    mSilentInput.assign(static_cast<std::size_t>(GetPlug()->MaxNChannels(ERoute::kInput)) * mBufferSize, 0.0);
    mDAC->startStream();
    mState.mBufferSize = mBufferSize;
    mActiveState = mState;

    for (int attempt = 0; attempt < 40 && !mCallbackActive.load(std::memory_order_acquire); ++attempt)
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    if (!mCallbackActive.load(std::memory_order_acquire))
    {
      mLastAudioError = "Audio callback did not start.";
      CloseAudio();
      return false;
    }
  }
  catch (RtAudioError& e)
  {
    mLastAudioError = e.getMessage().substr(0, 512);
    return false;
  }

  return true;
}

bool IPlugAPPHost::InitMidi()
{
  try
  {
    mMidiIn = std::make_unique<RtMidiIn>();
  }
  catch (RtMidiError &error)
  {
    mMidiIn = nullptr;
    error.printMessage();
    return false;
  }

  try
  {
    mMidiOut = std::make_unique<RtMidiOut>();
  }
  catch (RtMidiError &error)
  {
    mMidiOut = nullptr;
    error.printMessage();
    return false;
  }

  mMidiIn->setCallback(&MIDICallback, this);
  mMidiIn->ignoreTypes(false, true, false );

  return true;
}

void ApplyFades(double *pBuffer, int nChans, int nFrames, bool down)
{
  for (int i = 0; i < nChans; i++)
  {
    double *pIO = pBuffer + (i * nFrames);

    if (down)
    {
      for (int j = 0; j < nFrames; j++)
        pIO[j] *= ((double) (nFrames - (j + 1)) / (double) nFrames);
    }
    else
    {
      for (int j = 0; j < nFrames; j++)
        pIO[j] *= ((double) j / (double) nFrames);
    }
  }
}

// static
int IPlugAPPHost::AudioCallback(void* pOutputBuffer, void* pInputBuffer, uint32_t nFrames, double streamTime, RtAudioStreamStatus status, void* pUserData)
{
  IPlugAPPHost* _this = (IPlugAPPHost*) pUserData;

  _this->mCallbackActive.store(true, std::memory_order_release);
  if (status != 0) _this->mUnderrunCount.fetch_add(1, std::memory_order_relaxed);

  int nins = _this->GetPlug()->MaxNChannels(ERoute::kInput);
  int nouts = _this->GetPlug()->MaxNChannels(ERoute::kOutput);

  double* pInputBufferD = static_cast<double*>(pInputBuffer);
  if (!pInputBufferD)
  {
    const auto required = static_cast<std::size_t>(nins) * nFrames;
    if (_this->mSilentInput.size() < required) return 1;
    std::fill_n(_this->mSilentInput.data(), required, 0.0);
    pInputBufferD = _this->mSilentInput.data();
  }
  double* pOutputBufferD = static_cast<double*>(pOutputBuffer);

  bool startWait = _this->mVecWait >= APP_N_VECTOR_WAIT; // wait APP_N_VECTOR_WAIT * iovs before processing audio, to avoid clicks
  bool doFade = _this->mVecWait == APP_N_VECTOR_WAIT || _this->mAudioEnding;

  if (startWait && !_this->mAudioDone)
  {
    if (doFade)
      ApplyFades(pInputBufferD, nins, nFrames, _this->mAudioEnding);

    for (int i = 0; i < nFrames; i++)
    {
      _this->mBufIndex %= APP_SIGNAL_VECTOR_SIZE;

      if (_this->mBufIndex == 0)
      {
        for (int c = 0; c < nins; c++)
        {
          _this->mInputBufPtrs.Set(c, (pInputBufferD + (c * nFrames)) + i);
        }

        for (int c = 0; c < nouts; c++)
        {
          _this->mOutputBufPtrs.Set(c, (pOutputBufferD + (c * nFrames)) + i);
        }

        _this->mIPlug->AppProcess(_this->mInputBufPtrs.GetList(), _this->mOutputBufPtrs.GetList(), APP_SIGNAL_VECTOR_SIZE);

        _this->mSamplesElapsed += APP_SIGNAL_VECTOR_SIZE;
      }

      for (int c = 0; c < nouts; c++)
      {
        pOutputBufferD[c * nFrames + i] *= APP_MULT;
      }

      _this->mBufIndex++;
    }

    if (doFade)
      ApplyFades(pOutputBufferD, nouts, nFrames, _this->mAudioEnding);

    if (_this->mAudioEnding)
      _this->mAudioDone = true;
  }
  else
  {
    memset(pOutputBufferD, 0, nFrames * nouts * sizeof(double));
  }

  _this->mVecWait = std::min(_this->mVecWait + 1, uint32_t(APP_N_VECTOR_WAIT + 1));

  _this->mTestTone.ProcessNonInterleaved(pOutputBufferD, nouts, static_cast<int>(nFrames));
  _this->mMeters.Observe(pInputBufferD, nins, pOutputBufferD, nouts, static_cast<int>(nFrames));

  return 0;
}

std::vector<amphibia::setup::AudioBackendDescriptor> IPlugAPPHost::Enumerate()
{
  std::vector<amphibia::setup::AudioBackendDescriptor> result;
  const std::array<std::pair<const char*, const char*>, 2> backends{{{"asio", "ASIO"},
                                                                    {"directsound", "DirectSound"}}};
  for (const auto& backend : backends)
  {
    amphibia::setup::AudioBackendDescriptor descriptor;
    descriptor.id = backend.first;
    descriptor.name = backend.second;
    descriptor.preferred = descriptor.id == "asio";
    try
    {
      RtAudio probe(BackendApi(descriptor.id));
      const auto count = probe.getDeviceCount();
      for (std::uint32_t index = 0; index < count; ++index)
      {
        const auto info = probe.getDeviceInfo(index);
        if (!info.probed || (info.inputChannels == 0 && info.outputChannels == 0)) continue;
        amphibia::setup::AudioDeviceDescriptor device;
        device.id = descriptor.id + ':' + std::to_string(index);
        device.name = info.name.substr(0, 256);
        device.defaultInput = info.isDefaultInput;
        device.defaultOutput = info.isDefaultOutput;
        for (unsigned int channel = 0; channel < info.inputChannels; ++channel)
          device.inputChannelNames.push_back("Input " + std::to_string(channel + 1));
        for (unsigned int channel = 0; channel < info.outputChannels; ++channel)
          device.outputChannelNames.push_back("Output " + std::to_string(channel + 1));
        for (const auto rate : info.sampleRates) device.sampleRates.push_back(static_cast<std::uint32_t>(rate));
        for (const auto& size : kBufferSizeOptions) device.bufferSizes.push_back(static_cast<std::uint32_t>(std::stoul(size)));
        descriptor.devices.push_back(std::move(device));
      }
    }
    catch (const RtAudioError&) {}
    if (!descriptor.devices.empty()) result.push_back(std::move(descriptor));
  }
  return result;
}

std::optional<amphibia::setup::AudioConfiguration> IPlugAPPHost::Current() const
{
  if (!mDAC || !mDAC->isStreamOpen() || !mDAC->isStreamRunning()) return std::nullopt;
  amphibia::setup::AudioConfiguration value;
  value.backendId = mActiveState.mAudioDriverType == kDeviceASIO ? "asio" : "directsound";
  value.backendName = mActiveState.mAudioDriverType == kDeviceASIO ? "ASIO" : "DirectSound";
  const auto inputIndex = mNoInputActive ? -1 : GetAudioDeviceIdx(mActiveState.mAudioInDev.Get());
  const auto outputIndex = GetAudioDeviceIdx(mActiveState.mAudioOutDev.Get());
  value.inputDeviceId = mNoInputActive ? std::string{} : value.backendId + ':' + std::to_string(std::max(inputIndex, 0));
  value.outputDeviceId = value.backendId + ':' + std::to_string(std::max(outputIndex, 0));
  value.inputDeviceName = mNoInputActive ? "No input" : mActiveState.mAudioInDev.Get();
  value.outputDeviceName = mActiveState.mAudioOutDev.Get();
  value.inputChannel = mNoInputActive ? 0 : mActiveState.mAudioInChanL;
  value.outputFirstChannel = mActiveState.mAudioOutChanL;
  value.outputChannelCount = static_cast<std::uint32_t>(GetPlug()->MaxNChannels(ERoute::kOutput));
  value.sampleRate = mActiveState.mAudioSR;
  value.bufferSize = mActiveState.mBufferSize;
  return value;
}

std::optional<std::string> IPlugAPPHost::Validate(const amphibia::setup::AudioConfiguration& requested) const
{
  if (requested.backendId != "asio" && requested.backendId != "directsound") return "Audio backend is unavailable.";
  const auto inputIndex = DeviceIndex(requested.inputDeviceId, requested.backendId);
  const auto outputIndex = DeviceIndex(requested.outputDeviceId, requested.backendId);
  if (!outputIndex) return "Output device is unavailable.";
  try
  {
    RtAudio probe(BackendApi(requested.backendId));
    if (*outputIndex >= probe.getDeviceCount()) return "Output device is unavailable.";
    const auto output = probe.getDeviceInfo(*outputIndex);
    if (!output.probed || output.outputChannels == 0) return "Output device is unavailable.";
    std::optional<RtAudio::DeviceInfo> input;
    if (!requested.inputDeviceId.empty())
    {
      if (!inputIndex || *inputIndex >= probe.getDeviceCount()) return "Input device is unavailable.";
      input = probe.getDeviceInfo(*inputIndex);
      if (!input->probed || input->inputChannels == 0) return "Input device is unavailable.";
      if (requested.inputChannel == 0 || requested.inputChannel > input->inputChannels) return "Input channel is unavailable.";
    }
    if (requested.outputFirstChannel == 0 || requested.outputFirstChannel + requested.outputChannelCount - 1 > output.outputChannels)
      return "Output channels are unavailable.";
    if ((input && std::find(input->sampleRates.begin(), input->sampleRates.end(), requested.sampleRate) == input->sampleRates.end()) ||
        std::find(output.sampleRates.begin(), output.sampleRates.end(), requested.sampleRate) == output.sampleRates.end())
      return "Selected sample rate is not supported.";
    if (std::find_if(kBufferSizeOptions, kBufferSizeOptions + kNumBufferSizeOptions,
                     [&](const std::string& value) { return std::stoul(value) == requested.bufferSize; }) ==
        kBufferSizeOptions + kNumBufferSizeOptions) return "Selected buffer size is not supported.";
  }
  catch (const RtAudioError&) { return "Audio device information is unavailable."; }
  return std::nullopt;
}

amphibia::setup::AudioOpenResult IPlugAPPHost::Open(const amphibia::setup::AudioConfiguration& requested)
{
  if (const auto error = Validate(requested)) return {false, false, false, *error};
  const auto parsedInput = DeviceIndex(requested.inputDeviceId, requested.backendId);
  const auto inputIndex = parsedInput ? *parsedInput : kNoInputDevice;
  const auto outputIndex = *DeviceIndex(requested.outputDeviceId, requested.backendId);
  mState.mAudioDriverType = requested.backendId == "asio" ? kDeviceASIO : kDeviceDS;
  mState.mAudioInDev.Set(requested.inputDeviceId.empty() ? "No input" : requested.inputDeviceName.c_str());
  mState.mAudioOutDev.Set(requested.outputDeviceName.c_str());
  mState.mAudioInChanL = requested.inputChannel;
  mState.mAudioInChanR = requested.inputChannel;
  mState.mAudioOutChanL = requested.outputFirstChannel;
  mState.mAudioOutChanR = requested.outputFirstChannel + requested.outputChannelCount - 1;
  mState.mAudioSR = requested.sampleRate;
  mState.mBufferSize = requested.bufferSize;
  if (!TryToChangeAudioDriverType()) return {false, false, false, "Audio backend could not be opened."};
  ProbeAudioIO();
  const bool opened = InitAudio(inputIndex, outputIndex, requested.sampleRate, requested.bufferSize);
  return {opened, CallbackActive(), opened, opened ? std::string{} :
          (mLastAudioError.empty() ? "Audio device could not be opened." : mLastAudioError)};
}

std::string IPlugAPPHost::AudioDiagnostics() const
{
  std::ostringstream stream;
  const auto current = Current();
  stream << "Amphibia audio diagnostics\r\n";
  if (!current)
  {
    stream << "Device-open status: No audio\r\nLast error: "
           << (mLastAudioError.empty() ? "None" : mLastAudioError) << "\r\n";
    return stream.str();
  }
  stream << "Backend: " << current->backendName << "\r\nInput device: " << current->inputDeviceName
         << "\r\nOutput device: " << current->outputDeviceName << "\r\nInput channel: " << current->inputChannel
         << "\r\nOutput first channel: " << current->outputFirstChannel << "\r\nSample rate: " << current->sampleRate
         << "\r\nBuffer size: " << current->bufferSize << "\r\nCallback active: "
         << (CallbackActive() ? "Yes" : "No") << "\r\nUnderruns: " << UnderrunCount() << "\r\n";
  return stream.str();
}

ManagedLibraryConfigurationStatus IPlugAPPHost::ManagedLibraryConfiguration() const
{
  return amphibia::configuration::ManagedLibrary(*mIPlug);
}

void IPlugAPPHost::VerifyManagedLibrary()
{
  amphibia::configuration::VerifyManagedLibrary(*mIPlug);
}

void IPlugAPPHost::ClearManagedLibraryStaging()
{
  amphibia::configuration::ClearManagedLibraryStaging(*mIPlug);
}

void IPlugAPPHost::ScanManagedLibrarySources(std::vector<std::filesystem::path> sources)
{
  amphibia::configuration::ScanManagedLibrarySources(*mIPlug, std::move(sources));
}

void IPlugAPPHost::CancelManagedLibraryTask()
{
  amphibia::configuration::CancelManagedLibraryTask(*mIPlug);
}

// static
void IPlugAPPHost::MIDICallback(double deltatime, std::vector<uint8_t>* pMsg, void* pUserData)
{
  IPlugAPPHost* _this = (IPlugAPPHost*) pUserData;

  if (pMsg->size() == 0 || _this->mExiting)
    return;

  if (pMsg->size() > 3)
  {
    if(pMsg->size() > MAX_SYSEX_SIZE)
    {
      DBGMSG("SysEx message exceeds MAX_SYSEX_SIZE\n");
      return;
    }

    SysExData data { 0, static_cast<int>(pMsg->size()), pMsg->data() };

    _this->mIPlug->mSysExMsgsFromCallback.Push(data);
    return;
  }
  else if (pMsg->size())
  {
    IMidiMsg msg;
    msg.mStatus = pMsg->at(0);
    pMsg->size() > 1 ? msg.mData1 = pMsg->at(1) : msg.mData1 = 0;
    pMsg->size() > 2 ? msg.mData2 = pMsg->at(2) : msg.mData2 = 0;

    _this->mIPlug->mMidiMsgsFromCallback.Push(msg);
  }
}

// static
void IPlugAPPHost::ErrorCallback(RtAudioError::Type type, const std::string &errorText )
{
  //TODO:
}
