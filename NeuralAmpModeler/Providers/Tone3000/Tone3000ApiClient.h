#pragma once

#include "../../Network/IHttpTransport.h"
#include "Tone3000Configuration.h"
#include "Tone3000Parsing.h"

#include <atomic>
#include <memory>

namespace amphibia::providers::tone3000 {

using TokenCallback = std::function<void(ProviderResult<credentials::OAuthTokenSet>)>;
class Tone3000ApiClient final
{
public:
  Tone3000ApiClient(Tone3000Configuration configuration, network::IHttpTransport& transport);
  ~Tone3000ApiClient();

  Tone3000ApiClient(const Tone3000ApiClient&) = delete;
  Tone3000ApiClient& operator=(const Tone3000ApiClient&) = delete;

  network::RequestHandle ExchangeAuthorizationCode(std::string code, std::string verifier,
                                                    TokenCallback callback);
  network::RequestHandle Refresh(std::string refreshToken, TokenCallback callback);
  network::RequestHandle FetchCurrentUser(std::string accessToken, ProviderUserCallback callback);
  network::RequestHandle FetchTone(std::string accessToken, std::string toneId, ToneCallback callback);
  network::RequestHandle FetchModels(std::string accessToken, std::string toneId,
                                     RemoteContentFormat format, ModelPageCallback callback,
                                     std::size_t page = 1, std::size_t pageSize = 300,
                                     std::optional<RemoteNamArchitecture> architecture = std::nullopt);
  network::RequestHandle FetchModel(std::string accessToken, std::string modelId,
                                    RemoteContentFormat format, ModelCallback callback);
  void Shutdown();

private:
  struct State;
  std::shared_ptr<State> mState;
};

} // namespace amphibia::providers::tone3000
