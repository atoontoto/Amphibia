#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace amphibia::providers {

enum class RemoteProviderKind { Tone3000 };
enum class RemoteContentFormat { Nam, ImpulseResponse, Unknown };
enum class RemoteGearRole { AmpHead, AmpAndCab, Pedal, Cabinet, Space, Outboard, Experimental, Unknown };
enum class RemoteNamArchitecture { Unknown, A1, A2 };

struct RemoteCreator
{
  std::string providerUserId;
  std::string username;
  std::optional<std::string> displayName;
  std::optional<std::string> avatarUrl;
  std::optional<std::string> profileUrl;
};

struct RemoteTone
{
  RemoteProviderKind provider{RemoteProviderKind::Tone3000};
  std::string providerToneId;
  std::string title;
  std::optional<std::string> description;
  RemoteCreator creator;
  RemoteContentFormat format{RemoteContentFormat::Unknown};
  RemoteGearRole gearRole{RemoteGearRole::Unknown};
  std::optional<std::string> licenseId;
  std::optional<std::string> licenseName;
  std::optional<std::string> sourcePageUrl;
  std::vector<std::string> tags;
  std::vector<std::string> imageUrls;
};

struct RemoteModel
{
  RemoteProviderKind provider{RemoteProviderKind::Tone3000};
  std::string providerToneId;
  std::string providerModelId;
  std::string name;
  RemoteContentFormat format{RemoteContentFormat::Unknown};
  RemoteNamArchitecture architecture{RemoteNamArchitecture::Unknown};
  std::optional<std::uint64_t> declaredSize;
  std::optional<std::string> relativePath;
  std::optional<std::string> downloadUrl;
};

struct ProviderUser
{
  RemoteProviderKind provider{RemoteProviderKind::Tone3000};
  std::string providerUserId;
  std::string username;
  std::optional<std::string> displayName;
  std::optional<std::string> avatarUrl;
  std::optional<std::string> profileUrl;
};

enum class AuthorizationPurpose { ConnectAccount, SelectTone, ResolveKnownTone };

struct AuthorizationRequest
{
  AuthorizationPurpose purpose{AuthorizationPurpose::ConnectAccount};
  std::optional<std::string> toneId;
  std::vector<std::string> gearFilters;
  std::optional<RemoteContentFormat> format;
  std::optional<RemoteNamArchitecture> architecture;
};

enum class ProviderConnectionState { Unconfigured, Disconnected, Authorizing, Connected, Refreshing, Expired, Error };

enum class ProviderErrorCode
{
  None,
  Unconfigured,
  Cancelled,
  Superseded,
  Offline,
  DnsFailure,
  TlsFailure,
  ConnectTimeout,
  RequestTimeout,
  InvalidRedirect,
  AuthenticationRequired,
  AuthorizationRejected,
  InvalidOAuthState,
  TokenExchangeFailed,
  TokenRefreshFailed,
  InvalidGrant,
  Forbidden,
  NotFound,
  RateLimited,
  ServerError,
  InvalidJson,
  InvalidResponse,
  UnsupportedContent,
  ContentTooLarge,
  DownloadFailed,
  StorageFailed,
  IntegrityFailed,
  InternalError
};

struct ProviderError
{
  ProviderErrorCode code{ProviderErrorCode::None};
  std::string userMessage;
  std::optional<int> httpStatus;
  std::optional<std::chrono::system_clock::time_point> retryAt;
  std::optional<std::string> redactedDiagnostic;

  explicit operator bool() const noexcept { return code != ProviderErrorCode::None; }
};

enum class ProviderTaskState
{
  Idle,
  LaunchingBrowser,
  AwaitingAuthorization,
  ExchangingCode,
  LoadingCredential,
  RefreshingToken,
  FetchingUser,
  FetchingTone,
  FetchingModels,
  WaitingForRateLimit,
  Downloading,
  Validating,
  Hashing,
  Promoting,
  Completed,
  Failed,
  Cancelled,
  Superseded
};

struct ProviderTaskStatus
{
  std::uint64_t taskId{};
  ProviderTaskState state{ProviderTaskState::Idle};
  std::uint64_t completedBytes{};
  std::optional<std::uint64_t> totalBytes;
  std::optional<std::string> safeDisplayName;
  std::optional<ProviderErrorCode> error;
  std::optional<std::chrono::system_clock::time_point> retryAt;
};

struct AuthorizationResult
{
  std::optional<std::string> selectedToneId;
  bool cancelled{};
};

struct DownloadDestination
{
  std::filesystem::path stagingPath;
  std::uint64_t maximumBytes{};
};

struct DownloadResult
{
  std::filesystem::path verifiedStagingPath;
  std::uint64_t bytes{};
  std::string managedObjectSha256;
};

template <typename T> struct ProviderResult
{
  std::optional<T> value;
  ProviderError error;
  explicit operator bool() const noexcept { return value.has_value() && !error; }
};

using AuthorizationCallback = std::function<void(ProviderResult<AuthorizationResult>)>;
using ProviderUserCallback = std::function<void(ProviderResult<ProviderUser>)>;
using ToneCallback = std::function<void(ProviderResult<RemoteTone>)>;
using ModelListCallback = std::function<void(ProviderResult<std::vector<RemoteModel>>) >;
using DownloadCallback = std::function<void(ProviderResult<DownloadResult>)>;

} // namespace amphibia::providers
