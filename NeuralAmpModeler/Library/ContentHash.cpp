#include "ContentHash.h"

#include <array>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#elif defined(__APPLE__)
#include <CommonCrypto/CommonDigest.h>
#else
#error Amphibia ContentHash requires a supported platform crypto backend
#endif

namespace amphibia::library {
namespace {

int HexValue(char value)
{
  if (value >= '0' && value <= '9') return value - '0';
  if (value >= 'a' && value <= 'f') return value - 'a' + 10;
  if (value >= 'A' && value <= 'F') return value - 'A' + 10;
  return -1;
}

class Sha256
{
public:
  Sha256()
  {
#if defined(_WIN32)
    if (BCryptOpenAlgorithmProvider(&mAlgorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
      throw std::runtime_error("SHA-256 provider is unavailable");
    DWORD bytes = 0;
    DWORD objectBytes = 0;
    if (BCryptGetProperty(mAlgorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectBytes),
                          sizeof(objectBytes), &bytes, 0) < 0)
      throw std::runtime_error("SHA-256 provider object query failed");
    mObject.resize(objectBytes);
    if (BCryptCreateHash(mAlgorithm, &mHash, mObject.data(), static_cast<ULONG>(mObject.size()), nullptr, 0, 0) < 0)
      throw std::runtime_error("SHA-256 initialization failed");
#elif defined(__APPLE__)
    CC_SHA256_Init(&mContext);
#endif
  }

  ~Sha256()
  {
#if defined(_WIN32)
    if (mHash != nullptr) BCryptDestroyHash(mHash);
    if (mAlgorithm != nullptr) BCryptCloseAlgorithmProvider(mAlgorithm, 0);
#endif
  }

  Sha256(const Sha256&) = delete;
  Sha256& operator=(const Sha256&) = delete;

  void Update(std::span<const std::uint8_t> bytes)
  {
    if (mFinished) throw std::logic_error("SHA-256 hash already finalized");
    if (bytes.empty()) return;
#if defined(_WIN32)
    if (bytes.size() > static_cast<std::size_t>(ULONG_MAX))
      throw std::runtime_error("SHA-256 update is too large");
    if (BCryptHashData(mHash, const_cast<PUCHAR>(bytes.data()), static_cast<ULONG>(bytes.size()), 0) < 0)
      throw std::runtime_error("SHA-256 update failed");
#elif defined(__APPLE__)
    if (bytes.size() > static_cast<std::size_t>(std::numeric_limits<CC_LONG>::max()))
      throw std::runtime_error("SHA-256 update is too large");
    if (CC_SHA256_Update(&mContext, bytes.data(), static_cast<CC_LONG>(bytes.size())) != 1)
      throw std::runtime_error("SHA-256 update failed");
#endif
  }

  ContentHash Finish()
  {
    if (mFinished) throw std::logic_error("SHA-256 hash already finalized");
    mFinished = true;
    ContentHash result;
#if defined(_WIN32)
    if (BCryptFinishHash(mHash, result.bytes.data(), static_cast<ULONG>(result.bytes.size()), 0) < 0)
      throw std::runtime_error("SHA-256 finalization failed");
#elif defined(__APPLE__)
    if (CC_SHA256_Final(result.bytes.data(), &mContext) != 1)
      throw std::runtime_error("SHA-256 finalization failed");
#endif
    return result;
  }

private:
  bool mFinished{};
#if defined(_WIN32)
  BCRYPT_ALG_HANDLE mAlgorithm{};
  BCRYPT_HASH_HANDLE mHash{};
  std::vector<std::uint8_t> mObject;
#elif defined(__APPLE__)
  CC_SHA256_CTX mContext{};
#endif
};

} // namespace

std::string ToHex(const ContentHash& hash)
{
  static constexpr char digits[] = "0123456789abcdef";
  std::string result;
  result.resize(hash.bytes.size() * 2);
  for (std::size_t i = 0; i < hash.bytes.size(); ++i)
  {
    result[i * 2] = digits[hash.bytes[i] >> 4];
    result[i * 2 + 1] = digits[hash.bytes[i] & 0x0f];
  }
  return result;
}

ContentHash ContentHashFromHex(const std::string& text)
{
  if (text.size() != 64) throw std::invalid_argument("Content hash must contain 64 hexadecimal characters");
  ContentHash result;
  for (std::size_t i = 0; i < result.bytes.size(); ++i)
  {
    const int high = HexValue(text[i * 2]);
    const int low = HexValue(text[i * 2 + 1]);
    if (high < 0 || low < 0) throw std::invalid_argument("Content hash contains a non-hexadecimal character");
    result.bytes[i] = static_cast<std::uint8_t>((high << 4) | low);
  }
  return result;
}

ContentHash HashBytes(std::span<const std::uint8_t> bytes)
{
  Sha256 hasher;
  hasher.Update(bytes);
  return hasher.Finish();
}

ContentHash HashFile(const std::filesystem::path& path, const CancelCheck& cancelled,
                     const std::function<void(std::uint64_t)>& progress)
{
  std::ifstream input(path, std::ios::binary);
  if (!input.is_open())
    throw ImportException(ImportErrorCode::HashFailure, "Could not open file for hashing");

  Sha256 hasher;
  std::vector<std::uint8_t> buffer(1024 * 1024);
  std::uint64_t completed = 0;
  for (;;)
  {
    if (cancelled && cancelled())
      throw ImportException(ImportErrorCode::Cancelled, "Hashing cancelled");
    input.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
    const auto count = input.gcount();
    if (count > 0)
    {
      hasher.Update(std::span<const std::uint8_t>(buffer.data(), static_cast<std::size_t>(count)));
      completed += static_cast<std::uint64_t>(count);
      if (progress) progress(completed);
    }
    if (input.eof()) break;
    if (!input)
      throw ImportException(ImportErrorCode::HashFailure, "File read failed while hashing");
  }
  return hasher.Finish();
}

} // namespace amphibia::library
