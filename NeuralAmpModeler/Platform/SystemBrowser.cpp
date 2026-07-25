#include "SystemBrowser.h"

#include <algorithm>
#include <cctype>

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <ApplicationServices/ApplicationServices.h>
#endif

namespace amphibia::platform {
namespace {

providers::ProviderError Failure(providers::ProviderErrorCode code, const char* message)
{
  return {code, message, std::nullopt, std::nullopt, std::nullopt};
}

} // namespace

providers::ProviderError ValidateExternalAuthorizationUrl(const std::string& url)
{
  if (url.size() < 9 || url.size() > 8192 || !url.starts_with("https://"))
    return Failure(providers::ProviderErrorCode::InvalidRedirect, "Authorization URL must use HTTPS");
  if (std::any_of(url.begin(), url.end(), [](unsigned char c) { return c < 0x20 || c == 0x7f; }) ||
      url.find('#') != std::string::npos)
    return Failure(providers::ProviderErrorCode::InvalidRedirect, "Authorization URL was malformed");
  const auto authorityStart = std::string_view("https://").size();
  const auto authorityEnd = url.find_first_of("/?", authorityStart);
  const auto authority = url.substr(authorityStart, authorityEnd - authorityStart);
  if (authority.empty() || authority.find('@') != std::string::npos)
    return Failure(providers::ProviderErrorCode::InvalidRedirect, "Authorization URL authority was invalid");
  std::string lowercase = url;
  std::transform(lowercase.begin(), lowercase.end(), lowercase.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  for (const auto* forbidden : {"access_token=", "refresh_token=", "code_verifier=", "client_secret="})
    if (lowercase.find(forbidden) != std::string::npos)
      return Failure(providers::ProviderErrorCode::InvalidRedirect, "Authorization URL contained a secret");
  return {};
}

providers::ProviderError PlatformSystemBrowser::OpenExplicit(const std::string& validatedUrl)
{
  if (auto error = ValidateExternalAuthorizationUrl(validatedUrl)) return error;
#if defined(_WIN32)
  const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, validatedUrl.data(),
                                         static_cast<int>(validatedUrl.size()), nullptr, 0);
  if (length <= 0) return Failure(providers::ProviderErrorCode::InternalError, "Authorization URL encoding failed");
  std::wstring wide(static_cast<std::size_t>(length), L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, validatedUrl.data(), static_cast<int>(validatedUrl.size()),
                      wide.data(), length);
  if (reinterpret_cast<std::intptr_t>(ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) <= 32)
    return Failure(providers::ProviderErrorCode::InternalError, "The system browser could not be opened");
  return {};
#elif defined(__APPLE__)
  CFStringRef text = CFStringCreateWithBytes(kCFAllocatorDefault,
    reinterpret_cast<const UInt8*>(validatedUrl.data()), validatedUrl.size(), kCFStringEncodingUTF8, false);
  if (!text) return Failure(providers::ProviderErrorCode::InternalError, "Authorization URL encoding failed");
  CFURLRef url = CFURLCreateWithString(kCFAllocatorDefault, text, nullptr);
  CFRelease(text);
  if (!url) return Failure(providers::ProviderErrorCode::InternalError, "Authorization URL encoding failed");
  const OSStatus status = LSOpenCFURLRef(url, nullptr);
  CFRelease(url);
  if (status != noErr) return Failure(providers::ProviderErrorCode::InternalError, "The system browser could not be opened");
  return {};
#else
  return Failure(providers::ProviderErrorCode::InternalError, "System browser launch is unavailable on this platform");
#endif
}

} // namespace amphibia::platform
