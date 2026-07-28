# Uninstall Amphibia on Windows

Close Amphibia and every DAW, then use **Settings > Apps > Installed apps >
Amphibia > Uninstall** or the Start Menu uninstall shortcut.

The uninstaller removes only Amphibia-owned installed files, shortcuts, and the
uninstall registration. By default it preserves:

- `%LOCALAPPDATA%\Amphibia`, including settings, managed library, cache, and logs;
- the exact Amphibia TONE3000 credential in Windows Credential Manager;
- every external NAM folder, external IR folder, and original ZIP archive.

An interactive uninstall offers a separate warning with **No** as the default.
Choosing **Yes** removes only the fixed `%LOCALAPPDATA%\Amphibia` tree and the
exact credential target
`org.amphibiaaudio.Amphibia.TONE3000/oauth-v1`. The deletion routine refuses an
unexpected root and does not traverse reparse points. It never deletes referenced
external content.

Silent uninstall preserves all user data and credentials because no explicit
interactive confirmation can be obtained.

Windows may run an all-users uninstall under a different administrator account.
In that credentialed-elevation case, per-user cleanup applies to the account that
runs the uninstaller, not necessarily the Amphibia user. Disconnect TONE3000 in
Amphibia and remove that user's `%LOCALAPPDATA%\Amphibia` manually if complete
cleanup is required. Do not delete an external NAM/IR/ZIP location merely because
it was configured in Amphibia.
