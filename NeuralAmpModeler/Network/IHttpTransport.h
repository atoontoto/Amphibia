#pragma once

#include "HttpTypes.h"

namespace amphibia::network {

class IHttpTransport
{
public:
  virtual ~IHttpTransport() = default;
  virtual RequestHandle Begin(HttpRequest request, HttpCompletion completion) = 0;
  virtual RequestHandle BeginDownload(StreamingDownloadRequest request, DownloadProgress progress,
                                      DownloadCompletion completion) = 0;
  virtual void Cancel(RequestHandle) = 0;
  virtual void Shutdown() = 0;
};

} // namespace amphibia::network
