#pragma once

#include "../../Library/ManagedLibrary.h"
#include "../../Network/IHttpTransport.h"
#include "../ProviderTypes.h"

#include <memory>

namespace amphibia::providers::tone3000 {

class Tone3000DownloadCoordinator final
{
public:
  Tone3000DownloadCoordinator(network::IHttpTransport& transport, library::ManagedLibrary& library);
  ~Tone3000DownloadCoordinator();
  ProviderTaskStatus GetStatus() const;
  void Begin(RemoteTone tone, RemoteModel model, std::string accessToken, DownloadCallback callback);
  void Cancel();
  void Shutdown();

private:
  struct State;
  std::shared_ptr<State> mState;
};

} // namespace amphibia::providers::tone3000
