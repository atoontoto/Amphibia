#include "Tone3000DownloadCoordinator.h"

#include "../../Library/ContentHash.h"
#include "../../Loading/FileInspection.h"
#include "../../Network/HttpPolicy.h"

#include <mutex>

namespace amphibia::providers::tone3000 {
namespace {

template <typename T> ProviderResult<T> Failed(ProviderErrorCode code, const char* message,
                                                std::optional<int> status = std::nullopt)
{
  ProviderResult<T> result; result.error = {code, message, status}; return result;
}

std::string FormatName(RemoteContentFormat format)
{
  if (format == RemoteContentFormat::Nam) return "nam";
  if (format == RemoteContentFormat::ImpulseResponse) return "ir";
  return "unknown";
}

std::string GearName(RemoteGearRole role)
{
  switch (role)
  {
    case RemoteGearRole::AmpHead: return "amp";
    case RemoteGearRole::AmpAndCab: return "full-rig";
    case RemoteGearRole::Pedal: return "pedal";
    case RemoteGearRole::Cabinet: return "ir";
    case RemoteGearRole::Space: return "space";
    case RemoteGearRole::Outboard: return "outboard";
    case RemoteGearRole::Experimental: return "experimental";
    default: return "unknown";
  }
}

std::string ArchitectureName(RemoteNamArchitecture architecture)
{
  if (architecture == RemoteNamArchitecture::A1) return "1";
  if (architecture == RemoteNamArchitecture::A2) return "2";
  return "unknown";
}

} // namespace

struct Tone3000DownloadCoordinator::State
{
  network::IHttpTransport* transport{};
  library::ManagedLibrary* library{};
  mutable std::mutex mutex;
  ProviderTaskStatus status;
  network::RequestHandle request;
  std::atomic<bool> alive{true};
  std::uint64_t generation{};
};

Tone3000DownloadCoordinator::Tone3000DownloadCoordinator(network::IHttpTransport& transport,
                                                         library::ManagedLibrary& library)
  : mState(std::make_shared<State>())
{
  mState->transport = &transport; mState->library = &library;
}

Tone3000DownloadCoordinator::~Tone3000DownloadCoordinator() { Shutdown(); }

ProviderTaskStatus Tone3000DownloadCoordinator::GetStatus() const
{
  std::lock_guard lock(mState->mutex); return mState->status;
}

void Tone3000DownloadCoordinator::Begin(RemoteTone tone, RemoteModel model, std::string accessToken,
                                        DownloadCallback callback)
{
  auto state = mState;
  network::RequestHandle previous;
  std::uint64_t generation = 0;
  {
    std::lock_guard lock(state->mutex); previous = state->request; generation = ++state->generation;
    state->status = {generation, ProviderTaskState::Downloading, 0, model.declaredSize, model.name};
  }
  if (previous.value) state->transport->Cancel(previous);
  if (!state->alive.load() || tone.provider != RemoteProviderKind::Tone3000 || model.provider != RemoteProviderKind::Tone3000 ||
      tone.providerToneId.empty() || model.providerToneId != tone.providerToneId || model.providerModelId.empty() ||
      !model.downloadUrl || accessToken.empty() || (model.format != RemoteContentFormat::Nam &&
                                                     model.format != RemoteContentFormat::ImpulseResponse))
  {
    callback(Failed<DownloadResult>(ProviderErrorCode::InvalidResponse, "Selected provider model was invalid")); return;
  }
  const auto validated = network::ParseAndValidateUrl(*model.downloadUrl);
  if (!validated) { callback(Failed<DownloadResult>(ProviderErrorCode::InvalidRedirect, "Model download URL was unsafe")); return; }
  std::filesystem::path part;
  try { part = state->library->CreateProviderDownloadStagingPath(); }
  catch (...) { callback(Failed<DownloadResult>(ProviderErrorCode::StorageFailed, "Download staging was unavailable")); return; }

  network::StreamingDownloadRequest request;
  request.request.method = network::HttpMethod::Get; request.request.url = *model.downloadUrl;
  request.request.headers = {{"Accept", "application/octet-stream"}, {"Authorization", "Bearer " + std::move(accessToken)}};
  request.request.maximumRedirects = 5; request.request.category = network::HttpEndpointCategory::ModelDownload;
  request.request.idempotent = true; request.request.totalTimeout = std::chrono::minutes(10);
  request.destination = part;
  request.maximumBytes = model.format == RemoteContentFormat::Nam ? loading::kMaximumNamBytes : loading::kMaximumIrBytes;
  std::weak_ptr<State> weak = state;
  const auto handle = state->transport->BeginDownload(std::move(request),
    [weak, generation](auto, std::uint64_t bytes, std::optional<std::uint64_t> total) {
      const auto state = weak.lock(); if (!state || !state->alive.load()) return;
      std::lock_guard lock(state->mutex); if (state->generation != generation) return;
      state->status.completedBytes = bytes; state->status.totalBytes = total;
    },
    [weak, generation, part, tone = std::move(tone), model = std::move(model), callback = std::move(callback)](auto, auto result) mutable {
      const auto state = weak.lock(); if (!state || !state->alive.load()) return;
      { std::lock_guard lock(state->mutex); if (state->generation != generation) { state->library->DiscardProviderDownloadStagingPath(part); return; } }
      if (!result)
      {
        state->library->DiscardProviderDownloadStagingPath(part);
        { std::lock_guard lock(state->mutex); state->status.state = result.failure->code == ProviderErrorCode::Cancelled ? ProviderTaskState::Cancelled : ProviderTaskState::Failed; state->status.error = result.failure->code; }
        callback(Failed<DownloadResult>(result.failure->code, result.failure->safeMessage.c_str())); return;
      }
      if (result.value->statusCode < 200 || result.value->statusCode >= 300)
      {
        state->library->DiscardProviderDownloadStagingPath(part);
        callback(Failed<DownloadResult>(result.value->statusCode == 401 ? ProviderErrorCode::AuthenticationRequired : ProviderErrorCode::DownloadFailed,
                                        "Model download was not successful", result.value->statusCode)); return;
      }
      try
      {
        const auto verified = part.parent_path() /
          (model.format == RemoteContentFormat::Nam ? "verified.nam" : "verified.wav");
        std::filesystem::rename(part, verified);
        { std::lock_guard lock(state->mutex); state->status.state = ProviderTaskState::Validating; }
        if (model.format == RemoteContentFormat::Nam)
          loading::InspectRegularFile(verified, ".nam", loading::kMaximumNamBytes);
        else loading::InspectWaveFile(verified);
        { std::lock_guard lock(state->mutex); state->status.state = ProviderTaskState::Hashing; }
        const auto hash = library::HashFile(verified);
        library::ProviderAssociation association;
        association.providerKind = "tone3000"; association.providerToneId = tone.providerToneId;
        association.providerModelId = model.providerModelId; association.toneTitle = tone.title;
        association.modelTitle = model.name; association.creatorId = tone.creator.providerUserId;
        association.creatorUsername = tone.creator.username; association.licenseId = tone.licenseId;
        association.licenseName = tone.licenseName; association.sourcePageUrl = tone.sourcePageUrl;
        association.format = FormatName(model.format); association.gearRole = GearName(tone.gearRole);
        association.architecture = ArchitectureName(model.architecture);
        { std::lock_guard lock(state->mutex); state->status.state = ProviderTaskState::Promoting; }
        const auto promoted = state->library->PromoteProviderFile(verified, std::move(association));
        if (!(hash == promoted)) throw library::ImportException(library::ImportErrorCode::HashFailure, "Promoted hash changed");
        state->library->DiscardProviderDownloadStagingPath(verified);
        DownloadResult completed; completed.bytes = result.value->bytes; completed.managedObjectSha256 = library::ToHex(promoted);
        { std::lock_guard lock(state->mutex); state->status.state = ProviderTaskState::Completed; state->status.completedBytes = completed.bytes; }
        callback({std::move(completed), {}});
      }
      catch (const std::exception& error)
      {
        state->library->DiscardProviderDownloadStagingPath(part);
        { std::lock_guard lock(state->mutex); state->status.state = ProviderTaskState::Failed; state->status.error = ProviderErrorCode::IntegrityFailed; }
        auto failure = Failed<DownloadResult>(ProviderErrorCode::IntegrityFailed,
                                              "Downloaded model failed validation or promotion");
        failure.error.redactedDiagnostic = error.what();
        callback(std::move(failure));
      }
      catch (...)
      {
        state->library->DiscardProviderDownloadStagingPath(part);
        { std::lock_guard lock(state->mutex); state->status.state = ProviderTaskState::Failed; state->status.error = ProviderErrorCode::IntegrityFailed; }
        callback(Failed<DownloadResult>(ProviderErrorCode::IntegrityFailed,
                                        "Downloaded model failed validation or promotion"));
      }
    });
  { std::lock_guard lock(state->mutex); if (state->generation == generation) state->request = handle; }
}

void Tone3000DownloadCoordinator::Cancel()
{
  network::RequestHandle handle; { std::lock_guard lock(mState->mutex); handle = mState->request; }
  if (handle.value) mState->transport->Cancel(handle);
}

void Tone3000DownloadCoordinator::Shutdown()
{
  if (!mState || !mState->alive.exchange(false)) return;
  Cancel();
}

} // namespace amphibia::providers::tone3000
