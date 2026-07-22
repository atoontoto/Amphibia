#include "PathSafety.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <unordered_set>

namespace amphibia::library {
namespace {

std::string LowerAscii(std::string value)
{
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
  });
  return value;
}

bool HasControlCharacter(const std::string& value)
{
  return std::any_of(value.begin(), value.end(), [](unsigned char c) { return c < 0x20 || c == 0x7f; });
}

} // namespace

std::string PathToUtf8(const std::filesystem::path& path)
{
  const auto encoded = path.generic_u8string();
  return {reinterpret_cast<const char*>(encoded.data()), encoded.size()};
}

bool IsWindowsReservedComponent(const std::string& component)
{
  std::string base = component;
  const auto dot = base.find('.');
  if (dot != std::string::npos) base.resize(dot);
  base = LowerAscii(base);
  static const std::unordered_set<std::string> reserved = {
    "con", "prn", "aux", "nul", "clock$", "com1", "com2", "com3", "com4", "com5", "com6", "com7",
    "com8", "com9", "lpt1", "lpt2", "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9"};
  return reserved.contains(base);
}

std::optional<NormalizedPath> NormalizeImportPath(const std::string& original, const ImportLimits& limits,
                                                  std::string& diagnostic)
{
  diagnostic.clear();
  if (original.empty())
  {
    diagnostic = "Empty archive path";
    return std::nullopt;
  }
  if (original.find('\0') != std::string::npos || HasControlCharacter(original))
  {
    diagnostic = "Path contains a control or NUL character";
    return std::nullopt;
  }

  std::string value = original;
  std::replace(value.begin(), value.end(), '\\', '/');
  if (value.starts_with('/') || value.starts_with("//"))
  {
    diagnostic = "Absolute or UNC paths are not allowed";
    return std::nullopt;
  }
  if (value.size() >= 2 && std::isalpha(static_cast<unsigned char>(value[0])) && value[1] == ':')
  {
    diagnostic = "Drive-qualified paths are not allowed";
    return std::nullopt;
  }
  if (value.find(':') != std::string::npos)
  {
    diagnostic = "Colon and alternate-data-stream syntax are not allowed";
    return std::nullopt;
  }

  while (!value.empty() && value.back() == '/') value.pop_back();
  if (value.empty())
  {
    diagnostic = "Path resolves to an empty name";
    return std::nullopt;
  }
  if (value.size() > limits.maximumPathBytes)
  {
    diagnostic = "Normalized path is too long";
    return std::nullopt;
  }

  NormalizedPath result;
  std::size_t start = 0;
  while (start <= value.size())
  {
    const auto slash = value.find('/', start);
    const auto length = (slash == std::string::npos ? value.size() : slash) - start;
    const std::string component = value.substr(start, length);
    if (component.empty() || component == "." || component == "..")
    {
      diagnostic = "Empty, current, and parent path components are not allowed";
      return std::nullopt;
    }
    if (component.size() > limits.maximumComponentBytes)
    {
      diagnostic = "Path component is too long";
      return std::nullopt;
    }
    if (component.back() == ' ' || component.back() == '.')
    {
      diagnostic = "Path components ending in a dot or space are unsafe";
      return std::nullopt;
    }
    if (IsWindowsReservedComponent(component))
    {
      diagnostic = "Path contains a reserved device name";
      return std::nullopt;
    }
    result.components.push_back(component);
    if (slash == std::string::npos) break;
    start = slash + 1;
  }
  if (result.components.size() > limits.maximumDepth)
  {
    diagnostic = "Path exceeds the directory-depth limit";
    return std::nullopt;
  }

  std::ostringstream normalized;
  for (std::size_t i = 0; i < result.components.size(); ++i)
  {
    if (i != 0) normalized << '/';
    normalized << result.components[i];
  }
  result.value = normalized.str();
  return result;
}

std::string CaseCollisionKey(const std::string& normalized) { return LowerAscii(normalized); }

ImportedFileType ClassifyImportPath(const std::string& normalized)
{
  const auto name = LowerAscii(PathToUtf8(std::filesystem::u8path(normalized).filename()));
  const auto extension = LowerAscii(PathToUtf8(std::filesystem::u8path(name).extension()));
  if (extension == ".nam") return ImportedFileType::NamModel;
  if (extension == ".wav") return ImportedFileType::WaveImpulseResponse;
  if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".webp" ||
      extension == ".gif" || extension == ".svg")
    return ImportedFileType::Image;
  if (name == "readme" || name == "license" || name == "copying" || name == "manifest.json" ||
      name == "metadata.json" || name.starts_with("readme.") || name.starts_with("license.") ||
      name.starts_with("copying."))
    return ImportedFileType::MetadataText;
  return ImportedFileType::Unknown;
}

bool IsIgnoredPlatformPath(const std::string& normalized)
{
  const auto key = LowerAscii(normalized);
  const auto name = LowerAscii(PathToUtf8(std::filesystem::u8path(normalized).filename()));
  return key.starts_with("__macosx/") || name == ".ds_store" || name == "thumbs.db" || name == "desktop.ini" ||
         name.starts_with("._");
}

bool IsPathWithin(const std::filesystem::path& root, const std::filesystem::path& candidate)
{
  std::error_code error;
  const auto normalizedRoot = std::filesystem::weakly_canonical(root, error);
  if (error) return false;
  const auto normalizedCandidate = std::filesystem::weakly_canonical(candidate, error);
  if (error) return false;
  auto rootIt = normalizedRoot.begin();
  auto candidateIt = normalizedCandidate.begin();
  for (; rootIt != normalizedRoot.end(); ++rootIt, ++candidateIt)
  {
    if (candidateIt == normalizedCandidate.end()) return false;
#if defined(_WIN32)
    if (CaseCollisionKey(rootIt->string()) != CaseCollisionKey(candidateIt->string())) return false;
#else
    if (*rootIt != *candidateIt) return false;
#endif
  }
  return true;
}

} // namespace amphibia::library
