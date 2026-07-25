#pragma once

#include "ProviderTypes.h"

namespace amphibia::providers {

class IRemoteToneProvider
{
public:
  virtual ~IRemoteToneProvider() = default;
  virtual ProviderConnectionState GetConnectionState() const = 0;
  virtual ProviderTaskStatus GetTaskStatus() const = 0;
  virtual void BeginAuthorization(const AuthorizationRequest&, AuthorizationCallback) = 0;
  virtual void CancelAuthorization() = 0;
  virtual void Disconnect() = 0;
  virtual void FetchCurrentUser(ProviderUserCallback) = 0;
  virtual void FetchTone(const std::string& toneId, ToneCallback) = 0;
  virtual void FetchModelsForTone(const std::string& toneId, ModelListCallback) = 0;
  virtual void BeginModelDownload(const RemoteModel&, const DownloadDestination&, DownloadCallback) = 0;
  virtual void Shutdown() = 0;
};

} // namespace amphibia::providers
