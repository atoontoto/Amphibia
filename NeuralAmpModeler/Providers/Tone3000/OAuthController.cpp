#include "OAuthController.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace amphibia::providers::tone3000 {
namespace {

ProviderError Failure(ProviderErrorCode code, const char* message)
{
  return {code, message, std::nullopt, std::nullopt, std::nullopt};
}

std::string Encode(std::string_view value)
{
  static constexpr char hex[] = "0123456789ABCDEF";
  std::string result;
  result.reserve(value.size() * 3);
  for (const unsigned char byte : value)
  {
    if (std::isalnum(byte) || byte == '-' || byte == '.' || byte == '_' || byte == '~') result.push_back(byte);
    else
    {
      result.push_back('%');
      result.push_back(hex[byte >> 4]);
      result.push_back(hex[byte & 15]);
    }
  }
  return result;
}

const char* FormatName(RemoteContentFormat format)
{
  if (format == RemoteContentFormat::Nam) return "nam";
  if (format == RemoteContentFormat::ImpulseResponse) return "ir";
  return nullptr;
}

const char* ArchitectureName(RemoteNamArchitecture architecture)
{
  if (architecture == RemoteNamArchitecture::A1) return "1";
  if (architecture == RemoteNamArchitecture::A2) return "2";
  return nullptr;
}

} // namespace

OAuthController::OAuthController(Tone3000Configuration configuration, std::chrono::seconds timeout)
: mConfiguration(std::move(configuration)), mTimeout(timeout)
{
}

OAuthController::~OAuthController() { Shutdown(); }

ProviderError OAuthController::ValidateConfigurationAndRequest(const AuthorizationRequest& request) const
{
  if (!mConfiguration.HasPublishableConfiguration())
    return Failure(ProviderErrorCode::Unconfigured, "TONE3000 provider registration is not configured");
  const bool testHttp = mConfiguration.kind == Tone3000ConfigurationKind::Test && mConfiguration.allowTestHttp;
  if (!testHttp && !mConfiguration.authorizationEndpoint.starts_with("https://"))
    return Failure(ProviderErrorCode::InvalidRedirect, "Provider authorization must use HTTPS");
  if (mConfiguration.authorizationEndpoint.find_first_of("\r\n#") != std::string::npos ||
      mConfiguration.authorizationEndpoint.find('@', mConfiguration.authorizationEndpoint.find("://") + 3) !=
        std::string::npos)
    return Failure(ProviderErrorCode::InvalidRedirect, "Provider authorization endpoint is invalid");
  if (request.purpose == AuthorizationPurpose::ResolveKnownTone)
  {
    if (mConfiguration.knownTonePrompt.empty())
      return Failure(ProviderErrorCode::Unconfigured,
                     "Known-tone authorization is disabled pending provider prompt clarification");
    if (!request.toneId || request.toneId->empty() || request.toneId->size() > 256)
      return Failure(ProviderErrorCode::InvalidResponse, "Known-tone authorization requires a bounded tone ID");
  }
  if (request.gearFilters.size() > 16 ||
      std::any_of(request.gearFilters.begin(), request.gearFilters.end(), [](const std::string& value) {
        return value.empty() || value.size() > 32 ||
               !std::all_of(value.begin(), value.end(), [](unsigned char c) { return std::isalnum(c) || c == '-'; });
      }))
    return Failure(ProviderErrorCode::InvalidResponse, "Authorization filters were invalid");
  return {};
}

std::string OAuthController::BuildUrl(const AuthorizationRequest& request, const PkcePair& pkce,
                                      const std::string& state) const
{
  std::vector<std::pair<std::string, std::string>> fields = {
    {"client_id", mConfiguration.clientId}, {"redirect_uri", mConfiguration.redirectUri},
    {"response_type", "code"}, {"code_challenge", pkce.challenge},
    {"code_challenge_method", "S256"}, {"state", state}};
  if (!mConfiguration.requestedScopes.empty()) fields.emplace_back("scope", mConfiguration.requestedScopes);
  if (request.purpose == AuthorizationPurpose::SelectTone) fields.emplace_back("prompt", "select_tone");
  else if (request.purpose == AuthorizationPurpose::ResolveKnownTone)
  {
    fields.emplace_back("prompt", mConfiguration.knownTonePrompt);
    fields.emplace_back("tone_id", *request.toneId);
  }
  if (!request.gearFilters.empty())
  {
    std::string gears;
    for (const auto& gear : request.gearFilters)
    {
      if (!gears.empty()) gears.push_back('_');
      gears += gear;
    }
    fields.emplace_back("gears", std::move(gears));
  }
  if (request.format)
    if (const auto* value = FormatName(*request.format)) fields.emplace_back("format", value);
  if (request.architecture)
    if (const auto* value = ArchitectureName(*request.architecture)) fields.emplace_back("architecture", value);

  std::ostringstream result;
  result << mConfiguration.authorizationEndpoint <<
    (mConfiguration.authorizationEndpoint.find('?') == std::string::npos ? '?' : '&');
  for (std::size_t index = 0; index < fields.size(); ++index)
  {
    if (index) result << '&';
    result << Encode(fields[index].first) << '=' << Encode(fields[index].second);
  }
  return result.str();
}

ProviderResult<AuthorizationStart> OAuthController::Begin(const AuthorizationRequest& request)
{
  ProviderResult<AuthorizationStart> result;
  std::lock_guard<std::mutex> guard(mMutex);
  if (mShutdown)
  {
    result.error = Failure(ProviderErrorCode::Cancelled, "Provider authorization is shut down");
    return result;
  }
  if (auto error = ValidateConfigurationAndRequest(request))
  {
    result.error = std::move(error);
    return result;
  }
  mPending.reset();
  auto pkce = GeneratePkce();
  auto state = GenerateOAuthState();
  PendingAuthorization pending;
  pending.flowId = ++mNextFlowId;
  pending.request = request;
  pending.state = state;
  pending.verifier = std::move(pkce.verifier);
  pending.deadline = std::chrono::steady_clock::now() + mTimeout;
  const auto url = BuildUrl(request, pkce, state);
  const auto id = pending.flowId;
  mPending.emplace(std::move(pending));
  result.value = AuthorizationStart{id, url};
  return result;
}

ProviderResult<ConsumedAuthorization> OAuthController::HandleCallback(const std::string& callbackUri)
{
  ProviderResult<ConsumedAuthorization> result;
  std::lock_guard<std::mutex> guard(mMutex);
  if (mShutdown || !mPending)
  {
    result.error = Failure(ProviderErrorCode::Superseded, "Authorization callback is not active");
    return result;
  }
  auto pending = std::move(*mPending);
  mPending.reset(); // state and verifier become single-use before parsing/validation
  if (std::chrono::steady_clock::now() > pending.deadline)
  {
    result.error = Failure(ProviderErrorCode::Cancelled, "Authorization request timed out");
    return result;
  }
  auto parsed = ParseOAuthCallback(callbackUri, mConfiguration.redirectUri);
  if (!parsed)
  {
    result.error = std::move(parsed.error);
    return result;
  }
  if (!ConstantTimeEqual(parsed.value->state, pending.state))
  {
    result.error = Failure(ProviderErrorCode::InvalidOAuthState, "Authorization state did not match");
    return result;
  }
  if (parsed.value->cancelled && !parsed.value->code)
  {
    result.error = Failure(ProviderErrorCode::Cancelled, "Authorization was cancelled");
    return result;
  }
  if (parsed.value->error)
  {
    result.error = Failure(ProviderErrorCode::AuthorizationRejected, "Authorization was rejected");
    result.error.redactedDiagnostic = parsed.value->error;
    return result;
  }
  ConsumedAuthorization consumed;
  consumed.flowId = pending.flowId;
  consumed.request = std::move(pending.request);
  consumed.authorizationCode = std::move(*parsed.value->code);
  consumed.verifier = std::move(pending.verifier);
  consumed.selectedToneId = std::move(parsed.value->toneId);
  result.value.emplace(std::move(consumed));
  return result;
}

void OAuthController::Cancel()
{
  std::lock_guard<std::mutex> guard(mMutex);
  mPending.reset();
}

void OAuthController::Shutdown()
{
  std::lock_guard<std::mutex> guard(mMutex);
  mShutdown = true;
  mPending.reset();
}

bool OAuthController::IsPending() const
{
  std::lock_guard<std::mutex> guard(mMutex);
  return mPending.has_value();
}

} // namespace amphibia::providers::tone3000
