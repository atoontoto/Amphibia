#pragma once

#include "../Providers/IHostedSelectionCallbackReceiver.h"

#include <memory>

namespace amphibia::platform {

class FixedLoopbackCallbackReceiver final : public providers::IHostedSelectionCallbackReceiver
{
public:
  FixedLoopbackCallbackReceiver();
  ~FixedLoopbackCallbackReceiver() override;

  FixedLoopbackCallbackReceiver(const FixedLoopbackCallbackReceiver&) = delete;
  FixedLoopbackCallbackReceiver& operator=(const FixedLoopbackCallbackReceiver&) = delete;

  providers::ProviderError Start(const std::string& exactRegisteredRedirect,
                                 std::chrono::steady_clock::time_point deadline,
                                 providers::HostedCallbackCompletion completion) override;
  void Cancel() override;
  void Shutdown() override;
  bool IsActive() const override;

private:
  struct State;
  std::shared_ptr<State> mState;
};

} // namespace amphibia::platform
