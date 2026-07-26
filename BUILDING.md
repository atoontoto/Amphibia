# Building Amphibia

These are developer builds, not signed end-user packages.

## Common setup

Clone with recursive submodules. The pinned iPlug2 checkout omits downloaded SDK material; fetch its declared SDKs before building a VST3:

```text
git submodule update --init --recursive
cd iPlug2/Dependencies/IPlug
./download-iplug-sdks.sh
```

Run the repository check from the root:

```text
python tests/identity/validate_identity.py
```

## Windows

Install Visual Studio 2022 Build Tools with Desktop development with C++ and a Windows 10/11 SDK, then use a Developer PowerShell:

```text
msbuild NeuralAmpModeler/Amphibia.sln /t:Amphibia-app;Amphibia-vst3 /p:Configuration=Release;Platform=x64 /m /nologo
```

The projects default to the `v143` toolset. A newer installed toolset can be tested with a local `/p:PlatformToolset=...` override. The upstream VST3 post-build step may copy into the configured VST3 directory; CI overrides `VST3_64_PATH` to a staging directory.

Expected local outputs are under `NeuralAmpModeler/build-win/`, including `Amphibia.exe` and `Amphibia.vst3`.

## macOS

Install the current supported Xcode command-line tools, fetch iPlug2 SDKs/prebuilt libraries, then build from the pinned project:

```text
xcodebuild -project NeuralAmpModeler/projects/NeuralAmpModeler-macOS.xcodeproj -scheme macOS-APP -configuration Release CODE_SIGNING_ALLOWED=NO build
xcodebuild -project NeuralAmpModeler/projects/NeuralAmpModeler-macOS.xcodeproj -scheme macOS-VST3 -configuration Release CODE_SIGNING_ALLOWED=NO build
```

These commands are represented in CI but must be confirmed on a macOS runner before Milestone 1's cross-platform build checkbox can close.

## Not supported here

AAX requires separately licensed Avid SDK material. Installer generation, signing, notarization, and release publishing are not supported here. The repository TONE3000 build is intentionally unconfigured; do not invent registration values merely to make the controls clickable.

## Native provider tests

Milestone 5's provider-neutral hosted-flow suite requires no network, browser, account, or credential:

```text
msbuild tests/milestone5/Milestone5Tests.vcxproj /t:Build /p:Configuration=Debug;Platform=x64
tests/milestone5/build/x64/Debug/Milestone5Tests.exe
```

Run `tests/milestone4/Milestone4Tests.exe` as the inherited provider/transport/credential regression. Live testing is permitted only in an owner-reviewed development build using a registered development client/redirect and injected callback receiver. Never place a client secret in a desktop build.
