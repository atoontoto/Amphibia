#include "../../NeuralAmpModeler/Library/ContentHash.h"
#include "../../NeuralAmpModeler/Library/FolderScanner.h"
#include "../../NeuralAmpModeler/Library/LibraryIndex.h"
#include "../../NeuralAmpModeler/Library/ImportWorker.h"
#include "../../NeuralAmpModeler/Library/ManagedLibrary.h"
#include "../../NeuralAmpModeler/Library/PathSafety.h"
#include "../../NeuralAmpModeler/Library/ZipArchive.h"

#include "zlib.h"
#include "mz.h"

#include <windows.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace amphibia::library;

namespace {

void Require(bool condition, const char* message)
{
  if (!condition) throw std::runtime_error(message);
}

struct TemporaryDirectory
{
  std::filesystem::path path;
  TemporaryDirectory()
  {
    path = std::filesystem::temp_directory_path() /
           ("amphibia-m3-test-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(++counter));
    std::filesystem::create_directories(path);
  }
  ~TemporaryDirectory()
  {
    std::error_code error;
    std::filesystem::remove_all(path, error);
  }
  static inline std::atomic<unsigned> counter{};
};

void WriteBytes(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes)
{
  std::filesystem::create_directories(path.parent_path());
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  if (!output) throw std::runtime_error("test file write failed");
}

void WriteText(const std::filesystem::path& path, const std::string& text)
{
  WriteBytes(path, std::vector<std::uint8_t>(text.begin(), text.end()));
}

std::vector<std::uint8_t> WaveBytes()
{
  std::vector<std::uint8_t> bytes;
  auto u16 = [&](std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8));
  };
  auto u32 = [&](std::uint32_t value) {
    for (int i = 0; i < 4; ++i) bytes.push_back(static_cast<std::uint8_t>(value >> (i * 8)));
  };
  const std::array<std::int16_t, 8> samples{0, 100, -100, 200, -200, 50, -50, 0};
  bytes.insert(bytes.end(), {'R', 'I', 'F', 'F'});
  u32(36 + static_cast<std::uint32_t>(samples.size() * 2));
  bytes.insert(bytes.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
  u32(16); u16(1); u16(1); u32(48000); u32(96000); u16(2); u16(16);
  bytes.insert(bytes.end(), {'d', 'a', 't', 'a'});
  u32(static_cast<std::uint32_t>(samples.size() * 2));
  for (const auto sample : samples) u16(static_cast<std::uint16_t>(sample));
  return bytes;
}

struct ZipInput
{
  std::string path;
  std::vector<std::uint8_t> data;
  std::uint32_t externalAttributes{};
  std::uint16_t flags{MZ_ZIP_FLAG_UTF8};
  std::uint16_t method{};
  bool corruptCrc{};
};

void Put16(std::ofstream& output, std::uint16_t value)
{
  const std::array<char, 2> bytes{static_cast<char>(value), static_cast<char>(value >> 8)};
  output.write(bytes.data(), bytes.size());
}

void Put32(std::ofstream& output, std::uint32_t value)
{
  const std::array<char, 4> bytes{static_cast<char>(value), static_cast<char>(value >> 8),
                                  static_cast<char>(value >> 16), static_cast<char>(value >> 24)};
  output.write(bytes.data(), bytes.size());
}

void WriteStoredZip(const std::filesystem::path& path, const std::vector<ZipInput>& entries)
{
  struct Central { ZipInput input; std::uint32_t offset{}; std::uint32_t crc{}; };
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  std::vector<Central> central;
  for (const auto& input : entries)
  {
    Central item{input, static_cast<std::uint32_t>(output.tellp()),
                 static_cast<std::uint32_t>(crc32(0, input.data.data(), static_cast<uInt>(input.data.size())))};
    if (input.corruptCrc) item.crc ^= 0xffffffffu;
    Put32(output, 0x04034b50); Put16(output, 20); Put16(output, input.flags); Put16(output, input.method);
    Put16(output, 0); Put16(output, 0); Put32(output, item.crc); Put32(output, static_cast<std::uint32_t>(input.data.size()));
    Put32(output, static_cast<std::uint32_t>(input.data.size())); Put16(output, static_cast<std::uint16_t>(input.path.size()));
    Put16(output, 0); output.write(input.path.data(), static_cast<std::streamsize>(input.path.size()));
    output.write(reinterpret_cast<const char*>(input.data.data()), static_cast<std::streamsize>(input.data.size()));
    central.push_back(std::move(item));
  }
  const auto centralOffset = static_cast<std::uint32_t>(output.tellp());
  for (const auto& item : central)
  {
    Put32(output, 0x02014b50); Put16(output, static_cast<std::uint16_t>((3 << 8) | 20)); Put16(output, 20);
    Put16(output, item.input.flags); Put16(output, item.input.method); Put16(output, 0); Put16(output, 0); Put32(output, item.crc);
    Put32(output, static_cast<std::uint32_t>(item.input.data.size()));
    Put32(output, static_cast<std::uint32_t>(item.input.data.size()));
    Put16(output, static_cast<std::uint16_t>(item.input.path.size())); Put16(output, 0); Put16(output, 0); Put16(output, 0);
    Put16(output, 0); Put32(output, item.input.externalAttributes); Put32(output, item.offset);
    output.write(item.input.path.data(), static_cast<std::streamsize>(item.input.path.size()));
  }
  const auto centralSize = static_cast<std::uint32_t>(output.tellp()) - centralOffset;
  Put32(output, 0x06054b50); Put16(output, 0); Put16(output, 0); Put16(output, static_cast<std::uint16_t>(central.size()));
  Put16(output, static_cast<std::uint16_t>(central.size())); Put32(output, centralSize); Put32(output, centralOffset); Put16(output, 0);
}

void TestHashing()
{
  Require(ToHex(HashBytes({})) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
          "empty SHA-256 vector failed");
  const std::string abc = "abc";
  Require(ToHex(HashBytes(std::span(reinterpret_cast<const std::uint8_t*>(abc.data()), abc.size()))) ==
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
          "abc SHA-256 vector failed");
  Require(ContentHashFromHex(ToHex(HashBytes({}))) == HashBytes({}), "hash hex round trip failed");

  TemporaryDirectory temporary;
  std::vector<std::uint8_t> large(3 * 1024 * 1024 + 17, 0x5a);
  WriteBytes(temporary.path / "large.bin", large);
  Require(HashFile(temporary.path / "large.bin") == HashBytes(large), "streamed SHA-256 differs from byte hash");
  bool cancelled = false;
  try
  {
    HashFile(temporary.path / "large.bin", [] { return true; });
  }
  catch (...) { cancelled = true; }
  Require(cancelled, "hash cancellation was ignored");

  bool readFailure = false;
  try { (void)HashFile(temporary.path / "missing.bin"); }
  catch (const ImportException& exception) { readFailure = exception.Code() == ImportErrorCode::HashFailure; }
  Require(readFailure, "hash read failure was not structured");
}

void TestPathSafety()
{
  ImportLimits limits;
  std::string message;
  const auto safe = NormalizeImportPath("Pack\\Nested/Model.nam", limits, message);
  Require(safe && safe->value == "Pack/Nested/Model.nam", "safe path normalization failed");
  for (const auto* unsafe : {"../outside.nam", "C:/Windows/file.nam", "//server/share/file.wav", "/root/file.wav",
                             R"(\\server\share\file.wav)", R"(\\?\C:\device.nam)", "folder/.. /file.nam",
                             "folder/con.nam", "folder/file.nam:stream", "folder//file.nam"})
    Require(!NormalizeImportPath(unsafe, limits, message), "unsafe path was accepted");
  Require(CaseCollisionKey("Amp/Gain.nam") == CaseCollisionKey("amp/gain.nam"), "case collision key failed");

  ImportLimits shallow;
  shallow.maximumDepth = 2;
  Require(!NormalizeImportPath("one/two/three.nam", shallow, message), "path depth limit was ignored");
}

void TestFolderScan()
{
  TemporaryDirectory temporary;
  WriteText(temporary.path / "one.nam", R"({"architecture":"Linear","config":{},"weights":[]})");
  WriteBytes(temporary.path / "nested" / "ir.wav", WaveBytes());
  WriteText(temporary.path / std::filesystem::u8path(u8"nested/Ünicode.NAM"),
            R"({"architecture":"Linear","config":{},"weights":[]})");
  WriteText(temporary.path / "nested" / "ignore.txt", "ignored");
  const auto plan = ScanFolder(temporary.path);
  Require(plan.entries.size() == 4, "folder scan entry count failed");
  Require(std::count_if(plan.entries.begin(), plan.entries.end(), [](const auto& entry) { return entry.selected; }) == 3,
          "folder candidate selection failed");

  TemporaryDirectory empty;
  Require(ScanFolder(empty.path).entries.empty(), "empty folder scan failed");
  bool cancelled = false;
  try { (void)ScanFolder(temporary.path, {}, [] { return true; }); }
  catch (const ImportException& exception) { cancelled = exception.Code() == ImportErrorCode::Cancelled; }
  Require(cancelled, "folder scan cancellation failed");
  ImportLimits countLimit;
  countLimit.maximumEntries = 1;
  bool countRejected = false;
  try { (void)ScanFolder(temporary.path, countLimit); }
  catch (const ImportException& exception) { countRejected = exception.Code() == ImportErrorCode::TooManyEntries; }
  Require(countRejected, "folder file-count limit was ignored");

  std::error_code linkError;
  std::filesystem::create_directory_symlink(temporary.path / "nested", temporary.path / "linked", linkError);
  if (!linkError)
  {
    const auto linkedPlan = ScanFolder(temporary.path);
    Require(std::none_of(linkedPlan.entries.begin(), linkedPlan.entries.end(), [](const auto& entry) {
      return entry.normalizedRelativePath.starts_with("linked/");
    }), "folder scan followed a directory symlink");
  }
}

void TestZipSecurityAndSelection()
{
  TemporaryDirectory temporary;
  const std::string nam = R"({"architecture":"Linear","config":{},"weights":[]})";
  const auto wave = WaveBytes();
  WriteStoredZip(temporary.path / "empty.zip", {});
  Require(ScanZipArchive(temporary.path / "empty.zip").entries.empty(), "empty ZIP scan failed");
  WriteStoredZip(temporary.path / "pack.zip",
                 {{"Models/High Gain.nam", {nam.begin(), nam.end()}}, {"IR/Cab.wav", wave},
                  {"README.md", {'t', 'e', 'x', 't'}}, {"unsupported.exe", {'M', 'Z'}},
                  {"../escape.nam", {nam.begin(), nam.end()}}, {"Amp/Gain.nam", {nam.begin(), nam.end()}},
                  {"amp/gain.nam", {nam.begin(), nam.end()}},
                  {"link.nam", {nam.begin(), nam.end()}, static_cast<std::uint32_t>(0120000u << 16)}});
  auto plan = ScanZipArchive(temporary.path / "pack.zip");
  Require(plan.unsafeEntries >= 4, "ZIP unsafe/collision entries were not rejected");
  Require(std::count_if(plan.entries.begin(), plan.entries.end(), [](const auto& entry) { return entry.selected; }) == 2,
          "ZIP selected candidate count failed");

  const auto staging = temporary.path / "staging";
  const auto extracted = ExtractSelectedZipEntries(plan, staging);
  Require(extracted.size() == 2, "selective ZIP extraction count failed");
  Require(!std::filesystem::exists(temporary.path / "escape.nam"), "ZIP extraction escaped staging");
  for (const auto& item : extracted) Require(IsPathWithin(staging, item.stagingPath), "extracted path escaped staging");

  WriteStoredZip(temporary.path / "traversal.zip", {{"C:/bad.nam", {nam.begin(), nam.end()}}});
  const auto badPlan = ScanZipArchive(temporary.path / "traversal.zip");
  Require(badPlan.entries.front().status == ImportEntryStatus::UnsafePath, "drive ZIP path was accepted");

  WriteStoredZip(temporary.path / "absolute.zip",
                 {{"/root.nam", {nam.begin(), nam.end()}}, {"//server/share/ir.wav", wave},
                  {"folder/file.nam:stream", {nam.begin(), nam.end()}},
                  {"one/two/three/model.nam", {nam.begin(), nam.end()}}});
  ImportLimits shallow;
  shallow.maximumDepth = 3;
  const auto absolutePlan = ScanZipArchive(temporary.path / "absolute.zip", shallow);
  Require(std::all_of(absolutePlan.entries.begin(), absolutePlan.entries.end(), [](const auto& entry) {
    return entry.status == ImportEntryStatus::UnsafePath;
  }), "ZIP absolute/UNC/ADS/depth paths were accepted");

  WriteStoredZip(temporary.path / "policies.zip",
                 {{"encrypted.nam", {nam.begin(), nam.end()}, 0,
                    static_cast<std::uint16_t>(MZ_ZIP_FLAG_UTF8 | MZ_ZIP_FLAG_ENCRYPTED)},
                  {"method.nam", {nam.begin(), nam.end()}, 0, MZ_ZIP_FLAG_UTF8, 99}});
  const auto policies = ScanZipArchive(temporary.path / "policies.zip");
  Require(policies.entries[0].status == ImportEntryStatus::Encrypted &&
            policies.entries[1].status == ImportEntryStatus::Unsupported,
          "ZIP encryption/compression policies failed");
  ImportLimits oneEntry;
  oneEntry.maximumEntries = 1;
  bool countRejected = false;
  try { (void)ScanZipArchive(temporary.path / "policies.zip", oneEntry); }
  catch (const ImportException& exception) { countRejected = exception.Code() == ImportErrorCode::TooManyEntries; }
  Require(countRejected, "ZIP entry-count limit was not enforced");

  ImportLimits entryLimit;
  entryLimit.maximumNamBytes = nam.size() - 1;
  const auto oversized = ScanZipArchive(temporary.path / "traversal.zip", entryLimit);
  Require(oversized.entries.front().status == ImportEntryStatus::UnsafePath,
          "unsafe path classification was changed by size policy");
  WriteStoredZip(temporary.path / "large-entry.zip", {{"large.nam", {nam.begin(), nam.end()}}});
  const auto largeEntry = ScanZipArchive(temporary.path / "large-entry.zip", entryLimit);
  Require(largeEntry.entries.front().status == ImportEntryStatus::TooLarge, "ZIP entry-size limit was ignored");
  ImportLimits totalLimit;
  totalLimit.maximumExtractedBytes = nam.size() - 1;
  bool totalRejected = false;
  try { (void)ScanZipArchive(temporary.path / "large-entry.zip", totalLimit); }
  catch (const ImportException& exception) { totalRejected = exception.Code() == ImportErrorCode::ArchiveTooLarge; }
  Require(totalRejected, "ZIP total-size limit was ignored");

  bool scanCancelled = false;
  try { (void)ScanZipArchive(temporary.path / "pack.zip", {}, [] { return true; }); }
  catch (const ImportException& exception) { scanCancelled = exception.Code() == ImportErrorCode::Cancelled; }
  Require(scanCancelled, "ZIP scan cancellation failed");

  WriteStoredZip(temporary.path / "crc.zip", {{"bad.nam", {nam.begin(), nam.end()}, 0,
                                                MZ_ZIP_FLAG_UTF8, 0, true}});
  auto crcPlan = ScanZipArchive(temporary.path / "crc.zip");
  bool crcRejected = false;
  try { (void)ExtractSelectedZipEntries(crcPlan, temporary.path / "crc-staging"); }
  catch (const ImportException&) { crcRejected = true; }
  Require(crcRejected, "ZIP CRC mismatch was not rejected");
  bool extractionCancelled = false;
  try { (void)ExtractSelectedZipEntries(plan, temporary.path / "cancel-staging", {}, [] { return true; }); }
  catch (const ImportException& exception) { extractionCancelled = exception.Code() == ImportErrorCode::Cancelled; }
  Require(extractionCancelled, "ZIP extraction cancellation failed");

  bool noOverwrite = false;
  try { (void)ExtractSelectedZipEntries(plan, staging); }
  catch (const ImportException& exception) { noOverwrite = exception.Code() == ImportErrorCode::ExtractionFailure; }
  Require(noOverwrite, "ZIP extraction overwrote existing staging files");
  Require(HashFile(temporary.path / "pack.zip") == *plan.sourceArchiveHash, "source ZIP changed during import");

  WriteText(temporary.path / "invalid.zip", "not a ZIP");
  bool invalidRejected = false;
  try { (void)ScanZipArchive(temporary.path / "invalid.zip"); }
  catch (const ImportException& exception) { invalidRejected = exception.Code() == ImportErrorCode::InvalidArchive; }
  Require(invalidRejected, "invalid ZIP central directory was accepted");
}

void TestLibraryDedupAndTransactions()
{
  TemporaryDirectory temporary;
  const auto source = temporary.path / "source";
  const std::string nam = R"({"architecture":"Linear","config":{},"weights":[]})";
  WriteText(source / "First.nam", nam);
  WriteText(source / "Second.nam", nam);
  WriteBytes(source / "Cab.wav", WaveBytes());

  ManagedLibrary library(temporary.path / "library");
  library.Initialize();
  auto summary = library.ImportFiles({source / "First.nam", source / "Second.nam", source / "Cab.wav"}, "Files");
  Require(summary.failures.empty() && summary.objects.size() == 3, "managed file import failed");
  const auto otherSummary = library.ImportFiles({source / "First.nam"}, "Other pack");
  Require(otherSummary.failures.empty() && otherSummary.objects.size() == 1, "second pack association failed");
  const auto stats = library.Statistics();
  Require(stats.namObjects == 1 && stats.irObjects == 1 && stats.packs == 2, "content deduplication failed");
  const auto snapshot = library.Snapshot();
  Require(snapshot.packEntries.size() == 4 && snapshot.objects.size() == 2, "pack associations were not retained");
  Require(library.Resolve(summary.objects.front(), ManagedObjectType::NamModel).has_value(), "managed resolution failed");
  Require(library.ObjectPath(summary.objects.front(), ManagedObjectType::NamModel).filename() ==
            (ToHex(summary.objects.front()) + ".nam"), "content-addressed object path failed");
  const auto verify = library.Verify(true);
  Require(verify.issues.empty() && verify.checkedObjects == 2, "library verification failed");

  const auto original = snapshot;
  LibraryIndex index(temporary.path / "library" / "metadata");
  auto changed = snapshot;
  ++changed.revision;
  index.Commit(changed);
  Require(index.Load().revision == changed.revision, "schema reopen failed");

  const auto corruptRoot = temporary.path / "future";
  std::filesystem::create_directories(corruptRoot);
  WriteText(corruptRoot / "index-v1.json", R"({"schema_version":999,"revision":0})");
  bool rejected = false;
  try { LibraryIndex(corruptRoot).Load(); } catch (const ImportException&) { rejected = true; }
  Require(rejected, "future metadata schema was not rejected");

  auto filesPack = std::find_if(snapshot.packs.begin(), snapshot.packs.end(), [](const auto& item) {
    return item.second.displayName == "Files";
  });
  Require(filesPack != snapshot.packs.end(), "first pack metadata missing");
  library.RemovePack(filesPack->first);
  Require(library.Statistics().namObjects == 1, "pack removal deleted a shared physical object");
  const auto protectedHash = ToHex(summary.objects.front());
  Require(library.RemoveUnused({protectedHash}) == 1 && library.Statistics().namObjects == 1,
          "active-object cleanup protection failed");
  const auto remaining = library.Snapshot();
  library.RemovePack(remaining.packs.begin()->first);
  Require(library.RemoveUnused() == 1 && library.Statistics().namObjects == 0,
          "association-derived unused cleanup failed");

  WriteText(temporary.path / "library" / "staging" / "abandoned" / "partial", "partial");
  ManagedLibrary reopened(temporary.path / "library");
  reopened.Initialize();
  Require(!std::filesystem::exists(temporary.path / "library" / "staging" / "abandoned"),
          "startup staging recovery failed");

  WriteText(temporary.path / "outside" / "keep.txt", "keep");
  std::error_code stagingLinkError;
  std::filesystem::create_directory_symlink(temporary.path / "outside",
                                             temporary.path / "library" / "staging" / "redirected",
                                             stagingLinkError);
  if (!stagingLinkError)
  {
    reopened.ClearStaging();
    Require(std::filesystem::exists(temporary.path / "outside" / "keep.txt"),
            "staging cleanup traversed a directory link");
  }

  ManagedLibrary cancelledLibrary(temporary.path / "cancelled-library");
  cancelledLibrary.Initialize();
  int cancellationChecks = 0;
  bool importCancelled = false;
  try
  {
    (void)cancelledLibrary.ImportFiles({source / "First.nam"}, "Cancelled", LocalContentSource::ManagedImport, {},
                                       [&] { return ++cancellationChecks > 1; });
  }
  catch (const ImportException& exception) { importCancelled = exception.Code() == ImportErrorCode::Cancelled; }
  Require(importCancelled && cancelledLibrary.Snapshot().objects.empty(),
          "cancelled import published managed metadata");

  const auto malformedRoot = temporary.path / "malformed";
  std::filesystem::create_directories(malformedRoot);
  WriteText(malformedRoot / "index-v1.json", "{not-json");
  bool malformedRejected = false;
  try { (void)LibraryIndex(malformedRoot).Load(); } catch (const ImportException&) { malformedRejected = true; }
  Require(malformedRejected, "corrupt metadata was accepted");

  const auto realRoot = temporary.path / "real-library-root";
  std::filesystem::create_directories(realRoot);
  std::error_code rootLinkError;
  const auto linkedRoot = temporary.path / "linked-library-root";
  std::filesystem::create_directory_symlink(realRoot, linkedRoot, rootLinkError);
  if (!rootLinkError)
  {
    bool linkedRootRejected = false;
    try { ManagedLibrary(linkedRoot).Initialize(); } catch (const ImportException&) { linkedRootRejected = true; }
    Require(linkedRootRejected && std::filesystem::is_empty(realRoot),
            "redirected library root was modified before rejection");
  }

  WriteText(source / "Different.nam", nam + "\n");
  ManagedLibrary distinct(temporary.path / "distinct-library");
  distinct.Initialize();
  const auto distinctSummary = distinct.ImportFiles({source / "First.nam", source / "Different.nam"}, "Distinct");
  Require(distinctSummary.failures.empty() && distinct.Statistics().namObjects == 2,
          "different bytes were incorrectly deduplicated");
  bool verifyCancelled = false;
  try { (void)distinct.Verify(true, [] { return true; }); }
  catch (const ImportException& exception) { verifyCancelled = exception.Code() == ImportErrorCode::Cancelled; }
  Require(verifyCancelled, "library verification cancellation was converted into a validation issue");

  const auto distinctSnapshot = distinct.Snapshot();
  const auto corruptObject = distinct.Root() / distinctSnapshot.objects.begin()->second.libraryRelativePath;
  WriteText(corruptObject, "broken");
  const auto corruptReport = distinct.Verify(true);
  Require(!corruptReport.issues.empty(), "managed object corruption was not detected");
  WriteText(distinct.Root() / "objects" / "nam" / "orphan" / "unknown.nam", nam);
  Require(distinct.Verify(false).orphanedFiles == 1, "orphaned managed file was not reported");
}

void TestInvalidFilesDoNotPromote()
{
  TemporaryDirectory temporary;
  WriteText(temporary.path / "bad.nam", "not json");
  WriteText(temporary.path / "bad.wav", "not wave");
  ManagedLibrary library(temporary.path / "library");
  library.Initialize();
  const auto summary = library.ImportFiles({temporary.path / "bad.nam", temporary.path / "bad.wav"}, "Invalid");
  Require(summary.objects.empty() && summary.failures.size() == 2, "invalid files entered managed storage");
  Require(library.Statistics().namObjects == 0 && library.Statistics().irObjects == 0, "invalid objects were published");
}

void TestImportWorkerLifecycle()
{
  TemporaryDirectory temporary;
  const std::string nam = R"({"architecture":"Linear","config":{},"weights":[]})";
  WriteText(temporary.path / "source" / "Worker.nam", nam);
  WriteBytes(temporary.path / "source" / "Worker.wav", WaveBytes());
  ManagedLibrary library(temporary.path / "library");
  library.Initialize();
  ImportWorker worker(library);
  const auto scanTask = worker.Scan({temporary.path / "source"});
  for (int attempt = 0; attempt < 500 && worker.Progress().state == ImportTaskState::Scanning; ++attempt)
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  const auto plan = worker.Plan();
  Require(plan && worker.Progress().taskId == scanTask &&
            worker.Progress().state == ImportTaskState::AwaitingSelection,
          "background scan did not reach review state");
  std::vector<std::size_t> selected;
  for (std::size_t index = 0; index < plan->entries.size(); ++index)
    if (plan->entries[index].selected) selected.push_back(index);
  const auto importTask = worker.ImportSelected(selected);
  for (int attempt = 0; attempt < 1000; ++attempt)
  {
    const auto state = worker.Progress().state;
    if (state == ImportTaskState::Completed || state == ImportTaskState::Failed) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  Require(worker.Progress().taskId == importTask && worker.Progress().state == ImportTaskState::Completed,
          "background import did not complete");
  Require(worker.Summary() && worker.Summary()->objects.size() == 2,
          "background import summary is incomplete");
  worker.Cancel();
  Require(worker.Progress().state == ImportTaskState::Cancelled, "worker cancellation state was not published");
  worker.Shutdown();
}

void TestPerformanceSmoke()
{
  using Clock = std::chrono::steady_clock;
  const auto milliseconds = [](Clock::time_point start, Clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
  };
  TemporaryDirectory temporary;

  std::vector<std::uint8_t> hashData(32 * 1024 * 1024, 0xa5);
  WriteBytes(temporary.path / "hash.bin", hashData);
  auto start = Clock::now();
  (void)HashFile(temporary.path / "hash.bin");
  const auto hashMs = milliseconds(start, Clock::now());

  const auto folder = temporary.path / "folder";
  for (int index = 0; index < 500; ++index)
    WriteText(folder / ("item-" + std::to_string(index) + ".txt"), "metadata");
  start = Clock::now();
  const auto folderPlan = ScanFolder(folder);
  const auto folderMs = milliseconds(start, Clock::now());

  const std::string nam = R"({"architecture":"Linear","config":{},"weights":[]})";
  std::vector<ZipInput> inputs;
  for (int index = 0; index < 100; ++index)
    inputs.push_back({"Group/Model-" + std::to_string(index) + ".nam", {nam.begin(), nam.end()}});
  WriteStoredZip(temporary.path / "performance.zip", inputs);
  start = Clock::now();
  const auto zipPlan = ScanZipArchive(temporary.path / "performance.zip");
  const auto zipScanMs = milliseconds(start, Clock::now());
  start = Clock::now();
  const auto extracted = ExtractSelectedZipEntries(zipPlan, temporary.path / "performance-staging");
  const auto extractionMs = milliseconds(start, Clock::now());

  LibrarySnapshot snapshot;
  for (std::uint16_t index = 0; index < 500; ++index)
  {
    LibraryObject object;
    object.hash.bytes[0] = static_cast<std::uint8_t>(index >> 8);
    object.hash.bytes[1] = static_cast<std::uint8_t>(index);
    object.libraryRelativePath = "objects/nam/00/" + ToHex(object.hash) + ".nam";
    object.originalDisplayName = "Model " + std::to_string(index);
    snapshot.objects.emplace(ToHex(object.hash), std::move(object));
  }
  LibraryIndex index(temporary.path / "performance-metadata");
  start = Clock::now();
  index.Commit(snapshot);
  const auto commitMs = milliseconds(start, Clock::now());
  start = Clock::now();
  const auto reopened = index.Load();
  const auto openMs = milliseconds(start, Clock::now());
  Require(folderPlan.entries.size() == 500 && extracted.size() == 100 && reopened.objects.size() == 500,
          "performance smoke setup failed");

  std::cout << std::fixed << std::setprecision(2)
            << "performance hash_32_mib_ms=" << hashMs
            << " hash_mib_per_s=" << (32.0 * 1000.0 / hashMs)
            << " folder_scan_500_ms=" << folderMs
            << " zip_scan_100_ms=" << zipScanMs
            << " zip_extract_100_ms=" << extractionMs
            << " metadata_commit_500_ms=" << commitMs
            << " metadata_open_500_ms=" << openMs << '\n';
}

} // namespace

int main()
{
  try
  {
    std::cout << "hashing\n" << std::flush;
    TestHashing();
    std::cout << "paths\n" << std::flush;
    TestPathSafety();
    std::cout << "folder\n" << std::flush;
    TestFolderScan();
    std::cout << "zip\n" << std::flush;
    TestZipSecurityAndSelection();
    std::cout << "library\n" << std::flush;
    TestLibraryDedupAndTransactions();
    std::cout << "invalid\n" << std::flush;
    TestInvalidFilesDoNotPromote();
    std::cout << "worker\n" << std::flush;
    TestImportWorkerLifecycle();
    std::cout << "performance\n" << std::flush;
    TestPerformanceSmoke();
    std::cout << "Milestone 3 library tests passed\n";
    return 0;
  }
  catch (const std::exception& exception)
  {
    std::cerr << "Milestone 3 library tests failed: " << exception.what() << '\n';
    return 1;
  }
}
