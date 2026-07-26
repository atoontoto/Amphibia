#include "FolderValidation.h"

#include <algorithm>
#include <cctype>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace amphibia::setup
{
FolderValidationResult ValidateLocalFolder(const std::filesystem::path& path, const std::string& extension,
                                           std::size_t maximumCount)
{
  FolderValidationResult result;
  std::error_code code;
  if (!std::filesystem::exists(path, code))
  {
    result.status = FolderValidationStatus::Missing;
    result.message = "The folder no longer exists.";
    return result;
  }
  if (!std::filesystem::is_directory(path, code))
  {
    result.status = FolderValidationStatus::NotDirectory;
    result.message = "The selected path is not a directory.";
    return result;
  }
#if defined(_WIN32)
  const auto attributes = GetFileAttributesW(path.c_str());
  if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
  {
    result.status = FolderValidationStatus::Redirected;
    result.message = "The selected folder is a reparse point; choose its verified target explicitly.";
    return result;
  }
#else
  if (std::filesystem::is_symlink(std::filesystem::symlink_status(path, code)))
  {
    result.status = FolderValidationStatus::Redirected;
    result.message = "The selected folder is a symbolic link; choose its verified target explicitly.";
    return result;
  }
#endif
  std::string wanted = extension;
  std::transform(wanted.begin(), wanted.end(), wanted.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  for (std::filesystem::directory_iterator iterator(path, std::filesystem::directory_options::skip_permission_denied, code), end;
       !code && iterator != end; iterator.increment(code))
  {
    if (!iterator->is_regular_file(code)) { code.clear(); continue; }
    const auto currentUtf8 = iterator->path().extension().u8string();
    std::string current(currentUtf8.begin(), currentUtf8.end());
    std::transform(current.begin(), current.end(), current.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (current == wanted)
    {
      ++result.compatibleFiles;
      if (result.compatibleFiles >= maximumCount) { result.countTruncated = true; break; }
    }
  }
  if (code)
  {
    result.status = FolderValidationStatus::Error;
    result.message = "The folder could not be read.";
    return result;
  }
  result.status = FolderValidationStatus::Available;
  result.message = "Folder is available.";
  return result;
}
} // namespace amphibia::setup
