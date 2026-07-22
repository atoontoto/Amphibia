# Secure Local ZIP Import

Amphibia vendors minizip-ng 4.2.2 and zlib 1.3.2 as a reader-only ZIP stack. Runtime downloading and archive writing are absent. Traditional/AES encryption and all crypto, writer compression, bzip2, LZMA, zstd, iconv, and OpenSSL integrations are disabled. Stored and deflated entries, Unicode names, ZIP64 metadata, streaming reads, and CRC checks remain.

## Two phases

Scan opens the user-owned archive read-only, hashes it, enumerates the central directory, normalizes paths, classifies entries, calculates declared totals, and returns an immutable plan. It does not extract NAM/WAV candidates. Confirmation selects candidate indices.

Extraction reopens the archive, visits only selected candidate indices, and writes each to a randomized Amphibia staging root under a generated `entry-N.nam` or `entry-N.wav` name. Reads are streamed in 256 KiB blocks. Actual byte limits, declared size equality, CRC, cancellation, free space, and final format/hash verification are enforced before content-addressed publication. The source ZIP is never changed.

## Path and entry policy

Both `/` and `\` are separators. The scanner rejects absolute/rooted paths, drive-qualified paths, UNC/device paths, colons/alternate streams, NUL/control characters, empty/current/parent components, trailing dot/space aliases, Windows device names, excessive bytes/components/depth, duplicate normalized paths, and case-folded collisions. Both members of a collision are unsafe. Symlinks, hard links, devices, and other special Unix modes are rejected. ZIP source symlinks and Windows reparse points are rejected.

Platform junk and directories are ignored. NAM and WAV are selected by default. Small README/license/plain metadata is recorded as a candidate classification but is not promoted into executable/model object storage. Images and unrelated formats are unsupported and never executed or rendered as markup.

## Default limits

| Limit | Value |
| --- | ---: |
| ZIP bytes | 2 GiB |
| entries | 10,000 |
| path depth | 32 |
| normalized path | 512 bytes |
| component | 255 bytes |
| NAM entry | 512 MiB |
| WAV entry | 64 MiB |
| metadata entry | 10 MiB |
| metadata files | 32 |
| selected/declared total | 8 GiB |
| expansion ratio | 200:1 |

Negative sizes, multidisk entries, encryption, unsupported methods, overflows, and ratios over policy are rejected. Free disk space is checked before extraction. OS write errors, including disk-full, abort the task and staging is removed. Cancellation is checked during archive hashing, enumeration, every extraction block, validation, hashing, copy, and commit boundary.
