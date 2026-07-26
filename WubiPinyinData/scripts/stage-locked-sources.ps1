[CmdletBinding()]
param(
    [string]$RepositoryRoot = (Join-Path $PSScriptRoot "..\.."),
    [string]$OutputDirectory
)

$ErrorActionPreference = "Stop"

function Get-NormalizedPath([string]$Path) {
    return [System.IO.Path]::GetFullPath($Path)
}

function Assert-RelativePath([string]$Path) {
    if ([System.IO.Path]::IsPathRooted($Path) -or
        $Path -match '(^|[\\/])\.\.([\\/]|$)') {
        throw "Locked source path is not relative: $Path"
    }
}

function Get-Sha256([string]$Path) {
    $stream = [System.IO.File]::OpenRead($Path)
    $algorithm = [System.Security.Cryptography.SHA256]::Create()
    try {
        $hash = [System.BitConverter]::ToString($algorithm.ComputeHash($stream))
        return $hash.Replace("-", "").ToLowerInvariant()
    }
    finally {
        $algorithm.Dispose()
        $stream.Dispose()
    }
}

function Get-VerifiedFile([string]$Uri, [string]$Destination, [string]$ExpectedHash) {
    if (Test-Path -LiteralPath $Destination) {
        $existingHash = Get-Sha256 $Destination
        if ($existingHash -eq $ExpectedHash.ToLowerInvariant()) {
            return
        }
    }

    $temporary = "$Destination.download"
    Remove-Item -LiteralPath $temporary -Force -ErrorAction SilentlyContinue
    try {
        Invoke-WebRequest -Uri $Uri -OutFile $temporary -UseBasicParsing
        $actualHash = Get-Sha256 $temporary
        if ($actualHash -ne $ExpectedHash.ToLowerInvariant()) {
            throw "SHA-256 mismatch for $Uri. Expected $ExpectedHash, got $actualHash."
        }
        Move-Item -LiteralPath $temporary -Destination $Destination -Force
    }
    finally {
        Remove-Item -LiteralPath $temporary -Force -ErrorAction SilentlyContinue
    }
}

function Get-PlainFile([string]$Uri, [string]$Destination) {
    if (Test-Path -LiteralPath $Destination) {
        return
    }

    $temporary = "$Destination.download"
    Remove-Item -LiteralPath $temporary -Force -ErrorAction SilentlyContinue
    try {
        Invoke-WebRequest -Uri $Uri -OutFile $temporary -UseBasicParsing
        Move-Item -LiteralPath $temporary -Destination $Destination -Force
    }
    finally {
        Remove-Item -LiteralPath $temporary -Force -ErrorAction SilentlyContinue
    }
}

$RepositoryRoot = Get-NormalizedPath $RepositoryRoot
$DataRoot = Join-Path $RepositoryRoot "WubiPinyinData"
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $RepositoryRoot "output\data\WubiPinyinData"
}
$OutputDirectory = Get-NormalizedPath $OutputDirectory
$RimeOutput = Join-Path $OutputDirectory "rime"
$LicensesOutput = Join-Path $OutputDirectory "licenses"
$LockPath = Join-Path $DataRoot "sources.lock.json"

if (-not (Test-Path -LiteralPath $LockPath)) {
    throw "Missing source lock: $LockPath"
}

New-Item -ItemType Directory -Path $RimeOutput -Force | Out-Null
New-Item -ItemType Directory -Path $LicensesOutput -Force | Out-Null

Get-ChildItem -LiteralPath (Join-Path $DataRoot "rime") -Filter "*.yaml" -File |
    Copy-Item -Destination $RimeOutput -Force
Copy-Item -LiteralPath $LockPath -Destination (Join-Path $OutputDirectory "sources.lock.json") -Force
Copy-Item -LiteralPath (Join-Path $DataRoot "THIRD_PARTY_NOTICES.md") -Destination (Join-Path $OutputDirectory "THIRD_PARTY_NOTICES.md") -Force

$lock = Get-Content -LiteralPath $LockPath -Raw | ConvertFrom-Json
if ($lock.lock_format -ne 1) {
    throw "Unsupported source lock format: $($lock.lock_format)"
}

foreach ($source in $lock.sources) {
    $repository = $source.repository -replace '\.git$', ''
    $licenseDirectory = Join-Path $LicensesOutput $source.name
    New-Item -ItemType Directory -Path $licenseDirectory -Force | Out-Null

    foreach ($file in $source.files) {
        Assert-RelativePath $file.path
        $destination = Join-Path $RimeOutput $file.path
        New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
        Get-VerifiedFile "$repository/raw/$($source.commit)/$($file.path)" $destination $file.sha256
    }

    foreach ($noticeFile in @($source.license_file, $source.authors_file)) {
        Assert-RelativePath $noticeFile
        $destination = Join-Path $licenseDirectory $noticeFile
        New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
        Get-PlainFile "$repository/raw/$($source.commit)/$noticeFile" $destination
    }
}

Write-Host "Staged verified WubiPinyin sources in $OutputDirectory"
