# Settings ownership

| Setting | Standalone | VST3 | Shared |
| --- | --- | --- | --- |
| Backend/device/channels | owns | DAW owns | no |
| Sample rate/buffer | owns | DAW owns | no |
| Setup completion/version/time | owns | never launches from it | no |
| Preferred NAM folder | configures | configures | yes |
| Preferred IR folder | configures | configures | yes |
| Managed library/index | status and explicit actions | status and explicit actions | yes |
| TONE3000 status/actions | optional | optional | secure credential namespace |
| Active model/IR/parameters | application session | DAW project/instance | no |

The VST3 Configuration panel explicitly states that the DAW owns audio. It
cannot enumerate/open standalone devices or write standalone audio/setup
fields. Folder writes reload the document, merge only `shared`, and atomically
replace it under a bounded cross-process lock. Standalone writes likewise merge
only `standalone` where that API is used. The conflict policy for shared paths
is last successful complete write; a future schema is never overwritten.

Preferred folders seed same-directory navigation only when no model/IR restored
from project state is active. They are application preferences, not DAW state.
Tokens, authorization codes, PKCE values, signed URLs, client secrets, and test
state are not settings.

## Windows deployment ownership

The installer owns binaries, installed documentation, shortcuts, and its
uninstall record. It neither seeds nor rewrites `settings.ini`,
`settings-v1.json`, or its backup. Upgrade and default uninstall preserve the
entire fixed `%LOCALAPPDATA%\Amphibia` settings root. Silent uninstall always
preserves it. A separate default-No interactive cleanup may remove that fixed
root after path/reparse validation; it is not a settings migration.
