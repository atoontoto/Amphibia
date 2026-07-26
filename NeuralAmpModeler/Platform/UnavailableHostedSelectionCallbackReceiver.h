#pragma once

#include "../Providers/IHostedSelectionCallbackReceiver.h"

namespace amphibia::platform {

class UnavailableHostedSelectionCallbackReceiver final : public providers::IHostedSelectionCallbackReceiver
{
public:
  providers::ProviderError Start(const std::string&, std::chrono::steady_clock::time_point,
                                 providers::HostedCallbackCompletion) override;
  void Cancel() override {}
  void Shutdown() override {}
  bool IsActive() const override { return false; }
};

} // namespace amphibia::platform
