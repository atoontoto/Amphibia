# First-run setup

The Windows standalone application owns a native nine-page setup wizard. It
appears automatically only when Amphibia has no setup state for a genuinely new
installation. An existing `settings.ini` is treated as an upgrade: its active
audio configuration is offered as migration input and the user is invited to
review setup without losing existing behavior.

The pages are Welcome, Audio device, Input/output test, Local NAM folder, Local
IR folder, Managed library, TONE3000, Summary, and Finish. Audio, folders,
library verification, and TONE3000 are independently recoverable; optional
content/provider choices do not prevent completion.

## Commands and rerun paths

The standalone accepts only these options:

```text
--first-run       Force the wizard from Welcome
--setup           Open the wizard
--audio-settings  Open the audio page
--safe-mode       Start without automatic previous model/IR restoration
--help            Show bounded help and exit before audio initialization
--version         Show version/build commit and exit before audio initialization
```

Unknown options are rejected and never interpreted as paths or shell input.
Use the application menu commands `Run Setup Wizard Again`, `Audio Settings`,
and `Restart in Safe Mode` after first launch.

Closing the window or ordinary Cancel records `in_progress`; an explicit Skip
records `skipped`. Neither marks setup complete. Finish performs the single
durable commit, records the app version and completion time, and prevents the
wizard from reappearing on a normal restart. A missing saved audio device offers
an explicit repair prompt rather than silently forcing the wizard.

## Content and privacy

NAM and IR folder choices are references. Selection validates only the chosen
directory and counts compatible files in that directory; it does not recurse,
import, hash, copy, rename, delete, upload, or contact TONE3000. Managed import
is a separate reviewed action. The input meter publishes only a peak number;
input samples are not recorded, retained, or uploaded.

TONE3000 is optional. Entering its page starts no browser, credential, or
network operation. The repository build reports that production integration is
unconfigured and permits setup to finish normally.

## Platform status

The native wizard is implemented and compiled for the Windows standalone. It
is never part of the VST3 startup/scan/state path. The macOS standalone still
uses the pinned upstream APP sources and Milestone 6 wizard behavior has not
been implemented or validated there.
