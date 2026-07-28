# Sign Amphibia Windows artifacts

Unsigned local and CI artifacts are development artifacts, not stable public
releases. Release signing requires a project-controlled code-signing certificate
available to the Windows certificate store and a trusted RFC 3161 timestamp
service.

Set the certificate SHA-1 thumbprint without spaces:

```powershell
$env:AMPHIBIA_SIGN_CERT_SHA1 = "<40 hexadecimal characters>"
$env:AMPHIBIA_TIMESTAMP_URL = "https://your-approved-timestamp-service"
.\scripts\package-windows.ps1
```

The packaging script signs and verifies the staged standalone and VST3 binaries
before installer compilation, then signs and verifies the installer. It records
the status in the artifact/provenance manifests. It never accepts a PFX path or
password and never places key material in the repository or artifact directory.

Before publication, independently run:

```powershell
signtool verify /pa /all /v .\artifacts\release\0.1.0\Amphibia-0.1.0-Windows-Setup.exe
```

Also verify the embedded binary signatures after installation, certificate
subject/chain, timestamp, and `SHA256SUMS.txt`. Certificate issuance, protection,
rotation, and revocation are release-operator responsibilities.
