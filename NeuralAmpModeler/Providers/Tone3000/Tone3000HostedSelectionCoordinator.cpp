#include "Tone3000HostedSelectionCoordinator.h"

#include "HostedSelectionLease.h"

#include <algorithm>
#include <cctype>
#include <mutex>
#include <unordered_set>

namespace amphibia::providers::tone3000 {
namespace {

constexpr auto kSelectionTimeout = std::chrono::minutes(5);

ProviderError Failure(ProviderErrorCode code, const char* message)
{
  return {code, message};
}

RemoteContentFormat FormatFor(HostedSelectionKind kind)
{
  return kind == HostedSelectionKind::ImpulseResponse ? RemoteContentFormat::ImpulseResponse :
                                                        RemoteContentFormat::Nam;
}

AuthorizationRequest RequestFor(HostedSelectionKind kind)
{
  AuthorizationRequest request;
  request.purpose = AuthorizationPurpose::SelectTone;
  request.format = FormatFor(kind);
  if (kind == HostedSelectionKind::ImpulseResponse)
    request.gearFilters = {"cab"};
  else
  {
    request.gearFilters = {"amp", "amp-cab"};
    if (kind == HostedSelectionKind::NamA2) request.architecture = RemoteNamArchitecture::A2;
  }
  return request;
}

bool ValidId(const std::string& value)
{
  return !value.empty() && value.size() <= 128 &&
         std::none_of(value.begin(), value.end(), [](unsigned char c) { return c < 0x20 || c == 0x7f; });
}

VariantCompatibility Compatibility(const RemoteModel& model, const std::string& toneId,
                                   HostedSelectionKind kind, std::string& message)
{
  if (!ValidId(model.providerModelId) || !ValidId(model.providerToneId) || model.name.empty() ||
      model.name.size() > 1024)
  {
    message = "Invalid provider model identity";
    return VariantCompatibility::InvalidIdentity;
  }
  if (model.providerToneId != toneId)
  {
    message = "Variant belongs to a different tone";
    return VariantCompatibility::WrongTone;
  }
  if (model.format != FormatFor(kind))
  {
    message = "Unavailable format in this build";
    return VariantCompatibility::WrongFormat;
  }
  if (kind == HostedSelectionKind::NamA1AndCustom && model.architecture != RemoteNamArchitecture::A1)
  {
    message = "Unsupported or unknown architecture";
    return VariantCompatibility::UnsupportedArchitecture;
  }
  if (kind == HostedSelectionKind::NamA2 && model.architecture != RemoteNamArchitecture::A2)
  {
    message = "Unsupported or unknown architecture";
    return VariantCompatibility::UnsupportedArchitecture;
  }
  message = "Compatible";
  return VariantCompatibility::Compatible;
}

std::string LowerAscii(std::string value)
{
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return c < 0x80 ? static_cast<char>(std::tolower(c)) : static_cast<char>(c);
  });
  return value;
}

} // namespace

struct Tone3000HostedSelectionCoordinator::State : public std::enable_shared_from_this<State>
{
  Tone3000Configuration configuration;
  Tone3000ConfigurationValidation registration;
  IRemoteToneProvider* provider{};
  IHostedSelectionCallbackReceiver* receiver{};
  DownloadedLookup downloadedLookup;
  ActivationRequest activationRequest;
  DownloadedRemoval downloadedRemoval;
  mutable std::mutex mutex;
  HostedSelectionSnapshot snapshot;
  std::uint64_t generation{};
  bool alive{true};
  bool auditionAfterDownload{};
  std::unique_ptr<HostedSelectionLease> selectionLease;

  bool IsCurrent(std::uint64_t task) const
  {
    std::lock_guard lock(mutex);
    return alive && snapshot.taskId == task && snapshot.stage != HostedSelectionStage::Cancelled &&
           snapshot.stage != HostedSelectionStage::Failed;
  }

  void Fail(std::uint64_t task, ProviderError error)
  {
    std::lock_guard lock(mutex);
    if (!alive || snapshot.taskId != task || snapshot.stage == HostedSelectionStage::Cancelled) return;
    snapshot.stage = error.code == ProviderErrorCode::Cancelled ? HostedSelectionStage::Cancelled :
                                                                  HostedSelectionStage::Failed;
    snapshot.statusText = error.userMessage;
    snapshot.error = std::move(error);
    selectionLease.reset();
  }

  void AuthorizationComplete(std::uint64_t task, ProviderResult<AuthorizationResult> result)
  {
    receiver->Cancel();
    if (!IsCurrent(task)) return;
    if (!result)
    {
      Fail(task, std::move(result.error));
      return;
    }
    if (result.value->cancelled)
    {
      Fail(task, Failure(ProviderErrorCode::Cancelled, "The TONE3000 selection was cancelled."));
      return;
    }
    if (!result.value->selectedToneId || !ValidId(*result.value->selectedToneId))
    {
      Fail(task, Failure(ProviderErrorCode::InvalidResponse, "The TONE3000 selection did not return a valid tone."));
      return;
    }
    const auto toneId = *result.value->selectedToneId;
    {
      std::lock_guard lock(mutex);
      if (!alive || snapshot.taskId != task) return;
      snapshot.stage = HostedSelectionStage::FetchingTone;
      snapshot.statusText = "Loading exact tone details...";
    }
    HostedSelectionKind kind;
    {
      std::lock_guard lock(mutex);
      if (!alive || snapshot.taskId != task) return;
      kind = snapshot.kind;
    }
    std::weak_ptr<State> weak = shared_from_this();
    provider->FetchTone(toneId, [weak, task, toneId, kind](ProviderResult<RemoteTone> tone) mutable {
      const auto state = weak.lock();
      if (!state || !state->IsCurrent(task)) return;
      if (!tone)
      {
        state->Fail(task, std::move(tone.error));
        return;
      }
      if (tone.value->providerToneId != toneId || tone.value->format != FormatFor(kind))
      {
        state->Fail(task, Failure(ProviderErrorCode::UnsupportedContent,
                                  "The selected tone format is not compatible with this entry point."));
        return;
      }
      {
        std::lock_guard lock(state->mutex);
        if (!state->alive || state->snapshot.taskId != task) return;
        state->snapshot.tone = *tone.value;
        state->snapshot.stage = HostedSelectionStage::FetchingVariants;
        state->snapshot.statusText = "Loading exact variants...";
      }
      const auto architecture = kind == HostedSelectionKind::NamA2 ?
        std::optional<RemoteNamArchitecture>(RemoteNamArchitecture::A2) : std::nullopt;
      state->provider->FetchModelsForTone(toneId, FormatFor(kind), architecture,
        [weak, task, toneId, kind](ProviderResult<std::vector<RemoteModel>> models) mutable {
          const auto state = weak.lock();
          if (!state || !state->IsCurrent(task)) return;
          if (!models)
          {
            state->Fail(task, std::move(models.error));
            return;
          }
          std::unordered_set<std::string> ids;
          std::vector<HostedVariant> variants;
          variants.reserve(models.value->size());
          for (auto& model : *models.value)
          {
            if (!ids.insert(model.providerModelId).second)
            {
              state->Fail(task, Failure(ProviderErrorCode::InvalidResponse,
                                        "TONE3000 returned a duplicate exact model identity."));
              return;
            }
            HostedVariant variant;
            variant.model = std::move(model);
            variant.compatibility = Compatibility(variant.model, toneId, kind, variant.compatibilityMessage);
            if (state->downloadedLookup)
              if (auto hash = state->downloadedLookup(variant.model))
              {
                variant.managedObjectSha256 = std::move(hash);
                variant.downloaded = true;
              }
            variants.push_back(std::move(variant));
          }
          std::stable_sort(variants.begin(), variants.end(), [](const auto& left, const auto& right) {
            const auto leftPath = left.model.relativePath.value_or("");
            const auto rightPath = right.model.relativePath.value_or("");
            if (leftPath != rightPath) return leftPath < rightPath;
            if (left.model.name != right.model.name) return left.model.name < right.model.name;
            return left.model.providerModelId < right.model.providerModelId;
          });
          std::lock_guard lock(state->mutex);
          if (!state->alive || state->snapshot.taskId != task) return;
          state->snapshot.variants = std::move(variants);
          state->snapshot.selectedVariant = state->snapshot.variants.size() == 1 ?
            std::optional<std::size_t>(0) : std::nullopt;
          state->snapshot.stage = HostedSelectionStage::Ready;
          const auto compatible = std::count_if(state->snapshot.variants.begin(), state->snapshot.variants.end(),
            [](const auto& item) { return item.compatibility == VariantCompatibility::Compatible; });
          state->snapshot.statusText = std::to_string(compatible) + " compatible variant(s), " +
            std::to_string(state->snapshot.variants.size() - compatible) + " unavailable in this build";
          state->selectionLease.reset();
        });
    });
  }
};

Tone3000HostedSelectionCoordinator::Tone3000HostedSelectionCoordinator(
  Tone3000Configuration configuration, IRemoteToneProvider& provider,
  IHostedSelectionCallbackReceiver& receiver, DownloadedLookup downloadedLookup,
  ActivationRequest activationRequest, DownloadedRemoval downloadedRemoval)
  : mState(std::make_shared<State>())
{
  mState->registration = ValidateTone3000Configuration(configuration);
  mState->configuration = std::move(configuration);
  mState->provider = &provider;
  mState->receiver = &receiver;
  mState->downloadedLookup = std::move(downloadedLookup);
  mState->activationRequest = std::move(activationRequest);
  mState->downloadedRemoval = std::move(downloadedRemoval);
}

Tone3000HostedSelectionCoordinator::~Tone3000HostedSelectionCoordinator() { Shutdown(); }

ProviderError Tone3000HostedSelectionCoordinator::Begin(HostedSelectionKind kind)
{
  auto state = mState;
  if (!state->registration.valid || !state->registration.providerUsable)
    return Failure(ProviderErrorCode::Unconfigured, state->registration.userMessage.c_str());
  std::uint64_t task;
  {
    std::lock_guard lock(state->mutex);
    if (!state->alive) return Failure(ProviderErrorCode::Cancelled, "TONE3000 selection is shutting down.");
    const auto active = state->snapshot.stage >= HostedSelectionStage::WaitingForBrowser &&
                        state->snapshot.stage <= HostedSelectionStage::Preparing;
    if (active) return Failure(ProviderErrorCode::Superseded, "A TONE3000 operation is already active.");
    state->selectionLease = std::make_unique<HostedSelectionLease>(
      state->configuration.credentialServiceNamespace,
      state->configuration.credentialAccount + ".hosted-selection");
    if (!state->selectionLease->IsHeld())
    {
      state->selectionLease.reset();
      return Failure(ProviderErrorCode::Superseded,
                     "Another Amphibia instance is completing a TONE3000 selection.");
    }
    task = ++state->generation;
    state->snapshot = {};
    state->snapshot.taskId = task;
    state->snapshot.kind = kind;
    state->snapshot.stage = HostedSelectionStage::WaitingForBrowser;
    state->snapshot.expiresAt = std::chrono::system_clock::now() + kSelectionTimeout;
    state->snapshot.statusText = "Waiting for TONE3000 selection... Complete the selection in your browser.";
  }
  const auto deadline = std::chrono::steady_clock::now() + kSelectionTimeout;
  std::weak_ptr<State> weak = state;
  if (auto error = state->receiver->Start(state->configuration.redirectUri, deadline,
      [weak, task](ProviderResult<std::string> callback) mutable {
        const auto state = weak.lock();
        if (!state || !state->IsCurrent(task)) return;
        if (!callback)
        {
          state->Fail(task, std::move(callback.error));
          state->provider->CancelAuthorization();
          return;
        }
        state->provider->CompleteAuthorizationCallback(*callback.value);
      }))
  {
    state->Fail(task, error);
    return error;
  }
  state->provider->BeginAuthorization(RequestFor(kind),
    [weak, task](ProviderResult<AuthorizationResult> authorization) mutable {
      if (const auto state = weak.lock()) state->AuthorizationComplete(task, std::move(authorization));
    });
  return {};
}

ProviderError Tone3000HostedSelectionCoordinator::ReopenBrowser()
{
  const auto snapshot = Snapshot();
  if (snapshot.stage != HostedSelectionStage::WaitingForBrowser)
    return Failure(ProviderErrorCode::InvalidResponse, "No TONE3000 browser selection is waiting.");
  return mState->provider->ReopenAuthorizationBrowser();
}

void Tone3000HostedSelectionCoordinator::Cancel()
{
  auto state = mState;
  state->receiver->Cancel();
  state->provider->CancelCurrentOperation();
  std::lock_guard lock(state->mutex);
  if (!state->alive || state->snapshot.stage == HostedSelectionStage::Idle) return;
  state->snapshot.stage = HostedSelectionStage::Cancelled;
  state->snapshot.statusText = "The TONE3000 operation was cancelled.";
  state->snapshot.error = Failure(ProviderErrorCode::Cancelled, state->snapshot.statusText.c_str());
  state->selectionLease.reset();
}

void Tone3000HostedSelectionCoordinator::Shutdown()
{
  if (!mState) return;
  {
    std::lock_guard lock(mState->mutex);
    if (!mState->alive) return;
    mState->alive = false;
    mState->selectionLease.reset();
  }
  mState->receiver->Shutdown();
  mState->provider->Shutdown();
}

HostedSelectionSnapshot Tone3000HostedSelectionCoordinator::Snapshot() const
{
  std::lock_guard lock(mState->mutex);
  return mState->snapshot;
}

ProviderError Tone3000HostedSelectionCoordinator::SelectVariant(std::size_t index)
{
  std::lock_guard lock(mState->mutex);
  if (mState->snapshot.stage != HostedSelectionStage::Ready &&
      mState->snapshot.stage != HostedSelectionStage::Downloaded &&
      mState->snapshot.stage != HostedSelectionStage::Active)
    return Failure(ProviderErrorCode::InvalidResponse, "Variants are not ready for selection.");
  if (index >= mState->snapshot.variants.size())
    return Failure(ProviderErrorCode::InvalidResponse, "The selected variant does not exist.");
  mState->snapshot.selectedVariant = index;
  return {};
}

ProviderError Tone3000HostedSelectionCoordinator::SelectAdjacentDownloaded(int direction)
{
  std::lock_guard lock(mState->mutex);
  const auto& variants = mState->snapshot.variants;
  if (variants.empty() || direction == 0)
    return Failure(ProviderErrorCode::InvalidResponse, "No downloaded variant is available.");
  auto index = mState->snapshot.selectedVariant.value_or(direction > 0 ? variants.size() - 1 : 0);
  for (std::size_t attempt = 0; attempt < variants.size(); ++attempt)
  {
    index = direction > 0 ? (index + 1) % variants.size() : (index + variants.size() - 1) % variants.size();
    const auto& candidate = variants[index];
    if (candidate.downloaded && candidate.compatibility == VariantCompatibility::Compatible)
    {
      mState->snapshot.selectedVariant = index;
      return {};
    }
  }
  return Failure(ProviderErrorCode::NotFound, "No other downloaded compatible variant is available.");
}

ProviderError Tone3000HostedSelectionCoordinator::DownloadSelected(bool auditionAfterDownload)
{
  auto state = mState;
  RemoteModel selected;
  HostedSelectionKind kind;
  std::uint64_t task;
  ActivationRequest immediateActivation;
  std::string immediateHash;
  {
    std::lock_guard lock(state->mutex);
    if (state->snapshot.stage != HostedSelectionStage::Ready &&
        state->snapshot.stage != HostedSelectionStage::Downloaded &&
        state->snapshot.stage != HostedSelectionStage::Active)
      return Failure(ProviderErrorCode::InvalidResponse, "Variants are not ready for download.");
    if (!state->snapshot.selectedVariant || *state->snapshot.selectedVariant >= state->snapshot.variants.size())
      return Failure(ProviderErrorCode::InvalidResponse, "Choose one exact variant first.");
    const auto& variant = state->snapshot.variants[*state->snapshot.selectedVariant];
    if (variant.compatibility != VariantCompatibility::Compatible)
      return Failure(ProviderErrorCode::UnsupportedContent, variant.compatibilityMessage.c_str());
    task = state->snapshot.taskId;
    kind = state->snapshot.kind;
    selected = variant.model;
    state->auditionAfterDownload = auditionAfterDownload;
    if (variant.downloaded && variant.managedObjectSha256)
    {
      if (auditionAfterDownload && state->activationRequest)
      {
        state->snapshot.stage = HostedSelectionStage::Preparing;
        state->snapshot.statusText = "Preparing...";
        immediateHash = *variant.managedObjectSha256;
        immediateActivation = state->activationRequest;
      }
      else
      {
        state->snapshot.stage = HostedSelectionStage::Downloaded;
        state->snapshot.statusText = "Already in library";
      }
      if (!immediateActivation) return {};
    }
    else
    {
      state->snapshot.stage = HostedSelectionStage::RefreshingVariant;
      state->snapshot.statusText = "Refreshing exact variant details...";
    }
  }
  if (immediateActivation)
  {
    immediateActivation(task, immediateHash, kind);
    return {};
  }
  std::weak_ptr<State> weak = state;
  state->provider->FetchExactModel(selected.providerModelId, FormatFor(kind),
    [weak, task, selected, kind](ProviderResult<RemoteModel> refreshed) mutable {
      const auto state = weak.lock();
      if (!state || !state->IsCurrent(task)) return;
      if (!refreshed)
      {
        state->Fail(task, std::move(refreshed.error));
        return;
      }
      std::string compatibility;
      const auto compatible = Compatibility(*refreshed.value, selected.providerToneId, kind, compatibility);
      if (refreshed.value->providerModelId != selected.providerModelId || compatible != VariantCompatibility::Compatible)
      {
        state->Fail(task, Failure(ProviderErrorCode::IntegrityFailed,
                                  "The exact provider variant changed before download."));
        return;
      }
      {
        std::lock_guard lock(state->mutex);
        if (!state->alive || state->snapshot.taskId != task) return;
        state->snapshot.stage = HostedSelectionStage::Downloading;
        state->snapshot.statusText = "Downloading exact variant...";
      }
      state->provider->BeginModelDownload(*refreshed.value, {},
        [weak, task, modelId = selected.providerModelId, kind](ProviderResult<DownloadResult> download) mutable {
          const auto state = weak.lock();
          if (!state || !state->IsCurrent(task)) return;
          if (!download)
          {
            state->Fail(task, std::move(download.error));
            return;
          }
          ActivationRequest activation;
          bool audition = false;
          std::string hash;
          {
            std::lock_guard lock(state->mutex);
            if (!state->alive || state->snapshot.taskId != task) return;
            const auto found = std::find_if(state->snapshot.variants.begin(), state->snapshot.variants.end(),
              [&](const auto& item) { return item.model.providerModelId == modelId; });
            if (found == state->snapshot.variants.end()) return;
            found->downloaded = true;
            found->managedObjectSha256 = download.value->managedObjectSha256;
            hash = download.value->managedObjectSha256;
            audition = state->auditionAfterDownload && static_cast<bool>(state->activationRequest);
            if (audition)
            {
              state->snapshot.stage = HostedSelectionStage::Preparing;
              state->snapshot.statusText = "Downloaded. Preparing...";
              activation = state->activationRequest;
            }
            else
            {
              state->snapshot.stage = HostedSelectionStage::Downloaded;
              state->snapshot.statusText = "Downloaded";
            }
          }
          if (audition) activation(task, hash, kind);
        });
    });
  return {};
}

ProviderError Tone3000HostedSelectionCoordinator::RemoveSelectedLocalCopy()
{
  std::lock_guard lock(mState->mutex);
  if (!mState->snapshot.selectedVariant ||
      *mState->snapshot.selectedVariant >= mState->snapshot.variants.size())
    return Failure(ProviderErrorCode::InvalidResponse, "Choose one exact variant first.");
  auto& variant = mState->snapshot.variants[*mState->snapshot.selectedVariant];
  if (!variant.downloaded || !variant.managedObjectSha256)
    return Failure(ProviderErrorCode::NotFound, "This exact variant has no local copy.");
  if (variant.active)
    return Failure(ProviderErrorCode::Forbidden, "The active variant cannot be removed.");
  if (!mState->downloadedRemoval)
    return Failure(ProviderErrorCode::StorageFailed, "Local removal is unavailable.");
  if (auto error = mState->downloadedRemoval(variant.model, *variant.managedObjectSha256)) return error;
  variant.downloaded = false;
  variant.managedObjectSha256.reset();
  mState->snapshot.stage = HostedSelectionStage::Ready;
  mState->snapshot.statusText = "Local copy removed; the hosted variant remains available.";
  return {};
}

void Tone3000HostedSelectionCoordinator::NotifyActivation(std::uint64_t taskId,
                                                           const std::string& providerModelId,
                                                           bool active,
                                                           std::string failureMessage)
{
  std::lock_guard lock(mState->mutex);
  if (!mState->alive || mState->snapshot.taskId != taskId ||
      mState->snapshot.stage != HostedSelectionStage::Preparing) return;
  const auto found = std::find_if(mState->snapshot.variants.begin(), mState->snapshot.variants.end(),
    [&](const auto& item) { return item.model.providerModelId == providerModelId; });
  if (found == mState->snapshot.variants.end()) return;
  if (active)
  {
    for (auto& variant : mState->snapshot.variants) variant.active = false;
    found->active = true;
    mState->snapshot.stage = HostedSelectionStage::Active;
    mState->snapshot.statusText = "Active";
    mState->snapshot.error.reset();
  }
  else
  {
    mState->snapshot.stage = HostedSelectionStage::Downloaded;
    mState->snapshot.statusText = failureMessage.empty() ?
      "The variant downloaded successfully but could not be prepared." : std::move(failureMessage);
    mState->snapshot.error = Failure(ProviderErrorCode::UnsupportedContent, mState->snapshot.statusText.c_str());
  }
}

std::vector<std::size_t> Tone3000HostedSelectionCoordinator::FilteredVariantIndices(
  const HostedSelectionSnapshot& snapshot, std::string filter, std::size_t first, std::size_t maximum)
{
  if (filter.size() > 256) filter.resize(256);
  filter = LowerAscii(std::move(filter));
  maximum = (std::min)(maximum, std::size_t{64});
  std::vector<std::size_t> result;
  std::size_t skipped = 0;
  for (std::size_t index = 0; index < snapshot.variants.size() && result.size() < maximum; ++index)
  {
    const auto& variant = snapshot.variants[index];
    const auto searchable = LowerAscii(variant.model.name + " " + variant.model.relativePath.value_or(""));
    if (!filter.empty() && searchable.find(filter) == std::string::npos) continue;
    if (skipped++ < first) continue;
    result.push_back(index);
  }
  return result;
}

std::string Tone3000HostedSelectionCoordinator::GroupLabel(const HostedVariant& variant)
{
  if (variant.model.relativePath)
  {
    const auto slash = variant.model.relativePath->find_last_of("/\\");
    if (slash != std::string::npos && slash != 0)
      return variant.model.relativePath->substr(0, (std::min)(slash, std::size_t{256}));
  }
  if (variant.model.format == RemoteContentFormat::ImpulseResponse) return "Impulse responses";
  if (variant.model.architecture == RemoteNamArchitecture::A1) return "NAM A1";
  if (variant.model.architecture == RemoteNamArchitecture::A2) return "NAM A2";
  return "Other variants";
}

} // namespace amphibia::providers::tone3000
