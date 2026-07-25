#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace amphibia::providers::tone3000 {

struct PkcePair
{
  std::string verifier;
  std::string challenge;
};

std::vector<std::uint8_t> CryptographicRandomBytes(std::size_t count);
std::string Base64UrlNoPadding(const std::uint8_t* data, std::size_t size);
std::string Sha256Base64Url(const std::string& value);
PkcePair CreatePkceFromVerifier(std::string verifier);
PkcePair GeneratePkce();
std::string GenerateOAuthState();
bool ConstantTimeEqual(const std::string& left, const std::string& right) noexcept;
void SecureClear(std::string& value) noexcept;

} // namespace amphibia::providers::tone3000
