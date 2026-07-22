#include "ManagedLibrary.h"

#include "IPlugPaths.h"

namespace amphibia::library {

std::filesystem::path DefaultLibraryPath()
{
  WDL_String appSupport;
  iplug::AppSupportPath(appSupport, false);
  if (appSupport.GetLength() == 0)
    throw ImportException(ImportErrorCode::LibraryUnavailable, "Application-data path is unavailable");
  return std::filesystem::u8path(appSupport.Get()) / "Amphibia" / "Library";
}

} // namespace amphibia::library
