#pragma once

#include "ProviderTypes.h"

#include <chrono>
#include <functional>
#include <string>

namespace amphibia::providers {

using HostedCallbackCompletion = std::function<void(ProviderResult<std::string>)>;

class IHostedSelectionCallbackReceiver
{
public:
  virtual ~IHostedSelectionCallbackReceiver() = default;
  virtual ProviderError Start(const std::string& exactRegisteredRedirect,
                              std::chrono::steady_clock::time_point deadline,
                              HostedCallbackCompletion completion) = 0;
  virtual void Cancel() = 0;
  virtual void Shutdown() = 0;
  virtual bool IsActive() const = 0;
};

} // namespace amphibia::providers
