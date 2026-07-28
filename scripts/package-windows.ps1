[CmdletBinding()]
param(
    [switch]$SkipBuild,
    [switch]$SkipTests,
    [switch]$Development,
    [ValidateSet('Release')]
    [string]$Configuration = 'Release',
    [string]$PlatformToolset = 'v145',
    [string]$ArtifactsRoot,
    [string]$IsccPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

$script:RepositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$script:ProductRoot = Join-Path $script:RepositoryRoot 'NeuralAmpModeler'
$script:ConfigPath = Join-Path $script:ProductRoot 'config.h'
$script:InstallerPath = Join-Path $script:ProductRoot 'installer\Amphibia.iss'
$script:AppBinary = Join-Path $script:ProductRoot "build-win\app\x64\$Configuration\Amphibia.exe"
$script:Vst3Binary = Join-Path $script:ProductRoot "build-win\vst3\x64\$Configuration\Amphibia.vst3"
$script:ExpectedInnoVersion = '6.7.3'

function Invoke-Native {
    param(
        [Parameter(Mandatory)][string]$FilePath,
        [Parameter(Mandatory)][AllowEmptyCollection()][string[]]$ArgumentList,
        [string]$WorkingDirectory = $script:RepositoryRoot
    )

    Write-Host "> $FilePath $($ArgumentList -join ' ')"
    Push-Location $WorkingDirectory
    try {
        & $FilePath @ArgumentList
        if ($LASTEXITCODE -ne 0) {
            throw "Command failed with exit code $LASTEXITCODE`: $FilePath"
        }
    }
    finally {
        Pop-Location
    }
}

function Get-ProductVersion {
    $config = Get-Content -LiteralPath $script:ConfigPath -Raw
    $match = [regex]::Match($config, '(?m)^\s*#define\s+PLUG_VERSION_STR\s+"(?<version>\d+\.\d+\.\d+)"\s*$')
    if (-not $match.Success) {
        throw 'Could not read PLUG_VERSION_STR from NeuralAmpModeler/config.h.'
    }
    return $match.Groups['version'].Value
}

function Assert-RepositoryState {
    param([string]$Version)

    if ((git branch --show-current).Trim() -ne 'main') {
        throw 'Windows release packaging must run from main.'
    }

    $dirty = @(git status --porcelain=v1 --untracked-files=all)
    if ($LASTEXITCODE -ne 0) {
        throw 'Could not inspect the Git worktree.'
    }
    if ($dirty.Count -gt 0 -and -not $Development) {
        throw 'Release packaging requires a clean worktree. Use -Development only for explicitly non-release validation.'
    }
    if ($SkipTests -and -not $Development) {
        throw '-SkipTests is accepted only with -Development; verified release packages always run tests.'
    }

    $installer = Get-Content -LiteralPath $script:InstallerPath -Raw
    foreach ($required in @(
        '802D7F30-6B3A-44C3-B809-93DA217821CA',
        '#ifndef MyAppVersion',
        'Amphibia-" + MyAppVersion + "-Windows-Setup'
    )) {
        if (-not $installer.Contains($required)) {
            throw "Installer contract is missing: $required"
        }
    }

    $resource = Get-Content -LiteralPath (Join-Path $script:ProductRoot 'resources\main.rc') -Raw
    $resourceVersion = $Version.Replace('.', ',') + ',0'
    if ($resource -notmatch ('FILEVERSION\s+' + [regex]::Escape($resourceVersion)) -or
        $resource -notmatch ('PRODUCTVERSION\s+' + [regex]::Escape($resourceVersion)) -or
        $resource -notmatch ('VALUE\s+"FileVersion",\s+"' + [regex]::Escape($Version) + '"') -or
        $resource -notmatch ('VALUE\s+"ProductVersion",\s+"' + [regex]::Escape($Version) + '"')) {
        throw "Windows resource version does not agree with $Version."
    }

    foreach ($doc in @('README.md', 'CHANGELOG.md', 'docs\MILESTONES.md')) {
        $text = Get-Content -LiteralPath (Join-Path $script:RepositoryRoot $doc) -Raw
        if (-not $text.Contains($Version)) {
            throw "$doc does not contain authoritative product version $Version."
        }
    }
}

function Find-MSBuild {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
        throw 'vswhere.exe was not found. Install Visual Studio Build Tools with Desktop C++ support.'
    }
    $installation = (& $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath).Trim()
    if (-not $installation) {
        throw 'A Visual Studio installation containing MSBuild was not found.'
    }
    $candidate = Join-Path $installation 'MSBuild\Current\Bin\amd64\MSBuild.exe'
    if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
        throw "MSBuild was not found at the expected path: $candidate"
    }
    return $candidate
}

function Find-DumpBin {
    param([string]$MSBuildPath)
    $installation = [System.IO.Path]::GetFullPath((Join-Path (Split-Path $MSBuildPath -Parent) '..\..\..\..'))
    $candidate = Get-ChildItem -LiteralPath (Join-Path $installation 'VC\Tools\MSVC') -Directory |
        Sort-Object Name -Descending |
        ForEach-Object { Join-Path $_.FullName 'bin\Hostx64\x64\dumpbin.exe' } |
        Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
        Select-Object -First 1
    if (-not $candidate) {
        throw 'dumpbin.exe was not found in the selected Visual Studio installation.'
    }
    return $candidate
}

function Find-SignTool {
    $kitsRoot = (Get-ItemProperty -LiteralPath 'HKLM:\SOFTWARE\Microsoft\Windows Kits\Installed Roots' -ErrorAction SilentlyContinue).KitsRoot10
    if (-not $kitsRoot) {
        return $null
    }
    return Get-ChildItem -LiteralPath (Join-Path $kitsRoot 'bin') -Directory -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending |
        ForEach-Object { Join-Path $_.FullName 'x64\signtool.exe' } |
        Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
        Select-Object -First 1
}

function Find-InnoCompiler {
    if ($IsccPath) {
        $candidate = [System.IO.Path]::GetFullPath($IsccPath)
    }
    else {
        $commands = @(Get-Command ISCC.exe -ErrorAction SilentlyContinue)
        $candidate = if ($commands.Count -gt 0) { $commands[0].Source } else { $null }
        if (-not $candidate) {
            foreach ($path in @(
                (Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe'),
                (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'),
                (Join-Path $env:ProgramFiles 'Inno Setup 6\ISCC.exe')
            )) {
                if (Test-Path -LiteralPath $path -PathType Leaf) {
                    $candidate = $path
                    break
                }
            }
        }
    }
    if (-not $candidate -or -not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
        throw "Inno Setup $script:ExpectedInnoVersion ISCC.exe was not found. See docs/PACKAGING_WINDOWS.md."
    }
    $changeLog = Join-Path (Split-Path $candidate -Parent) 'whatsnew.htm'
    $actual = if (Test-Path -LiteralPath $changeLog -PathType Leaf) {
        $match = [regex]::Match(
            (Get-Content -LiteralPath $changeLog -Raw),
            '<span class="ver">(?<version>\d+\.\d+\.\d+)\s*</span>')
        if ($match.Success) { $match.Groups['version'].Value } else { 'unknown' }
    }
    else {
        'unknown'
    }
    if ($actual -ne $script:ExpectedInnoVersion) {
        throw "Inno Setup $script:ExpectedInnoVersion is required; found '$actual'."
    }
    return $candidate
}

function Get-InnoVersion {
    param([string]$CompilerPath)
    $changeLog = Join-Path (Split-Path $CompilerPath -Parent) 'whatsnew.htm'
    $match = [regex]::Match(
        (Get-Content -LiteralPath $changeLog -Raw),
        '<span class="ver">(?<version>\d+\.\d+\.\d+)\s*</span>')
    if (-not $match.Success) {
        throw 'Could not record the validated Inno Setup version.'
    }
    return $match.Groups['version'].Value
}

function Invoke-ProductBuild {
    param([string]$MSBuildPath)
    if ($SkipBuild) {
        Write-Host 'Skipping compilation by request; supplied binaries will still be fully validated.'
        return
    }

    $common = @(
        '/p:Configuration=Release',
        '/p:Platform=x64',
        "/p:PlatformToolset=$PlatformToolset",
        '/p:PostBuildEventUseInBuild=false',
        '/m:1',
        '/nologo'
    )
    Invoke-Native $MSBuildPath (@('NeuralAmpModeler\Amphibia.sln', '/t:Amphibia-app:Rebuild') + $common)
    Invoke-Native $MSBuildPath (@('NeuralAmpModeler\Amphibia.sln', '/t:Amphibia-vst3:Rebuild') + $common)
}

function Invoke-RequiredTests {
    param([string]$MSBuildPath)
    if ($SkipTests) {
        Write-Warning 'Tests were skipped in explicit development mode. Output will be marked UNVERIFIED.'
        return
    }

    foreach ($scriptPath in @(
        'tests\identity\validate_identity.py',
        'tests\characterization\test_inherited_contract.py',
        'tests\characterization\test_state_contract.py',
        'tests\characterization\test_milestone6_contract.py',
        'tests\milestone7\test_installer.py',
        'tests\milestone7\test_packaging.py'
    )) {
        Invoke-Native 'python' @($scriptPath)
    }

    foreach ($milestone in 2..6) {
        $project = "tests\milestone$milestone\Milestone${milestone}Tests.vcxproj"
        $output = "tests\milestone$milestone\build\x64\Release\Milestone${milestone}Tests.exe"
        Invoke-Native $MSBuildPath @(
            $project,
            '/t:Build',
            '/p:Configuration=Release',
            '/p:Platform=x64',
            "/p:PlatformToolset=$PlatformToolset",
            '/m:1',
            '/nologo'
        )
        Invoke-Native (Join-Path $script:RepositoryRoot $output) @()
    }
}

function Assert-Binary {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string]$Version,
        [Parameter(Mandatory)][string]$DumpBinPath
    )
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Expected Release binary is missing: $Path"
    }
    if ($Path -match '(?i)\\Debug\\|\.pdb$') {
        throw "Debug or PDB input is forbidden: $Path"
    }
    $metadata = (Get-Item -LiteralPath $Path).VersionInfo
    if ($metadata.ProductName -ne 'Amphibia' -or $metadata.FileDescription -ne 'Amphibia' -or
        -not $metadata.FileVersion.StartsWith($Version, [StringComparison]::Ordinal) -or
        -not $metadata.ProductVersion.StartsWith($Version, [StringComparison]::Ordinal)) {
        throw "Binary metadata/version validation failed: $Path"
    }

    $headers = (& $DumpBinPath /headers $Path 2>&1 | Out-String)
    if ($LASTEXITCODE -ne 0 -or $headers -notmatch '(?im)\b8664 machine \(x64\)') {
        throw "Binary is not a verified Windows x64 PE: $Path"
    }
    $dependents = (& $DumpBinPath /dependents $Path 2>&1 | Out-String)
    if ($LASTEXITCODE -ne 0) {
        throw "Dependency inspection failed: $Path"
    }
    if ($dependents -match '(?im)\b(msvcp\d+|vcruntime\d+|ucrtbased|vcruntime\d+d)\.dll\b') {
        throw "Unexpected dynamically linked MSVC runtime in statically linked Release binary: $Path"
    }
    return $dependents
}

function Initialize-ReleaseDirectories {
    param([string]$Version)

    $base = if ($ArtifactsRoot) {
        [System.IO.Path]::GetFullPath($ArtifactsRoot)
    }
    else {
        Join-Path $script:RepositoryRoot 'artifacts\release'
    }
    $releaseBase = [System.IO.Path]::GetFullPath((Join-Path $script:RepositoryRoot 'artifacts\release'))
    if (-not $Development -and -not $base.StartsWith($releaseBase, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Release output must remain below $releaseBase"
    }
    $versionRoot = [System.IO.Path]::GetFullPath((Join-Path $base $Version))
    if ($versionRoot -eq $base -or -not $versionRoot.StartsWith(($base.TrimEnd('\') + '\'), [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Refusing to clean an unsafe release staging path.'
    }
    if (Test-Path -LiteralPath $versionRoot) {
        Remove-Item -LiteralPath $versionRoot -Recurse -Force
    }
    $stageRoot = Join-Path $versionRoot '_staging'
    $packageRoot = Join-Path $versionRoot '_packages'
    foreach ($directory in @($versionRoot, $stageRoot, $packageRoot)) {
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
    }
    return @{
        VersionRoot = $versionRoot
        StageRoot = $stageRoot
        PackageRoot = $packageRoot
    }
}

function Copy-ReleaseInputs {
    param(
        [hashtable]$Directories,
        [string]$Version
    )
    $stage = $Directories.StageRoot
    $standalone = Join-Path $stage 'Standalone'
    $vst3 = Join-Path $stage 'VST3\Amphibia.vst3'
    $vst3BinaryDir = Join-Path $vst3 'Contents\x86_64-win'
    $vst3Resources = Join-Path $vst3 'Contents\Resources'
    $documentation = Join-Path $stage 'Documentation'
    foreach ($directory in @($standalone, $vst3BinaryDir, $vst3Resources, $documentation)) {
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
    }

    Copy-Item -LiteralPath $script:AppBinary -Destination (Join-Path $standalone 'Amphibia.exe')
    Copy-Item -LiteralPath $script:Vst3Binary -Destination (Join-Path $vst3BinaryDir 'Amphibia.vst3')

    $rootCopies = @{
        'LICENSE' = 'LICENSE.txt'
        'THIRD_PARTY_NOTICES.md' = 'THIRD_PARTY_NOTICES.txt'
        'ATTRIBUTION.md' = 'ATTRIBUTION.txt'
        'PRIVACY.md' = 'PRIVACY.txt'
        'README.md' = 'README.txt'
        'CHANGELOG.md' = 'CHANGELOG.txt'
    }
    foreach ($entry in $rootCopies.GetEnumerator()) {
        Copy-Item -LiteralPath (Join-Path $script:RepositoryRoot $entry.Key) -Destination (Join-Path $stage $entry.Value)
    }
    Copy-Item -LiteralPath (Join-Path $stage 'THIRD_PARTY_NOTICES.txt') -Destination (Join-Path $vst3Resources 'THIRD_PARTY_NOTICES.txt')

    foreach ($doc in @(
        'FIRST_RUN_SETUP.md',
        'AUDIO_CONFIGURATION.md',
        'INSTALL_WINDOWS.md',
        'UPGRADE_WINDOWS.md',
        'REPAIR_WINDOWS.md',
        'UNINSTALL_WINDOWS.md',
        'LOCAL_LIBRARY.md',
        'ZIP_IMPORT.md',
        'TONE3000_INTEGRATION.md',
        'TROUBLESHOOTING.md'
    )) {
        $source = Join-Path $script:RepositoryRoot "docs\$doc"
        if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
            throw "Required installed documentation is missing: $doc"
        }
        Copy-Item -LiteralPath $source -Destination (Join-Path $documentation $doc)
    }

    @(
        "Portable development package - manual installation"
        ''
        "Amphibia $Version for Windows x64."
        'This archive does not install files, create shortcuts, configure a DAW, or rescan plug-ins.'
        'The VST3 bundle must be copied manually to a standard VST3 location.'
        'Unsigned status and source/dependency provenance are recorded beside this file.'
    ) | Set-Content -LiteralPath (Join-Path $stage 'PORTABLE-README.txt') -Encoding UTF8

    if ($SkipTests) {
        'UNVERIFIED DEVELOPMENT OUTPUT - REQUIRED TESTS WERE SKIPPED.' |
            Set-Content -LiteralPath (Join-Path $stage 'UNVERIFIED-DEVELOPMENT.txt') -Encoding ASCII
    }
}

function Invoke-OptionalSigning {
    param(
        [string[]]$Paths,
        [string]$SignToolPath
    )
    $certificate = $env:AMPHIBIA_SIGN_CERT_SHA1
    if (-not $certificate) {
        Write-Warning 'No Authenticode certificate was configured. Artifacts remain unsigned.'
        return 'Unsigned'
    }
    if (-not $SignToolPath) {
        throw 'Signing was requested but signtool.exe was not found.'
    }
    if ($certificate -notmatch '^[0-9A-Fa-f]{40}$') {
        throw 'AMPHIBIA_SIGN_CERT_SHA1 must contain exactly 40 hexadecimal characters.'
    }
    $timestamp = if ($env:AMPHIBIA_TIMESTAMP_URL) { $env:AMPHIBIA_TIMESTAMP_URL } else { 'http://timestamp.digicert.com' }
    if ($timestamp -notmatch '^https?://') {
        throw 'AMPHIBIA_TIMESTAMP_URL must be an HTTP(S) timestamp endpoint.'
    }
    foreach ($path in $Paths) {
        Invoke-Native $SignToolPath @('sign', '/sha1', $certificate, '/fd', 'SHA256', '/tr', $timestamp, '/td', 'SHA256', $path)
        Invoke-Native $SignToolPath @('verify', '/pa', '/all', $path)
    }
    return 'Authenticode signed and verified'
}

function Write-ProvenanceManifests {
    param(
        [hashtable]$Directories,
        [string]$Version,
        [string]$MSBuildPath,
        [string]$IsccCompiler,
        [string]$SigningStatus,
        [string]$AppDependencies,
        [string]$Vst3Dependencies
    )
    $root = $Directories.VersionRoot
    $commit = (git rev-parse HEAD).Trim()
    $branch = (git branch --show-current).Trim()
    $dirty = if (@(git status --porcelain=v1 --untracked-files=all).Count -eq 0) { 'clean' } else { 'dirty' }
    $upstream = (git remote get-url upstream).Trim()
    $submodules = @{}
    foreach ($line in @(git submodule status --recursive)) {
        if ($line -match '^[ +-]?(?<hash>[0-9a-f]{40})\s+(?<path>\S+)') {
            $submodules[$matches.path] = $matches.hash
        }
    }
    $sdkRoot = (Get-ItemProperty -LiteralPath 'HKLM:\SOFTWARE\Microsoft\Windows Kits\Installed Roots' -ErrorAction SilentlyContinue).KitsRoot10
    $sdkVersion = if ($sdkRoot) {
        (Get-ChildItem -LiteralPath (Join-Path $sdkRoot 'Include') -Directory | Sort-Object Name -Descending | Select-Object -First 1).Name
    } else { 'unknown' }
    $msbuildVersion = (Get-Item -LiteralPath $MSBuildPath).VersionInfo.ProductVersion
    $innoVersion = Get-InnoVersion $IsccCompiler

    @(
        "Amphibia Git commit: $commit"
        "Branch: $branch"
        "Worktree status: $dirty"
        "Upstream remote: $upstream"
        'Upstream base commit: 96337e9ab6e3beb619459779bbb5c47e1b04d8c4'
        "NeuralAmpModelerCore commit: $($submodules['NeuralAmpModelerCore'])"
        "iPlug2 commit: $($submodules['iPlug2'])"
        "AudioDSPTools commit: $($submodules['AudioDSPTools'])"
        "Eigen commit: $($submodules['eigen'])"
        "Build date UTC: $([DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ'))"
        "Toolchain: MSBuild $msbuildVersion / $PlatformToolset"
        "Windows SDK: $sdkVersion"
        "Installer: Inno Setup $innoVersion"
        "Signing: $SigningStatus"
    ) | Set-Content -LiteralPath (Join-Path $root 'SOURCE-COMMIT.txt') -Encoding UTF8

    @(
        'Amphibia dependency versions'
        ''
        'NeuralAmpModelerPlugin upstream base: 0.7.15 / 96337e9ab6e3beb619459779bbb5c47e1b04d8c4 (MIT)'
        "NeuralAmpModelerCore: $($submodules['NeuralAmpModelerCore']) (MIT)"
        "iPlug2 fork: $($submodules['iPlug2']) (zlib-style)"
        "AudioDSPTools: $($submodules['AudioDSPTools']) (MIT)"
        "Eigen root: $($submodules['eigen']) (MPL-2.0 and component licenses)"
        'minizip-ng: 4.2.2 reader subset (zlib license)'
        'zlib: 1.3.2 inflate/checksum subset (zlib license)'
        'nlohmann/json: pinned iPlug2 dependency source (MIT)'
        "Windows SDK: $sdkVersion (Microsoft SDK/runtime terms)"
        "MSVC/MSBuild: $msbuildVersion / $PlatformToolset (Microsoft toolchain/runtime terms)"
        "Inno Setup: $innoVersion (Inno Setup LICENSE.TXT; compiler not redistributed)"
        'Windows APIs: Win32, DirectSound, winmm, WinHTTP/Schannel, CNG, Credential Manager, Winsock'
        ''
        'Release binaries use the static MSVC runtime (/MT); no Visual C++ Redistributable is bundled.'
        'See THIRD_PARTY_NOTICES.txt and docs/DEPENDENCIES.md for notices and remaining review items.'
        ''
        'Standalone PE dependents:'
        (($AppDependencies -split "`r?`n" | Where-Object { $_ -match '(?i)\.dll\s*$' } | ForEach-Object { $_.Trim() }) -join ', ')
        'VST3 PE dependents:'
        (($Vst3Dependencies -split "`r?`n" | Where-Object { $_ -match '(?i)\.dll\s*$' } | ForEach-Object { $_.Trim() }) -join ', ')
    ) | Set-Content -LiteralPath (Join-Path $root 'DEPENDENCY-VERSIONS.txt') -Encoding UTF8

    Copy-Item -LiteralPath (Join-Path $script:RepositoryRoot 'THIRD_PARTY_NOTICES.md') -Destination (Join-Path $root 'THIRD_PARTY_NOTICES.txt')
    foreach ($name in @('SOURCE-COMMIT.txt', 'DEPENDENCY-VERSIONS.txt')) {
        Copy-Item -LiteralPath (Join-Path $root $name) -Destination (Join-Path $Directories.StageRoot $name)
    }
}

function New-ZipFromDirectory {
    param(
        [string]$SourceDirectory,
        [string]$Destination
    )
    if (Test-Path -LiteralPath $Destination) {
        Remove-Item -LiteralPath $Destination -Force
    }
    Compress-Archive -Path (Join-Path $SourceDirectory '*') -DestinationPath $Destination -CompressionLevel Optimal
}

function New-ReleaseArchives {
    param(
        [hashtable]$Directories,
        [string]$Version
    )
    $packages = $Directories.PackageRoot
    $root = $Directories.VersionRoot
    $stage = $Directories.StageRoot

    $standalonePackage = Join-Path $packages "Amphibia-$Version-Windows-Standalone"
    New-Item -ItemType Directory -Path $standalonePackage | Out-Null
    Copy-Item -LiteralPath (Join-Path $stage 'Standalone\Amphibia.exe') -Destination $standalonePackage
    foreach ($name in @('LICENSE.txt', 'THIRD_PARTY_NOTICES.txt', 'ATTRIBUTION.txt', 'PRIVACY.txt', 'README.txt', 'CHANGELOG.txt', 'SOURCE-COMMIT.txt', 'DEPENDENCY-VERSIONS.txt')) {
        Copy-Item -LiteralPath (Join-Path $stage $name) -Destination $standalonePackage
    }
    Copy-Item -LiteralPath (Join-Path $stage 'Documentation') -Destination $standalonePackage -Recurse
    New-ZipFromDirectory $standalonePackage (Join-Path $root "Amphibia-$Version-Windows-Standalone.zip")

    $vst3Package = Join-Path $packages "Amphibia-$Version-Windows-VST3"
    New-Item -ItemType Directory -Path $vst3Package | Out-Null
    Copy-Item -LiteralPath (Join-Path $stage 'VST3\Amphibia.vst3') -Destination $vst3Package -Recurse
    Copy-Item -LiteralPath (Join-Path $stage 'LICENSE.txt') -Destination $vst3Package
    Copy-Item -LiteralPath (Join-Path $stage 'ATTRIBUTION.txt') -Destination $vst3Package
    Copy-Item -LiteralPath (Join-Path $stage 'SOURCE-COMMIT.txt') -Destination $vst3Package
    Copy-Item -LiteralPath (Join-Path $stage 'DEPENDENCY-VERSIONS.txt') -Destination $vst3Package
    New-ZipFromDirectory $vst3Package (Join-Path $root "Amphibia-$Version-Windows-VST3.zip")

    $portablePackage = Join-Path $packages "Amphibia-$Version-Windows-Portable"
    New-Item -ItemType Directory -Path $portablePackage | Out-Null
    Copy-Item -LiteralPath (Join-Path $stage 'Standalone') -Destination $portablePackage -Recurse
    Copy-Item -LiteralPath (Join-Path $stage 'VST3') -Destination $portablePackage -Recurse
    Copy-Item -LiteralPath (Join-Path $stage 'Documentation') -Destination $portablePackage -Recurse
    foreach ($name in @('LICENSE.txt', 'THIRD_PARTY_NOTICES.txt', 'ATTRIBUTION.txt', 'PRIVACY.txt', 'README.txt', 'CHANGELOG.txt', 'SOURCE-COMMIT.txt', 'DEPENDENCY-VERSIONS.txt', 'PORTABLE-README.txt')) {
        Copy-Item -LiteralPath (Join-Path $stage $name) -Destination $portablePackage
    }
    if ($SkipTests) {
        Copy-Item -LiteralPath (Join-Path $stage 'UNVERIFIED-DEVELOPMENT.txt') -Destination $portablePackage
    }
    New-ZipFromDirectory $portablePackage (Join-Path $root "Amphibia-$Version-Windows-Portable.zip")
}

function Assert-ArchiveContents {
    param(
        [string]$VersionRoot,
        [string]$Version
    )
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    foreach ($archive in Get-ChildItem -LiteralPath $VersionRoot -Filter '*.zip') {
        $zip = [System.IO.Compression.ZipFile]::OpenRead($archive.FullName)
        try {
            $entries = @($zip.Entries | ForEach-Object { $_.FullName.Replace('\', '/') })
            if ($entries.Count -eq 0) {
                throw "Archive is empty: $($archive.Name)"
            }
            foreach ($entry in $entries) {
                if ($entry -match '(?i)(^|/)(Debug|tests?|Models?|IRs?|Library|Cache|Logs?)(/|$)' -or
                    $entry -match '(?i)\.(pdb|ilk|lib|exp|nam|wav|zip|pfx|p12|key|pem|log)$' -or
                    $entry -match '(?i)(settings-v1\.json|settings\.ini|credential|token|signed-url)') {
                    throw "Forbidden release archive entry '$entry' in $($archive.Name)."
                }
                if ($entry -match '^[A-Za-z]:/' -or $entry.StartsWith('/')) {
                    throw "Absolute archive entry '$entry' in $($archive.Name)."
                }
            }
            if ($archive.Name -like '*VST3*' -and
                -not ($entries -contains 'Amphibia.vst3/Contents/x86_64-win/Amphibia.vst3')) {
                throw "VST3 archive does not contain the required bundle binary: $($archive.Name)"
            }
        }
        finally {
            $zip.Dispose()
        }
    }
}

function Write-ArtifactInventoryAndChecksums {
    param(
        [string]$VersionRoot,
        [string]$Version,
        [string]$SigningStatus
    )
    $artifactNames = @(
        "Amphibia-$Version-Windows-Setup.exe",
        "Amphibia-$Version-Windows-Portable.zip",
        "Amphibia-$Version-Windows-VST3.zip",
        "Amphibia-$Version-Windows-Standalone.zip",
        'SOURCE-COMMIT.txt',
        'DEPENDENCY-VERSIONS.txt',
        'THIRD_PARTY_NOTICES.txt'
    )
    foreach ($name in $artifactNames) {
        if (-not (Test-Path -LiteralPath (Join-Path $VersionRoot $name) -PathType Leaf)) {
            throw "Expected final artifact is missing: $name"
        }
    }

    @(
        "Amphibia $Version Windows x64 artifact inventory"
        "Verification: $(if ($SkipTests) { 'UNVERIFIED DEVELOPMENT' } else { 'required automated tests passed' })"
        "Signing: $SigningStatus"
        'Publication: workflow/development artifact only; not a stable public release'
        ''
        $artifactNames
    ) | Set-Content -LiteralPath (Join-Path $VersionRoot 'ARTIFACT-MANIFEST.txt') -Encoding UTF8
    $artifactNames += 'ARTIFACT-MANIFEST.txt'

    $sumLines = foreach ($name in $artifactNames) {
        $hash = (Get-FileHash -LiteralPath (Join-Path $VersionRoot $name) -Algorithm SHA256).Hash.ToLowerInvariant()
        "$hash  $name"
    }
    $sumPath = Join-Path $VersionRoot 'SHA256SUMS.txt'
    $sumLines | Set-Content -LiteralPath $sumPath -Encoding ASCII

    foreach ($line in Get-Content -LiteralPath $sumPath) {
        if ($line -notmatch '^(?<hash>[0-9a-f]{64})  (?<name>.+)$') {
            throw "Invalid checksum line: $line"
        }
        $actual = (Get-FileHash -LiteralPath (Join-Path $VersionRoot $matches.name) -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($actual -ne $matches.hash) {
            throw "Checksum verification failed: $($matches.name)"
        }
    }
}

$version = Get-ProductVersion
Assert-RepositoryState $version
$msbuild = Find-MSBuild
$dumpbin = Find-DumpBin $msbuild
$iscc = Find-InnoCompiler

Invoke-ProductBuild $msbuild
Invoke-RequiredTests $msbuild

$appDependencies = Assert-Binary $script:AppBinary $version $dumpbin
$vst3Dependencies = Assert-Binary $script:Vst3Binary $version $dumpbin

$directories = Initialize-ReleaseDirectories $version
Copy-ReleaseInputs $directories $version

$signTool = Find-SignTool
$stagedBinaries = @(
    (Join-Path $directories.StageRoot 'Standalone\Amphibia.exe'),
    (Join-Path $directories.StageRoot 'VST3\Amphibia.vst3\Contents\x86_64-win\Amphibia.vst3')
)
$signingStatus = Invoke-OptionalSigning $stagedBinaries $signTool
Write-ProvenanceManifests $directories $version $msbuild $iscc $signingStatus $appDependencies $vst3Dependencies

Invoke-Native $iscc @(
    "/DMyAppVersion=$version",
    "/DStageRoot=$($directories.StageRoot)",
    "/DReleaseOutputDir=$($directories.VersionRoot)",
    $script:InstallerPath
)

$installer = Join-Path $directories.VersionRoot "Amphibia-$version-Windows-Setup.exe"
if (-not (Test-Path -LiteralPath $installer -PathType Leaf)) {
    throw 'Inno Setup completed without producing the expected installer artifact.'
}
if ($env:AMPHIBIA_SIGN_CERT_SHA1) {
    $signingStatus = Invoke-OptionalSigning @($installer) $signTool
}

New-ReleaseArchives $directories $version
Assert-ArchiveContents $directories.VersionRoot $version
Write-ArtifactInventoryAndChecksums $directories.VersionRoot $version $signingStatus
Invoke-Native 'python' @('tests\milestone7\test_packaging.py', '--artifacts', $directories.VersionRoot)

Write-Host ''
Write-Host "Amphibia $version Windows packaging completed."
Write-Host "Signing: $signingStatus"
Write-Host "Artifacts: $($directories.VersionRoot)"
Get-ChildItem -LiteralPath $directories.VersionRoot -File |
    Sort-Object Name |
    ForEach-Object { Write-Host "  $($_.FullName)" }
