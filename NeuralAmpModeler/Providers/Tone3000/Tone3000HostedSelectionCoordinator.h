#pragma once

#include "../IHostedSelectionCallbackReceiver.h"
#include "../IRemoteToneProvider.h"
#include "Tone3000Configuration.h"

#include <functional>
#include <memory>

namespace amphibia::providers::tone3000 {

class Tone3000HostedSelectionCoordinator final
{
public:
  using DownloadedLookup = std::function<std::optional<std::string>(const RemoteModel&)>;
  using DownloadedRemoval = std::function<ProviderError(const RemoteModel&, const std::string& managedSha256)>;
  using ActivationRequest =
    std::function<void(std::uint64_t taskId, const std::string& managedSha256, HostedSelectionKind)>;

  Tone3000HostedSelectionCoordinator(Tone3000Configuration configuration,
                                     IRemoteToneProvider& provider,
                                     IHostedSelectionCallbackReceiver& receiver,
                                     DownloadedLookup downloadedLookup = {},
                                     ActivationRequest activationRequest = {},
                                     DownloadedRemoval downloadedRemoval = {});
  ~Tone3000HostedSelectionCoordinator();

  Tone3000HostedSelectionCoordinator(const Tone3000HostedSelectionCoordinator&) = delete;
  Tone3000HostedSelectionCoordinator& operator=(const Tone3000HostedSelectionCoordinator&) = delete;

  ProviderError Begin(HostedSelectionKind kind);
  ProviderError ReopenBrowser();
  void Cancel();
  void Shutdown();
  HostedSelectionSnapshot Snapshot() const;
  ProviderError SelectVariant(std::size_t index);
  ProviderError SelectAdjacentDownloaded(int direction);
  ProviderError DownloadSelected(bool auditionAfterDownload);
  ProviderError RemoveSelectedLocalCopy();
  void NotifyActivation(std::uint64_t taskId, const std::string& providerModelId,
                        bool active, std::string failureMessage = {});

  static std::vector<std::size_t> FilteredVariantIndices(const HostedSelectionSnapshot& snapshot,
                                                          std::string filter,
                                                          std::size_t first = 0,
                                                          std::size_t maximum = 12);
  static std::string GroupLabel(const HostedVariant& variant);

private:
  struct State;
  std::shared_ptr<State> mState;
};

} // namespace amphibia::providers::tone3000
