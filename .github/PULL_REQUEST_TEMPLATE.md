## Outcome

Describe the user-visible or engineering result and link related issues.

## Validation

List exact commands and results. Identify every tested OS, architecture, app/plug-in format, host, sample rate, and block size that matters.

## Risk review

- [ ] Identity/state compatibility is unchanged, or the migration and tests are explained.
- [ ] No audio-thread allocation, file/network I/O, blocking, preparation, or destruction was introduced.
- [ ] New dependencies/assets include exact source, version/hash, license, and notice updates.
- [ ] No credentials, private models/IRs, generated packages, or proprietary SDK files are included.
- [ ] Privacy, security, provider terms, and attribution documents are updated where applicable.
- [ ] `python tests/identity/validate_identity.py` passes.
- [ ] The applicable native build was run, or the reason it could not run is stated.

## Upstream impact

Note retained technical filenames, submodule changes, likely merge conflicts, and whether any change is suitable to propose upstream.
