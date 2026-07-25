#include "Tone3000Provider.h"

#include <mutex>

namespace amphibia::providers::tone3000 {
namespace {

template <typename T> ProviderResult<T> Failed(ProviderErrorCode code, const char* message)
{
  ProviderResult<T> result; result.error = {code, message}; return result;
}

} // namespace

struct Tone3000Provider::State : public std::enable_shared_from_this<State>
{
  Tone3000Configuration configuration;
  platform::ISystemBrowser* browser{};
  credentials::ISecureCredentialStore* credentialStore{};
  credentials::CredentialKey credentialKey;
  std::unique_ptr<OAuthController> oauth;
  std::unique_ptr<Tone3000ApiClient> api;
  std::unique_ptr<Tone3000DownloadCoordinator> downloads;
  mutable std::mutex mutex;
  ProviderConnectionState connection{ProviderConnectionState::Unconfigured};
  ProviderTaskStatus status;
  AuthorizationCallback authorizationCallback;
  std::atomic<bool> alive{true};
  std::uint64_t nextTask{};

  using AccessCallback = std::function<void(ProviderResult<std::string>)>;

  void SetStatus(ProviderTaskState taskState, std::optional<ProviderErrorCode> error = std::nullopt)
  {
    std::lock_guard lock(mutex); status = {++nextTask, taskState}; status.error = error;
  }

  void AccessToken(AccessCallback callback)
  {
    credentials::SecureCredential loaded;
    const auto loadedResult = credentialStore->Load(credentialKey, loaded);
    if (!loadedResult)
    {
      { std::lock_guard lock(mutex); connection = ProviderConnectionState::Disconnected; }
      callback(Failed<std::string>(ProviderErrorCode::AuthenticationRequired, "TONE3000 account is not connected")); return;
    }
    if (loaded.tokens.accessTokenExpiry > std::chrono::system_clock::now() + std::chrono::minutes(1))
    {
      { std::lock_guard lock(mutex); connection = ProviderConnectionState::Connected; }
      callback({std::move(loaded.tokens.accessToken), {}}); return;
    }
    if (!loaded.tokens.refreshToken)
    {
      credentialStore->Remove(credentialKey);
      callback(Failed<std::string>(ProviderErrorCode::AuthenticationRequired, "TONE3000 session expired")); return;
    }
    auto refreshLock = std::make_shared<credentials::CredentialRefreshLock>(credentialKey, std::chrono::seconds(5));
    if (!refreshLock->IsLocked())
    { callback(Failed<std::string>(ProviderErrorCode::TokenRefreshFailed, "TONE3000 credential refresh is busy")); return; }

    credentials::SecureCredential current;
    if (!credentialStore->Load(credentialKey, current))
    { callback(Failed<std::string>(ProviderErrorCode::AuthenticationRequired, "TONE3000 session is unavailable")); return; }
    if (current.tokens.accessTokenExpiry > std::chrono::system_clock::now() + std::chrono::minutes(1))
    { callback({std::move(current.tokens.accessToken), {}}); return; }
    if (!current.tokens.refreshToken)
    { callback(Failed<std::string>(ProviderErrorCode::AuthenticationRequired, "TONE3000 refresh token is unavailable")); return; }
    const auto generation = current.tokens.rotationGeneration;
    auto providerUserId = current.providerUserId; auto username = current.username;
    { std::lock_guard lock(mutex); connection = ProviderConnectionState::Refreshing; status = {++nextTask, ProviderTaskState::RefreshingToken}; }
    std::weak_ptr<State> weak = shared_from_this();
    api->Refresh(std::move(*current.tokens.refreshToken),
      [weak, refreshLock, generation, providerUserId = std::move(providerUserId), username = std::move(username),
       callback = std::move(callback)](auto result) mutable {
        const auto state = weak.lock(); if (!state || !state->alive.load()) return;
        if (!result || !result.value->refreshToken)
        {
          if (result.error.code == ProviderErrorCode::InvalidGrant) state->credentialStore->Remove(state->credentialKey);
          { std::lock_guard lock(state->mutex); state->connection = ProviderConnectionState::Expired; state->status.state = ProviderTaskState::Failed; state->status.error = result.error.code; }
          callback(Failed<std::string>(result.error.code == ProviderErrorCode::None ? ProviderErrorCode::TokenRefreshFailed : result.error.code,
                                       "TONE3000 token refresh failed")); return;
        }
        credentials::SecureCredential replacement;
        replacement.tokens = std::move(*result.value); replacement.tokens.rotationGeneration = generation + 1;
        replacement.providerUserId = std::move(providerUserId); replacement.username = std::move(username);
        replacement.verifiedAt = library::UnixTimeNow();
        const auto access = replacement.tokens.accessToken;
        if (!state->credentialStore->Store(state->credentialKey, replacement))
        { callback(Failed<std::string>(ProviderErrorCode::StorageFailed, "Refreshed credential could not be stored")); return; }
        { std::lock_guard lock(state->mutex); state->connection = ProviderConnectionState::Connected; state->status.state = ProviderTaskState::Completed; }
        callback({access, {}});
      });
  }
};

Tone3000Provider::Tone3000Provider(Tone3000Configuration configuration, platform::ISystemBrowser& browser,
                                   credentials::ISecureCredentialStore& credentials,
                                   network::IHttpTransport& transport, library::ManagedLibrary& library)
  : mState(std::make_shared<State>())
{
  mState->configuration = std::move(configuration); mState->browser = &browser; mState->credentialStore = &credentials;
  mState->connection = mState->configuration.HasPublishableConfiguration() ? ProviderConnectionState::Disconnected
                                                                            : ProviderConnectionState::Unconfigured;
  mState->oauth = std::make_unique<OAuthController>(mState->configuration);
  mState->api = std::make_unique<Tone3000ApiClient>(mState->configuration, transport);
  mState->downloads = std::make_unique<Tone3000DownloadCoordinator>(transport, library);
}

Tone3000Provider::~Tone3000Provider() { Shutdown(); }

ProviderConnectionState Tone3000Provider::GetConnectionState() const
{ std::lock_guard lock(mState->mutex); return mState->connection; }

ProviderTaskStatus Tone3000Provider::GetTaskStatus() const
{
  const auto download = mState->downloads->GetStatus();
  if (download.state != ProviderTaskState::Idle) return download;
  std::lock_guard lock(mState->mutex); return mState->status;
}

void Tone3000Provider::BeginAuthorization(const AuthorizationRequest& request, AuthorizationCallback callback)
{
  auto state = mState;
  if (!state->configuration.HasPublishableConfiguration())
  { callback(Failed<AuthorizationResult>(ProviderErrorCode::Unconfigured, "TONE3000 provider registration is not configured")); return; }
  auto start = state->oauth->Begin(request);
  if (!start) { callback(Failed<AuthorizationResult>(start.error.code, start.error.userMessage.c_str())); return; }
  {
    std::lock_guard lock(state->mutex); state->authorizationCallback = std::move(callback);
    state->connection = ProviderConnectionState::Authorizing;
    state->status = {start.value->flowId, ProviderTaskState::LaunchingBrowser};
  }
  if (const auto error = state->browser->OpenExplicit(start.value->authorizationUrl))
  {
    state->oauth->Cancel(); AuthorizationCallback failed;
    { std::lock_guard lock(state->mutex); failed = std::move(state->authorizationCallback); state->connection = ProviderConnectionState::Error; state->status.state = ProviderTaskState::Failed; state->status.error = error.code; }
    if (failed) failed(Failed<AuthorizationResult>(error.code, error.userMessage.c_str()));
  }
  else { std::lock_guard lock(state->mutex); state->status.state = ProviderTaskState::AwaitingAuthorization; }
}

void Tone3000Provider::CompleteAuthorizationCallback(const std::string& callbackUri)
{
  auto state = mState; auto consumed = state->oauth->HandleCallback(callbackUri);
  if (!consumed)
  {
    AuthorizationCallback callback; { std::lock_guard lock(state->mutex); callback = std::move(state->authorizationCallback); state->connection = ProviderConnectionState::Error; }
    if (callback) callback(Failed<AuthorizationResult>(consumed.error.code, consumed.error.userMessage.c_str())); return;
  }
  { std::lock_guard lock(state->mutex); state->status.state = ProviderTaskState::ExchangingCode; }
  auto selected = consumed.value->selectedToneId; std::weak_ptr<State> weak = state;
  state->api->ExchangeAuthorizationCode(std::move(consumed.value->authorizationCode), std::move(consumed.value->verifier),
    [weak, selected = std::move(selected)](auto tokens) mutable {
      const auto state = weak.lock(); if (!state || !state->alive.load()) return;
      AuthorizationCallback callback; { std::lock_guard lock(state->mutex); callback = std::move(state->authorizationCallback); }
      if (!tokens || !tokens.value->refreshToken)
      {
        { std::lock_guard lock(state->mutex); state->connection = ProviderConnectionState::Error; state->status.state = ProviderTaskState::Failed; state->status.error = tokens.error.code; }
        if (callback) callback(Failed<AuthorizationResult>(tokens.error.code == ProviderErrorCode::None ? ProviderErrorCode::TokenExchangeFailed : tokens.error.code,
                                                           "TONE3000 authorization failed")); return;
      }
      credentials::SecureCredential credential; credential.tokens = std::move(*tokens.value);
      credential.tokens.rotationGeneration = 1; credential.verifiedAt = library::UnixTimeNow();
      if (!state->credentialStore->Store(state->credentialKey, credential))
      { if (callback) callback(Failed<AuthorizationResult>(ProviderErrorCode::StorageFailed, "TONE3000 credential could not be stored")); return; }
      { std::lock_guard lock(state->mutex); state->connection = ProviderConnectionState::Connected; state->status.state = ProviderTaskState::Completed; }
      if (callback) callback({AuthorizationResult{std::move(selected), false}, {}});
    });
}

void Tone3000Provider::CancelAuthorization()
{
  mState->oauth->Cancel(); AuthorizationCallback callback;
  { std::lock_guard lock(mState->mutex); callback = std::move(mState->authorizationCallback); mState->connection = ProviderConnectionState::Disconnected; mState->status.state = ProviderTaskState::Cancelled; }
  if (callback) callback(Failed<AuthorizationResult>(ProviderErrorCode::Cancelled, "TONE3000 authorization was cancelled"));
}

void Tone3000Provider::Disconnect()
{
  CancelAuthorization(); mState->downloads->Cancel(); mState->credentialStore->Remove(mState->credentialKey);
  std::lock_guard lock(mState->mutex); mState->connection = mState->configuration.HasPublishableConfiguration() ? ProviderConnectionState::Disconnected : ProviderConnectionState::Unconfigured;
}

void Tone3000Provider::FetchCurrentUser(ProviderUserCallback callback)
{
  std::weak_ptr<State> weak = mState;
  mState->AccessToken([weak, callback = std::move(callback)](auto token) mutable {
    const auto state = weak.lock(); if (!state || !state->alive.load()) return;
    if (!token) { callback(Failed<ProviderUser>(token.error.code, token.error.userMessage.c_str())); return; }
    state->api->FetchCurrentUser(std::move(*token.value), [weak, callback = std::move(callback)](auto user) mutable {
      const auto state = weak.lock(); if (!state || !state->alive.load()) return;
      if (user)
      {
        credentials::SecureCredential credential;
        if (state->credentialStore->Load(state->credentialKey, credential))
        { credential.providerUserId = user.value->providerUserId; credential.username = user.value->username; credential.verifiedAt = library::UnixTimeNow(); state->credentialStore->Store(state->credentialKey, credential); }
      }
      callback(std::move(user));
    });
  });
}

void Tone3000Provider::FetchTone(const std::string& toneId, ToneCallback callback)
{
  std::weak_ptr<State> weak = mState;
  mState->AccessToken([weak, toneId, callback = std::move(callback)](auto token) mutable {
    const auto state = weak.lock(); if (!state || !state->alive.load()) return;
    if (!token) { callback(Failed<RemoteTone>(token.error.code, token.error.userMessage.c_str())); return; }
    state->api->FetchTone(std::move(*token.value), toneId, std::move(callback));
  });
}

void Tone3000Provider::FetchModelsForTone(const std::string& toneId, ModelListCallback callback)
{
  std::weak_ptr<State> weak = mState;
  mState->AccessToken([weak, toneId, callback = std::move(callback)](auto token) mutable {
    const auto state = weak.lock(); if (!state || !state->alive.load()) return;
    if (!token) { callback(Failed<std::vector<RemoteModel>>(token.error.code, token.error.userMessage.c_str())); return; }
    state->api->FetchModels(std::move(*token.value), toneId, RemoteContentFormat::Nam, std::move(callback));
  });
}

void Tone3000Provider::BeginModelDownload(const RemoteModel& model, const DownloadDestination&, DownloadCallback callback)
{
  std::weak_ptr<State> weak = mState;
  FetchTone(model.providerToneId, [weak, model, callback = std::move(callback)](auto tone) mutable {
    const auto state = weak.lock(); if (!state || !state->alive.load()) return;
    if (!tone) { callback(Failed<DownloadResult>(tone.error.code, tone.error.userMessage.c_str())); return; }
    state->AccessToken([weak, tone = std::move(*tone.value), model, callback = std::move(callback)](auto token) mutable {
      const auto state = weak.lock(); if (!state || !state->alive.load()) return;
      if (!token) { callback(Failed<DownloadResult>(token.error.code, token.error.userMessage.c_str())); return; }
      state->downloads->Begin(std::move(tone), model, std::move(*token.value), std::move(callback));
    });
  });
}

void Tone3000Provider::Shutdown()
{
  if (!mState || !mState->alive.exchange(false)) return;
  mState->oauth->Shutdown(); mState->downloads->Shutdown(); mState->api->Shutdown();
}

} // namespace amphibia::providers::tone3000
