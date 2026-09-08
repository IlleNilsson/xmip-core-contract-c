<#
    .SYNOPSIS
    Builds the C contract module as a shared library and runs its probe.

    .DESCRIPTION
    The gate xgit runs (Test-XmipSelfVerifyingModule): exit 0 when the module
    builds and the probe passes, non-zero otherwise. One compiler everywhere,
    zig cc, declared in prerequisite.toml as `c`. The ABI header comes from
    xmip-core-abi, found in the estate when this repository is mounted there
    and through XMIP_ABI_INCLUDE otherwise.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot

if (-not (Get-Command zig -ErrorAction SilentlyContinue)) {
    Write-Host 'FAILED. zig is not installed; prerequisite.toml declares it as c.'
    exit 2
}

[string] $include = $env:XMIP_ABI_INCLUDE
if (-not $include) {
    $include = Join-Path $PSScriptRoot '..' '..' '..' 'foundation' 'abi' 'include'
}
if (-not (Test-Path -LiteralPath (Join-Path $include 'xmip_module.h'))) {
    Write-Host "FAILED. xmip_module.h not found under $include; set XMIP_ABI_INCLUDE."
    exit 2
}

New-Item -ItemType Directory -Force -Path build | Out-Null
[string] $library = if ($IsWindows) { 'xmip_core_contract_c.dll' }
    elseif ($IsMacOS) { 'libxmip_core_contract_c.dylib' } else { 'libxmip_core_contract_c.so' }
[string] $probe = if ($IsWindows) { 'probe.exe' } else { 'probe' }

Write-Host "   zig cc -shared -> build/$library"
& zig cc -shared -O2 -fvisibility=hidden -Wall -Wextra -Werror -I $include `
    src/contract.c -o (Join-Path build $library)
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "   zig cc -> build/$probe"
& zig cc -O1 -Wall -Wextra -Werror -I $include src/contract.c tests/probe.c -o (Join-Path build $probe)
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& (Join-Path $PSScriptRoot 'build' $probe)
exit $LASTEXITCODE
