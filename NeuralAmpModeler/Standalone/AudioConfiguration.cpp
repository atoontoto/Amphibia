#include "AudioConfiguration.h"

#include <algorithm>
#include <cmath>

namespace amphibia::setup
{
AudioTransactionResult AudioConfigurationTransaction::Apply(const AudioConfiguration& requested)
{
  AudioTransactionResult result;
  const auto previous = mService.Current();
  if (const auto error = mService.Validate(requested))
  {
    result.message = *error;
    result.active = previous;
    return result;
  }
  mService.StopTestTone();
  const auto opened = mService.Open(requested);
  if (opened.success && opened.callbackActive && opened.outputAvailable)
  {
    result.committed = true;
    result.active = requested;
    result.message = "Audio configuration is active.";
    return result;
  }
  result.message = opened.message.empty() ? "Audio device could not be opened." : opened.message;
  if (previous)
  {
    const auto restored = mService.Open(*previous);
    if (restored.success && restored.callbackActive && restored.outputAvailable)
    {
      result.rolledBack = true;
      result.active = previous;
      result.message += " The previous working configuration was restored.";
      return result;
    }
  }
  mService.Close();
  result.noAudio = true;
  result.message += " Amphibia is continuing without audio.";
  return result;
}

bool TestToneGenerator::Start(double sampleRate, double frequencyHz, double durationSeconds)
{
  if (sampleRate < 8000.0 || sampleRate > 768000.0 || frequencyHz < 20.0 || frequencyHz > 20000.0 ||
      durationSeconds <= 0.0 || durationSeconds > 5.0) return false;
  if (mActive.exchange(true, std::memory_order_acq_rel)) return false;
  constexpr double pi = 3.14159265358979323846;
  mPhase = 0.0;
  mPhaseIncrement = 2.0 * pi * frequencyHz / sampleRate;
  mFadeSamples = static_cast<std::uint64_t>(std::max(1.0, sampleRate * 0.025));
  mTotalSamples = static_cast<std::uint64_t>(sampleRate * durationSeconds);
  mRemaining.store(mTotalSamples, std::memory_order_release);
  mGain = 0.0;
  mGainStep = kMaximumPeak / static_cast<double>(mFadeSamples);
  return true;
}

void TestToneGenerator::Stop()
{
  mRemaining.store(std::min(mRemaining.load(std::memory_order_relaxed), mFadeSamples), std::memory_order_release);
}

void TestToneGenerator::ProcessNonInterleaved(double* output, int channels, int frames)
{
  if (!output || channels <= 0 || frames <= 0 || !mActive.load(std::memory_order_acquire)) return;
  constexpr double pi = 3.14159265358979323846;
  auto remaining = mRemaining.load(std::memory_order_relaxed);
  for (int frame = 0; frame < frames; ++frame)
  {
    if (remaining == 0)
    {
      mActive.store(false, std::memory_order_release);
      break;
    }
    const auto elapsed = mTotalSamples > remaining ? mTotalSamples - remaining : 0;
    if (elapsed < mFadeSamples) mGain = std::min(kMaximumPeak, mGain + mGainStep);
    if (remaining <= mFadeSamples) mGain = std::max(0.0, mGain - mGainStep);
    const double sample = std::sin(mPhase) * mGain;
    for (int channel = 0; channel < channels; ++channel) output[channel * frames + frame] += sample;
    mPhase += mPhaseIncrement;
    if (mPhase >= 2.0 * pi) mPhase -= 2.0 * pi;
    --remaining;
  }
  mRemaining.store(remaining, std::memory_order_release);
  if (remaining == 0) mActive.store(false, std::memory_order_release);
}

void AudioMeters::Observe(const double* input, int inputChannels, const double* output, int outputChannels, int frames)
{
  double inputPeak = 0.0;
  double outputPeak = 0.0;
  if (input)
    for (int channel = 0; channel < inputChannels; ++channel)
      for (int frame = 0; frame < frames; ++frame)
        inputPeak = std::max(inputPeak, std::abs(input[channel * frames + frame]));
  if (output)
    for (int channel = 0; channel < outputChannels; ++channel)
      for (int frame = 0; frame < frames; ++frame)
        outputPeak = std::max(outputPeak, std::abs(output[channel * frames + frame]));
  mInputPeak.store(inputPeak, std::memory_order_relaxed);
  mOutputPeak.store(outputPeak, std::memory_order_relaxed);
}

void AudioMeters::Reset()
{
  mInputPeak.store(0.0, std::memory_order_relaxed);
  mOutputPeak.store(0.0, std::memory_order_relaxed);
}
} // namespace amphibia::setup
