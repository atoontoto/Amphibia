#pragma once

#include "SetupTypes.h"

#include <atomic>
#include <optional>
#include <string>
#include <vector>

namespace amphibia::setup
{
struct AudioOpenResult
{
  bool success{};
  bool callbackActive{};
  bool outputAvailable{};
  std::string message;
};

class IAudioConfigurationService
{
public:
  virtual ~IAudioConfigurationService() = default;
  virtual std::vector<AudioBackendDescriptor> Enumerate() = 0;
  virtual std::optional<AudioConfiguration> Current() const = 0;
  virtual std::optional<std::string> Validate(const AudioConfiguration& requested) const = 0;
  virtual AudioOpenResult Open(const AudioConfiguration& requested) = 0;
  virtual void Close() = 0;
  virtual void StopTestTone() = 0;
};

struct AudioTransactionResult
{
  bool committed{};
  bool rolledBack{};
  bool noAudio{};
  std::string message;
  std::optional<AudioConfiguration> active;
};

class AudioConfigurationTransaction
{
public:
  explicit AudioConfigurationTransaction(IAudioConfigurationService& service) : mService(service) {}
  AudioTransactionResult Apply(const AudioConfiguration& requested);

private:
  IAudioConfigurationService& mService;
};

class TestToneGenerator
{
public:
  static constexpr double kMaximumPeak = 0.05;
  bool Start(double sampleRate, double frequencyHz = 440.0, double durationSeconds = 2.0);
  void Stop();
  bool Active() const { return mState.load(std::memory_order_acquire) == 2; }
  void ProcessNonInterleaved(double* output, int channels, int frames);

private:
  // 0 = inactive, 1 = being initialized by the UI thread, 2 = active on the audio thread.
  std::atomic<std::uint8_t> mState{0};
  std::atomic<std::uint64_t> mRemaining{0};
  double mPhase{};
  double mPhaseIncrement{};
  double mGain{};
  double mGainStep{};
  std::uint64_t mFadeSamples{};
  std::uint64_t mTotalSamples{};
};

class AudioMeters
{
public:
  void Observe(const double* input, int inputChannels, const double* output, int outputChannels, int frames);
  double InputPeak() const { return mInputPeak.load(std::memory_order_relaxed); }
  double OutputPeak() const { return mOutputPeak.load(std::memory_order_relaxed); }
  void Reset();

private:
  std::atomic<double> mInputPeak{0.0};
  std::atomic<double> mOutputPeak{0.0};
};
} // namespace amphibia::setup
