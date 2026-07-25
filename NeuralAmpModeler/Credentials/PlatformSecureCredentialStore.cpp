#include "ISecureCredentialStore.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#include <wincred.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#endif

namespace amphibia::credentials {
namespace {

constexpr std::array<std::uint8_t, 8> kMagic{'A', 'M', 'P', 'H', 'C', 'R', 'D', 1};
constexpr std::size_t kMaximumTokenBytes = 1536;
constexpr std::size_t kMaximumTextBytes = 512;
constexpr std::size_t kMaximumBlobBytes = 2560; // Windows generic-credential platform maximum.

CredentialResult Failure(CredentialError error, const char* message) { return {error, message}; }

bool ValidKeyPart(const std::string& value)
{
  return !value.empty() && value.size() <= 128 &&
         std::all_of(value.begin(), value.end(), [](unsigned char c) {
           return std::isalnum(c) || c == '.' || c == '-' || c == '_';
         });
}

void Append64(std::vector<std::uint8_t>& output, std::uint64_t value)
{
  for (int shift = 0; shift < 64; shift += 8) output.push_back(static_cast<std::uint8_t>(value >> shift));
}

void AppendString(std::vector<std::uint8_t>& output, const std::optional<std::string>& value, std::size_t maximum)
{
  const std::string* text = value ? &*value : nullptr;
  if (text && text->size() > maximum) throw std::invalid_argument("Credential value is too large");
  const std::uint32_t size = text ? static_cast<std::uint32_t>(text->size()) : std::numeric_limits<std::uint32_t>::max();
  for (int shift = 0; shift < 32; shift += 8) output.push_back(static_cast<std::uint8_t>(size >> shift));
  if (text) output.insert(output.end(), text->begin(), text->end());
}

std::vector<std::uint8_t> Serialize(const SecureCredential& value)
{
  if (value.tokens.accessToken.empty() || value.tokens.accessToken.size() > kMaximumTokenBytes ||
      value.tokens.tokenType != "bearer" ||
      (value.tokens.refreshToken && value.tokens.refreshToken->size() > kMaximumTokenBytes))
    throw std::invalid_argument("Credential token fields are invalid");
  std::vector<std::uint8_t> output(kMagic.begin(), kMagic.end());
  const auto expiry = std::chrono::duration_cast<std::chrono::seconds>(
    value.tokens.accessTokenExpiry.time_since_epoch()).count();
  Append64(output, static_cast<std::uint64_t>(expiry));
  Append64(output, value.tokens.rotationGeneration);
  Append64(output, static_cast<std::uint64_t>(value.verifiedAt));
  AppendString(output, value.tokens.accessToken, kMaximumTokenBytes);
  AppendString(output, value.tokens.refreshToken, kMaximumTokenBytes);
  AppendString(output, value.tokens.tokenType, 16);
  AppendString(output, value.tokens.scope, kMaximumTextBytes);
  AppendString(output, value.providerUserId, kMaximumTextBytes);
  AppendString(output, value.username, kMaximumTextBytes);
  if (output.size() > kMaximumBlobBytes) throw std::invalid_argument("Credential is too large");
  return output;
}

bool Read64(const std::vector<std::uint8_t>& input, std::size_t& offset, std::uint64_t& value)
{
  if (offset + 8 > input.size()) return false;
  value = 0;
  for (int shift = 0; shift < 64; shift += 8) value |= static_cast<std::uint64_t>(input[offset++]) << shift;
  return true;
}

bool ReadString(const std::vector<std::uint8_t>& input, std::size_t& offset, std::optional<std::string>& value,
                std::size_t maximum)
{
  if (offset + 4 > input.size()) return false;
  std::uint32_t size = 0;
  for (int shift = 0; shift < 32; shift += 8) size |= static_cast<std::uint32_t>(input[offset++]) << shift;
  if (size == std::numeric_limits<std::uint32_t>::max()) { value.reset(); return true; }
  if (size > maximum || offset + size > input.size()) return false;
  value.emplace(reinterpret_cast<const char*>(input.data() + offset), size);
  offset += size;
  return true;
}

bool Deserialize(const std::vector<std::uint8_t>& input, SecureCredential& value)
{
  if (input.size() < kMagic.size() || input.size() > kMaximumBlobBytes ||
      !std::equal(kMagic.begin(), kMagic.end(), input.begin())) return false;
  std::size_t offset = kMagic.size();
  std::uint64_t expiry = 0, generation = 0, verified = 0;
  std::optional<std::string> access, refresh, tokenType, scope, providerUser, username;
  if (!Read64(input, offset, expiry) || !Read64(input, offset, generation) || !Read64(input, offset, verified) ||
      !ReadString(input, offset, access, kMaximumTokenBytes) ||
      !ReadString(input, offset, refresh, kMaximumTokenBytes) ||
      !ReadString(input, offset, tokenType, 16) || !ReadString(input, offset, scope, kMaximumTextBytes) ||
      !ReadString(input, offset, providerUser, kMaximumTextBytes) ||
      !ReadString(input, offset, username, kMaximumTextBytes) || offset != input.size() ||
      !access || access->empty() || !tokenType || *tokenType != "bearer" ||
      expiry > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) return false;
  SecureCredential parsed;
  parsed.tokens.accessToken = std::move(*access);
  parsed.tokens.refreshToken = std::move(refresh);
  parsed.tokens.tokenType = std::move(*tokenType);
  parsed.tokens.scope = std::move(scope);
  parsed.tokens.rotationGeneration = generation;
  parsed.tokens.accessTokenExpiry = std::chrono::system_clock::time_point(std::chrono::seconds(expiry));
  parsed.providerUserId = std::move(providerUser);
  parsed.username = std::move(username);
  parsed.verifiedAt = static_cast<std::int64_t>(verified);
  value = std::move(parsed);
  return true;
}

void ClearBlob(std::vector<std::uint8_t>& blob)
{
  volatile std::uint8_t* data = blob.empty() ? nullptr : blob.data();
  for (std::size_t index = 0; index < blob.size(); ++index) data[index] = 0;
  blob.clear();
}

#if defined(_WIN32)
std::wstring Wide(const std::string& value)
{
  const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                                        nullptr, 0);
  if (count <= 0) return {};
  std::wstring result(static_cast<std::size_t>(count), L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), count);
  return result;
}
#endif

} // namespace

bool PlatformSecureCredentialStore::IsAvailable() const noexcept
{
#if defined(_WIN32) || defined(__APPLE__)
  return true;
#else
  return false;
#endif
}

CredentialResult PlatformSecureCredentialStore::Store(const CredentialKey& key, const SecureCredential& value)
{
  if (!ValidKeyPart(key.serviceNamespace) || !ValidKeyPart(key.account))
    return Failure(CredentialError::InvalidKey, "Credential namespace is invalid");
  std::vector<std::uint8_t> blob;
  try { blob = Serialize(value); }
  catch (...) { return Failure(CredentialError::InvalidValue, "Credential value is invalid"); }
#if defined(_WIN32)
  const auto target = Wide(key.serviceNamespace + "/" + key.account);
  CREDENTIALW credential{};
  credential.Type = CRED_TYPE_GENERIC;
  credential.TargetName = const_cast<wchar_t*>(target.c_str());
  credential.CredentialBlobSize = static_cast<DWORD>(blob.size());
  credential.CredentialBlob = blob.data();
  credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
  credential.UserName = const_cast<wchar_t*>(L"Amphibia TONE3000 OAuth");
  const bool ok = CredWriteW(&credential, 0) != FALSE;
  const auto error = GetLastError();
  ClearBlob(blob);
  if (!ok) return Failure(error == ERROR_ACCESS_DENIED ? CredentialError::AccessDenied : CredentialError::PlatformError,
                          "Windows Credential Manager could not store the credential");
  return {};
#elif defined(__APPLE__)
  CFStringRef service = CFStringCreateWithCString(nullptr, key.serviceNamespace.c_str(), kCFStringEncodingUTF8);
  CFStringRef account = CFStringCreateWithCString(nullptr, key.account.c_str(), kCFStringEncodingUTF8);
  CFDataRef data = CFDataCreate(nullptr, blob.data(), blob.size());
  const void* queryKeys[] = {kSecClass, kSecAttrService, kSecAttrAccount};
  const void* queryValues[] = {kSecClassGenericPassword, service, account};
  CFDictionaryRef query = CFDictionaryCreate(nullptr, queryKeys, queryValues, 3,
                                             &kCFTypeDictionaryKeyCallBacks,
                                             &kCFTypeDictionaryValueCallBacks);
  const void* updateKeys[] = {kSecValueData}; const void* updateValues[] = {data};
  CFDictionaryRef update = CFDictionaryCreate(nullptr, updateKeys, updateValues, 1,
                                              &kCFTypeDictionaryKeyCallBacks,
                                              &kCFTypeDictionaryValueCallBacks);
  OSStatus status = SecItemUpdate(query, update);
  if (status == errSecItemNotFound)
  {
    const void* addKeys[] = {kSecClass, kSecAttrService, kSecAttrAccount, kSecValueData};
    const void* addValues[] = {kSecClassGenericPassword, service, account, data};
    CFDictionaryRef add = CFDictionaryCreate(nullptr, addKeys, addValues, 4,
                                            &kCFTypeDictionaryKeyCallBacks,
                                            &kCFTypeDictionaryValueCallBacks);
    status = SecItemAdd(add, nullptr); CFRelease(add);
  }
  CFRelease(update); CFRelease(query); CFRelease(data); CFRelease(account); CFRelease(service); ClearBlob(blob);
  return status == errSecSuccess ? CredentialResult{} : Failure(CredentialError::PlatformError, "macOS Keychain could not store the credential");
#else
  ClearBlob(blob);
  return Failure(CredentialError::Unavailable, "Secure credential storage is unavailable");
#endif
}

CredentialResult PlatformSecureCredentialStore::Load(const CredentialKey& key, SecureCredential& value)
{
  if (!ValidKeyPart(key.serviceNamespace) || !ValidKeyPart(key.account))
    return Failure(CredentialError::InvalidKey, "Credential namespace is invalid");
  std::vector<std::uint8_t> blob;
#if defined(_WIN32)
  const auto target = Wide(key.serviceNamespace + "/" + key.account);
  PCREDENTIALW credential = nullptr;
  if (!CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &credential))
    return Failure(GetLastError() == ERROR_NOT_FOUND ? CredentialError::NotFound : CredentialError::PlatformError,
                   "Windows Credential Manager could not load the credential");
  blob.assign(credential->CredentialBlob, credential->CredentialBlob + credential->CredentialBlobSize);
  CredFree(credential);
#elif defined(__APPLE__)
  CFStringRef service = CFStringCreateWithCString(nullptr, key.serviceNamespace.c_str(), kCFStringEncodingUTF8);
  CFStringRef account = CFStringCreateWithCString(nullptr, key.account.c_str(), kCFStringEncodingUTF8);
  const void* keys[] = {kSecClass, kSecAttrService, kSecAttrAccount, kSecReturnData, kSecMatchLimit};
  const void* values[] = {kSecClassGenericPassword, service, account, kCFBooleanTrue, kSecMatchLimitOne};
  CFDictionaryRef query = CFDictionaryCreate(nullptr, keys, values, 5,
                                             &kCFTypeDictionaryKeyCallBacks,
                                             &kCFTypeDictionaryValueCallBacks);
  CFTypeRef found = nullptr; const OSStatus status = SecItemCopyMatching(query, &found);
  CFRelease(query); CFRelease(account); CFRelease(service);
  if (status != errSecSuccess)
    return Failure(status == errSecItemNotFound ? CredentialError::NotFound : CredentialError::PlatformError,
                   "macOS Keychain could not load the credential");
  CFDataRef data = static_cast<CFDataRef>(found);
  blob.assign(CFDataGetBytePtr(data), CFDataGetBytePtr(data) + CFDataGetLength(data)); CFRelease(found);
#else
  return Failure(CredentialError::Unavailable, "Secure credential storage is unavailable");
#endif
  const bool valid = Deserialize(blob, value);
  ClearBlob(blob);
  return valid ? CredentialResult{} : Failure(CredentialError::Corrupt, "Stored credential was invalid");
}

CredentialResult PlatformSecureCredentialStore::Remove(const CredentialKey& key)
{
  if (!ValidKeyPart(key.serviceNamespace) || !ValidKeyPart(key.account))
    return Failure(CredentialError::InvalidKey, "Credential namespace is invalid");
#if defined(_WIN32)
  const auto target = Wide(key.serviceNamespace + "/" + key.account);
  if (CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0) || GetLastError() == ERROR_NOT_FOUND) return {};
  return Failure(CredentialError::PlatformError, "Windows Credential Manager could not remove the credential");
#elif defined(__APPLE__)
  CFStringRef service = CFStringCreateWithCString(nullptr, key.serviceNamespace.c_str(), kCFStringEncodingUTF8);
  CFStringRef account = CFStringCreateWithCString(nullptr, key.account.c_str(), kCFStringEncodingUTF8);
  const void* keys[] = {kSecClass, kSecAttrService, kSecAttrAccount};
  const void* values[] = {kSecClassGenericPassword, service, account};
  CFDictionaryRef query = CFDictionaryCreate(nullptr, keys, values, 3,
                                             &kCFTypeDictionaryKeyCallBacks,
                                             &kCFTypeDictionaryValueCallBacks);
  const OSStatus status = SecItemDelete(query); CFRelease(query); CFRelease(account); CFRelease(service);
  if (status == errSecSuccess || status == errSecItemNotFound) return {};
  return Failure(CredentialError::PlatformError, "macOS Keychain could not remove the credential");
#else
  return Failure(CredentialError::Unavailable, "Secure credential storage is unavailable");
#endif
}

} // namespace amphibia::credentials
