[CmdletBinding()]
param(
    [string]$RepositoryRoot = (Join-Path $PSScriptRoot "..")
)

$ErrorActionPreference = "Stop"

$RepositoryRoot = [System.IO.Path]::GetFullPath($RepositoryRoot)
$LibrimeRoot = Join-Path $RepositoryRoot "librime"
$PatchPath = Join-Path $RepositoryRoot "patches\librime-wubipinyin-1c233581.patch"
$ExpectedRevision = "1c23358157934bd6e6d6981f0c0164f05393b497"

if (-not (Test-Path -LiteralPath $LibrimeRoot)) {
    throw "Missing librime submodule: $LibrimeRoot"
}
if (-not (Test-Path -LiteralPath $PatchPath)) {
    throw "Missing WubiPinyin librime patch: $PatchPath"
}

$revision = (& git -C $LibrimeRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $revision -ne $ExpectedRevision) {
    throw "librime must be pinned to $ExpectedRevision before applying the WubiPinyin patch."
}

# The patch adds this file. Avoid a deliberately failing reverse apply on a
# clean checkout because PowerShell treats native stderr as a terminating error.
$HybridFilterPath = Join-Path $LibrimeRoot "src\rime\gear\hybrid_filter.cc"
if (Test-Path -LiteralPath $HybridFilterPath) {
    & git -C $LibrimeRoot apply --unidiff-zero --reverse --check --whitespace=error $PatchPath
    if ($LASTEXITCODE -eq 0) {
        Write-Host "WubiPinyin librime patch is already applied."
        exit 0
    }
    throw "librime contains an unrecognized HybridFilter change."
}

$status = & git -C $LibrimeRoot status --porcelain
if ($LASTEXITCODE -ne 0) {
    throw "Unable to inspect librime working tree."
}
if ($status) {
    throw "librime has local changes that are not the WubiPinyin patch."
}

& git -C $LibrimeRoot apply --unidiff-zero --check --whitespace=error $PatchPath
if ($LASTEXITCODE -ne 0) {
    throw "WubiPinyin librime patch does not apply cleanly."
}
& git -C $LibrimeRoot apply --unidiff-zero --whitespace=error $PatchPath
if ($LASTEXITCODE -ne 0) {
    throw "Unable to apply the WubiPinyin librime patch."
}
& git -C $LibrimeRoot diff --check
if ($LASTEXITCODE -ne 0) {
    throw "WubiPinyin librime patch introduced whitespace errors."
}
