#include "../../NeuralAmpModeler/Loading/AsyncDSPWorker.h"
#include "../../NeuralAmpModeler/Loading/FileInspection.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace amphibia::loading;
using namespace std::chrono_literals;

namespace
{

std::atomic<long long> gMaximumActivationNanoseconds{0};

void Require(bool condition, const char* message)
{
  if (!condition)
    throw std::runtime_error(message);
}

template <typename Predicate> void WaitUntil(Predicate predicate, const char* message)
{
  const auto deadline = std::chrono::steady_clock::now() + 5s;
  while (!predicate())
  {
    if (std::chrono::steady_clock::now() >= deadline)
      throw std::runtime_error(message);
    std::this_thread::sleep_for(1ms);
  }
}

struct FakeMetadata
{
  int identity{};
};

struct FakeDSP
{
  explicit FakeDSP(int value)
  : identity(value)
  {
    ++live;
  }

  ~FakeDSP()
  {
    {
      std::lock_guard<std::mutex> lock(destructionMutex);
      destructionThreads.push_back(std::this_thread::get_id());
    }
    --live;
  }

  int identity{};
  static inline std::atomic<int> live{0};
  static inline std::mutex destructionMutex;
  static inline std::vector<std::thread::id> destructionThreads;
};

using Worker = AsyncDSPWorker<FakeDSP, FakeMetadata>;

ProcessingConfiguration Config(std::uint64_t generation)
{
  return {48000.0, 128, 1, 2, generation};
}

int Identity(const std::string& path)
{
  if (path == "A.nam") return 1;
  if (path == "B.nam") return 2;
  if (path == "C.nam") return 3;
  return 9;
}

void ActivateReady(Worker& worker, int expected)
{
  WaitUntil([&] { return worker.Status().state == LoadState::ReadyToActivate; }, "prepared result did not become ready");
  const auto start = std::chrono::steady_clock::now();
  const auto activation = worker.ActivateAtBlockBoundary();
  const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
  auto maximum = gMaximumActivationNanoseconds.load();
  while (elapsed > maximum && !gMaximumActivationNanoseconds.compare_exchange_weak(maximum, elapsed))
  {
  }
  Require(activation.activated, "ready result did not activate");
  Require(activation.metadata.identity == expected, "wrong prepared result activated");
  WaitUntil([&] { return worker.Status().state == LoadState::Active; }, "activation was not acknowledged");
  Require(worker.ActiveForAudio() != nullptr && worker.ActiveForAudio()->identity == expected, "active DSP mismatch");
}

void TestNewestWinsAndFailurePreservesActive()
{
  std::atomic<bool> enteredA{false};
  std::atomic<bool> releaseA{false};
  const auto callbackThread = std::this_thread::get_id();
  std::atomic<bool> preparedOnCallbackThread{false};
  Worker worker([&](const LoadRequest& request, const Worker::CancelCheck&, const Worker::ProgressCallback& progress) {
    if (std::this_thread::get_id() == callbackThread)
      preparedOnCallbackThread = true;
    if (request.path == "invalid.nam")
      throw LoadException(LoadErrorCode::InvalidModel, "Invalid model");
    if (request.path == "A.nam" && !releaseA.load())
    {
      enteredA = true;
      while (!releaseA.load())
        std::this_thread::yield();
    }
    progress(LoadState::Preparing, 0.5f);
    const int identity = Identity(request.path);
    return PreparedDSP<FakeDSP, FakeMetadata>{std::make_unique<FakeDSP>(identity), {identity}};
  });

  worker.Submit("A.nam", Config(1));
  WaitUntil([&] { return enteredA.load(); }, "A did not start");
  worker.Submit("B.nam", Config(1));
  const auto cRequest = worker.Submit("C.nam", Config(1));
  releaseA = true;
  ActivateReady(worker, 3);
  Require(worker.Status().requestId == cRequest, "older request reported active");
  Require(!preparedOnCallbackThread.load(), "DSP preparation ran on the simulated audio thread");

  worker.Submit("invalid.nam", Config(1));
  WaitUntil([&] { return worker.Status().state == LoadState::Failed; }, "invalid model did not fail");
  Require(worker.ActiveForAudio() != nullptr && worker.ActiveForAudio()->identity == 3,
          "failed replacement destroyed the active DSP");
}

void TestGenerationCancellationAndReclamation()
{
  Worker worker([](const LoadRequest& request, const Worker::CancelCheck& cancelled,
                   const Worker::ProgressCallback&) {
    if (cancelled())
      throw LoadException(LoadErrorCode::Superseded, "Superseded");
    const int identity = Identity(request.path);
    return PreparedDSP<FakeDSP, FakeMetadata>{std::make_unique<FakeDSP>(identity), {identity}};
  });

  worker.Submit("A.nam", Config(1));
  ActivateReady(worker, 1);
  const auto audioThread = std::this_thread::get_id();

  worker.Submit("B.nam", Config(1));
  ActivateReady(worker, 2);
  WaitUntil([] {
    std::lock_guard<std::mutex> lock(FakeDSP::destructionMutex);
    return !FakeDSP::destructionThreads.empty();
  }, "retired DSP was not reclaimed");
  {
    std::lock_guard<std::mutex> lock(FakeDSP::destructionMutex);
    Require(FakeDSP::destructionThreads.back() != audioThread, "retired DSP was destroyed on the audio thread");
  }
  Require(worker.PeakRetiredDepth() <= 1, "retired mailbox exceeded its bound");

  worker.Submit("C.nam", Config(1));
  WaitUntil([&] { return worker.Status().state == LoadState::ReadyToActivate; }, "generation-1 C was not ready");
  const auto newRequest = worker.Reconfigure(Config(2));
  Require(!worker.ActivateAtBlockBoundary().activated, "old processing generation activated");
  ActivateReady(worker, 3);
  Require(worker.Status().requestId == newRequest, "reconfigured request was not active");

  worker.Submit("A.nam", Config(2));
  worker.Cancel();
  WaitUntil([&] { return worker.Status().state == LoadState::Cancelled; }, "cancel status was not published");
  Require(!worker.ActivateAtBlockBoundary().activated, "cancelled result activated");

  worker.SubmitClear(Config(2));
  WaitUntil([&] { return worker.Status().state == LoadState::ReadyToActivate; }, "clear was not ready");
  const auto clear = worker.ActivateAtBlockBoundary();
  Require(clear.activated && clear.cleared, "clear did not activate");
  WaitUntil([&] { return worker.Status().state == LoadState::Active; }, "clear was not acknowledged");
  Require(worker.ActiveForAudio() == nullptr, "clear retained an active DSP");
}

void TestShutdownDuringCooperativePreparation()
{
  std::atomic<bool> entered{false};
  const auto start = std::chrono::steady_clock::now();
  {
    Worker worker([&](const LoadRequest&, const Worker::CancelCheck& cancelled, const Worker::ProgressCallback&) {
      entered = true;
      while (!cancelled())
        std::this_thread::yield();
      return PreparedDSP<FakeDSP, FakeMetadata>{};
    });
    worker.Submit("A.nam", Config(1));
    WaitUntil([&] { return entered.load(); }, "shutdown test did not start");
  }
  Require(std::chrono::steady_clock::now() - start < 2s, "cooperative shutdown was not bounded");
}

void TestTwoStagesAndBoundedStress()
{
  auto factory = [](const LoadRequest& request, const Worker::CancelCheck& cancelled,
                    const Worker::ProgressCallback&) {
    if (request.path.find("invalid") != std::string::npos)
      throw LoadException(LoadErrorCode::InvalidFormat, "Invalid fixture");
    if (cancelled())
      throw LoadException(LoadErrorCode::Superseded, "Superseded");
    const int identity = Identity(request.path);
    return PreparedDSP<FakeDSP, FakeMetadata>{std::make_unique<FakeDSP>(identity), {identity}};
  };
  Worker model(factory);
  Worker ir(factory);

  for (int iteration = 0; iteration < 32; ++iteration)
  {
    const std::string modelName = iteration % 2 == 0 ? "A.nam" : "B.nam";
    const std::string irName = iteration % 2 == 0 ? "B.nam" : "A.nam";
    model.Submit(modelName, Config(1));
    ir.Submit(irName, Config(1));
    ActivateReady(model, Identity(modelName));
    ActivateReady(ir, Identity(irName));

    model.Submit("invalid.nam", Config(1));
    WaitUntil([&] { return model.Status().state == LoadState::Failed; }, "stress invalid model did not fail");
    Require(model.ActiveForAudio() != nullptr, "stress failure removed the active model");

    // Same-file requests and cancellation are intentionally repeated.
    ir.Submit(irName, Config(1));
    ir.Cancel();
    WaitUntil([&] { return ir.Status().state == LoadState::Cancelled; }, "stress cancellation failed");
  }

  Require(model.PeakRetiredDepth() <= 1 && ir.PeakRetiredDepth() <= 1, "stress retirement was unbounded");
}

void PutU16(std::ofstream& stream, std::uint16_t value)
{
  const char bytes[] = {static_cast<char>(value & 0xff), static_cast<char>((value >> 8) & 0xff)};
  stream.write(bytes, sizeof(bytes));
}

void PutU32(std::ofstream& stream, std::uint32_t value)
{
  const char bytes[] = {static_cast<char>(value & 0xff), static_cast<char>((value >> 8) & 0xff),
                        static_cast<char>((value >> 16) & 0xff), static_cast<char>((value >> 24) & 0xff)};
  stream.write(bytes, sizeof(bytes));
}

void WriteWave(const std::filesystem::path& path, std::uint16_t channels, std::uint16_t bits,
               std::uint32_t sampleRate, std::uint32_t dataBytes)
{
  std::ofstream stream(path, std::ios::binary);
  stream.write("RIFF", 4);
  PutU32(stream, 36 + dataBytes);
  stream.write("WAVEfmt ", 8);
  PutU32(stream, 16);
  PutU16(stream, 1);
  PutU16(stream, channels);
  PutU32(stream, sampleRate);
  const auto blockAlign = static_cast<std::uint16_t>(channels * bits / 8);
  PutU32(stream, sampleRate * blockAlign);
  PutU16(stream, blockAlign);
  PutU16(stream, bits);
  stream.write("data", 4);
  PutU32(stream, dataBytes);
  std::vector<char> data(dataBytes, 0);
  stream.write(data.data(), static_cast<std::streamsize>(data.size()));
}

void TestWaveInspection()
{
  const auto root = std::filesystem::temp_directory_path() / "amphibia-m2-fixtures";
  std::filesystem::create_directories(root);
  const auto valid = root / "valid.wav";
  const auto stereo = root / "stereo.wav";
  const auto empty = root / "empty.wav";
  const auto unsupported = root / "unsupported.wav";
  WriteWave(valid, 1, 16, 44100, 32);
  WriteWave(stereo, 2, 16, 48000, 32);
  WriteWave(empty, 1, 16, 48000, 0);
  WriteWave(unsupported, 1, 8, 48000, 16);

  const auto inspected = InspectWaveFile(valid);
  Require(inspected.channels == 1 && inspected.sampleRate == 44100 && inspected.dataBytes == 32,
          "valid mismatched-rate WAV inspection changed");
  for (const auto& path : {stereo, empty, unsupported})
  {
    bool rejected = false;
    try
    {
      (void)InspectWaveFile(path);
    }
    catch (const LoadException&)
    {
      rejected = true;
    }
    Require(rejected, "invalid WAV was accepted");
  }
  std::filesystem::remove_all(root);
}

} // namespace

int main()
{
  try
  {
    TestNewestWinsAndFailurePreservesActive();
    TestGenerationCancellationAndReclamation();
    TestShutdownDuringCooperativePreparation();
    TestTwoStagesAndBoundedStress();
    TestWaveInspection();
    Require(FakeDSP::live.load() == 0, "fake DSP leak detected");
    std::cout << "Milestone 2 async loading tests: PASS\n"
              << "Maximum measured activation call: " << gMaximumActivationNanoseconds.load() << " ns\n";
    return 0;
  }
  catch (const std::exception& error)
  {
    std::cerr << "Milestone 2 async loading tests: FAIL: " << error.what() << '\n';
    return 1;
  }
}
