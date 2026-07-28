# Build on Windows

Install Visual Studio Build Tools with Desktop development with C++, a Windows
SDK, Git, and Python 3. Initialize submodules recursively and fetch the pinned
iPlug SDK/library payloads as described in [../BUILDING.md](../BUILDING.md).

From a Developer PowerShell:

```powershell
msbuild NeuralAmpModeler\Amphibia.sln /t:Amphibia-app /p:Configuration=Release /p:Platform=x64 /m /nologo
msbuild NeuralAmpModeler\Amphibia.sln /t:Amphibia-vst3 /p:Configuration=Release /p:Platform=x64 /m /nologo
```

The project default toolset remains v143. This development environment has also
validated an explicit `PlatformToolset=v145` override. Build output is not an
installer and may include files that are forbidden from release packaging.

For a verified Windows package use [PACKAGING_WINDOWS.md](PACKAGING_WINDOWS.md);
do not manually copy a build tree into an installer.
