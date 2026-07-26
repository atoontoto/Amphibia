#pragma once

#include "../../Credentials/CredentialRefreshLock.h"
#include "../../Credentials/ISecureCredentialStore.h"
#include "../../Platform/SystemBrowser.h"
#include "../IRemoteToneProvider.h"
#include "OAuthController.h"
#include "Tone3000ApiClient.h"
#include "Tone3000DownloadCoordinator.h"

namespace amphibia::providers::tone3000 {

class Tone3000Provider final : public IRemoteToneProvider
{
public:
  Tone3000Provider(Tone3000Configuration configuration, platform::ISystemBrowser& browser,
                   credentials::ISecureCredentialStore& credentials, network::IHttpTransport& transport,
                   library::ManagedLibrary& library);
  ~Tone3000Provider() override;

  ProviderConnectionState GetConnectionState() const override;
  ProviderTaskStatus GetTaskStatus() const override;
  void BeginAuthorization(const AuthorizationRequest&, AuthorizationCallback) override;
  void CancelAuthorization() override;
  void Disconnect() override;
  void FetchCurrentUser(ProviderUserCallback) override;
  void FetchTone(const std::string& toneId, ToneCallback) override;
  void FetchModelsForTone(const std::string& toneId, RemoteContentFormat,
                          std::optional<RemoteNamArchitecture>, ModelListCallback) override;
  void FetchExactModel(const std::string& modelId, RemoteContentFormat, ModelCallback) override;
  void BeginModelDownload(const RemoteModel&, const DownloadDestination&, DownloadCallback) override;
  void CompleteAuthorizationCallback(const std::string& callbackUri) override;
  ProviderError ReopenAuthorizationBrowser() override;
  void CancelCurrentOperation() override;
  void Shutdown() override;

private:
  struct State;
  std::shared_ptr<State> mState;
};

} // namespace amphibia::providers::tone3000
