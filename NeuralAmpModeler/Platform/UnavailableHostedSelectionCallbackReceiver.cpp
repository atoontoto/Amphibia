#include "UnavailableHostedSelectionCallbackReceiver.h"

namespace amphibia::platform {

providers::ProviderError UnavailableHostedSelectionCallbackReceiver::Start(
  const std::string&, std::chrono::steady_clock::time_point, providers::HostedCallbackCompletion)
{
  return {providers::ProviderErrorCode::Unconfigured,
          "No approved TONE3000 desktop callback receiver is configured in this build."};
}

} // namespace amphibia::platform
