#include "Pkce.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <stdexcept>

#if defined(_WIN32)
#include <windows.h>
#include <bcrypt.h>
#elif defined(__APPLE__)
#include <CommonCrypto/CommonDigest.h>
#include <Security/SecRandom.h>
#else
#error Amphibia PKCE requires an audited platform CSPRNG and SHA-256 implementation
#endif

namespace amphibia::providers::tone3000 {
namespace {

std::array<std::uint8_t, 32> Sha256(const std::string& value)
{
  std::array<std::uint8_t, 32> result{};
#if defined(_WIN32)
  BCRYPT_ALG_HANDLE algorithm = nullptr;
  BCRYPT_HASH_HANDLE hash = nullptr;
  DWORD objectBytes = 0;
  DWORD copied = 0;
  std::vector<std::uint8_t> object;
  if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0 ||
      BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectBytes),
                        sizeof(objectBytes), &copied, 0) < 0)
  {
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    throw std::runtime_error("Platform SHA-256 initialization failed");
  }
  object.resize(objectBytes);
  if (BCryptCreateHash(algorithm, &hash, object.data(), objectBytes, nullptr, 0, 0) < 0 ||
      BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char*>(value.data())),
                     static_cast<ULONG>(value.size()), 0) < 0 ||
      BCryptFinishHash(hash, result.data(), static_cast<ULONG>(result.size()), 0) < 0)
  {
    if (hash) BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    throw std::runtime_error("Platform SHA-256 operation failed");
  }
  BCryptDestroyHash(hash);
  BCryptCloseAlgorithmProvider(algorithm, 0);
#else
  CC_SHA256(value.data(), static_cast<CC_LONG>(value.size()), result.data());
#endif
  return result;
}

} // namespace

std::vector<std::uint8_t> CryptographicRandomBytes(std::size_t count)
{
  if (count == 0 || count > 4096) throw std::invalid_argument("Invalid cryptographic random byte count");
  std::vector<std::uint8_t> result(count);
#if defined(_WIN32)
  if (BCryptGenRandom(nullptr, result.data(), static_cast<ULONG>(result.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
    throw std::runtime_error("Platform cryptographic random generation failed");
#else
  if (SecRandomCopyBytes(kSecRandomDefault, result.size(), result.data()) != errSecSuccess)
    throw std::runtime_error("Platform cryptographic random generation failed");
#endif
  return result;
}

std::string Base64UrlNoPadding(const std::uint8_t* data, std::size_t size)
{
  static constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  std::string result;
  result.reserve((size * 4 + 2) / 3);
  std::size_t index = 0;
  while (index + 3 <= size)
  {
    const auto value = (static_cast<std::uint32_t>(data[index]) << 16) |
                       (static_cast<std::uint32_t>(data[index + 1]) << 8) | data[index + 2];
    result.push_back(alphabet[(value >> 18) & 63]);
    result.push_back(alphabet[(value >> 12) & 63]);
    result.push_back(alphabet[(value >> 6) & 63]);
    result.push_back(alphabet[value & 63]);
    index += 3;
  }
  if (index < size)
  {
    std::uint32_t value = static_cast<std::uint32_t>(data[index]) << 16;
    if (index + 1 < size) value |= static_cast<std::uint32_t>(data[index + 1]) << 8;
    result.push_back(alphabet[(value >> 18) & 63]);
    result.push_back(alphabet[(value >> 12) & 63]);
    if (index + 1 < size) result.push_back(alphabet[(value >> 6) & 63]);
  }
  return result;
}

std::string Sha256Base64Url(const std::string& value)
{
  const auto hash = Sha256(value);
  return Base64UrlNoPadding(hash.data(), hash.size());
}

PkcePair CreatePkceFromVerifier(std::string verifier)
{
  if (verifier.size() < 43 || verifier.size() > 128 ||
      !std::all_of(verifier.begin(), verifier.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~';
      }))
    throw std::invalid_argument("PKCE verifier does not satisfy RFC 7636");
  auto challenge = Sha256Base64Url(verifier);
  return {std::move(verifier), std::move(challenge)};
}

PkcePair GeneratePkce()
{
  const auto random = CryptographicRandomBytes(32);
  return CreatePkceFromVerifier(Base64UrlNoPadding(random.data(), random.size()));
}

std::string GenerateOAuthState()
{
  const auto random = CryptographicRandomBytes(32);
  return Base64UrlNoPadding(random.data(), random.size());
}

bool ConstantTimeEqual(const std::string& left, const std::string& right) noexcept
{
  std::size_t difference = left.size() ^ right.size();
  const auto maximum = (std::max)(left.size(), right.size());
  for (std::size_t index = 0; index < maximum; ++index)
  {
    const unsigned char a = index < left.size() ? static_cast<unsigned char>(left[index]) : 0;
    const unsigned char b = index < right.size() ? static_cast<unsigned char>(right[index]) : 0;
    difference |= a ^ b;
  }
  return difference == 0;
}

void SecureClear(std::string& value) noexcept
{
  volatile char* data = value.empty() ? nullptr : value.data();
  for (std::size_t index = 0; index < value.size(); ++index) data[index] = 0;
  value.clear();
  value.shrink_to_fit();
}

} // namespace amphibia::providers::tone3000
