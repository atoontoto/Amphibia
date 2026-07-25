#pragma once

#include "IHttpTransport.h"

#include <memory>

namespace amphibia::network {

class PlatformHttpTransport final : public IHttpTransport
{
public:
  PlatformHttpTransport();
  ~PlatformHttpTransport() override;
  PlatformHttpTransport(const PlatformHttpTransport&) = delete;
  PlatformHttpTransport& operator=(const PlatformHttpTransport&) = delete;

  RequestHandle Begin(HttpRequest request, HttpCompletion completion) override;
  RequestHandle BeginDownload(StreamingDownloadRequest request, DownloadProgress progress,
                              DownloadCompletion completion) override;
  void Cancel(RequestHandle handle) override;
  void Shutdown() override;

private:
  struct Impl;
  std::unique_ptr<Impl> mImpl;
};

} // namespace amphibia::network
