#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

namespace amphibia::setup
{
enum class FolderValidationStatus { Available, Missing, NotDirectory, Redirected, Error };

struct FolderValidationResult
{
  FolderValidationStatus status{FolderValidationStatus::Error};
  std::size_t compatibleFiles{};
  bool countTruncated{};
  std::string message;
};

FolderValidationResult ValidateLocalFolder(const std::filesystem::path& path, const std::string& extension,
                                           std::size_t maximumCount = 100000);
} // namespace amphibia::setup
