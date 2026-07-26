# Troubleshooting setup

## The wizard appeared again

Cancel/window close leaves setup `in_progress`; use Finish to mark it complete.
An explicit repair prompt can also appear when saved audio cannot reopen. Use
`--setup` or the menu command when you intentionally want to review everything.

## No audio or a missing device

Open Audio Settings, refresh devices, then select a current backend/output,
channel pair, supported sample rate, and buffer. Output-only mode is supported.
If a new selection fails, Amphibia attempts to restore the previous working
stream. Continuing without audio does not disable local file/library setup.

## The test tone or input meter does not work

The test requires an active output callback and an explicit confirmation. The
meter requires a configured input rather than output-only mode. Amphibia does
not record or upload the signal. Lower physical volume before testing.

## A NAM or IR folder is unavailable

Choose the actual directory rather than a file, symlink, or reparse point.
Amphibia does not follow redirected folder selections. A removed folder is
reported; choose it again after reconnecting the drive or Skip for Now.

## Managed library warning

Open the Managed library page/panel to view schema, counts, task state, and the
last verification result. Verify and Clean Staging run in the background and
can be cancelled. Amphibia does not relocate or automatically delete managed
objects. If the library is unavailable/read-only/unsupported, referenced local
NAM and IR files remain usable.

## TONE3000 is unavailable

The repository build is intentionally unconfigured. Entering the page does not
connect. Local files and managed local imports still work, and setup may finish.
Never place tokens or signed URLs in the JSON settings file.

## Recovery commands

- `--audio-settings`: repair standalone audio.
- `--safe-mode`: suppress automatic prior model/IR restoration.
- `--first-run`: force a complete setup review.
- `--help`: show the exact supported option set without initializing audio.

`settings-v1.json.bak` is automatic recovery evidence. Preserve corrupt files
when reporting a bug, but redact local paths/device names and never attach
credentials, signed URLs, private models, or hardware serial numbers.
