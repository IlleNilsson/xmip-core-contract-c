<#
    .SYNOPSIS
    Builds the C contract module as a shared library and runs its probe.

    .DESCRIPTION
    The gate xgit runs (Test-XmipSelfVerifyingModule): exit 0 when the module
    builds and the probe passes, non-zero otherwise. The probe and the build
    are the capability's, shared by every language technology (ADR-0044):
    probe/verify.ps1 beside this repository's mount in the estate, or where
    XMIP_CONTRACT_PROBE points.
#>
[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

[string] $probe = $env:XMIP_CONTRACT_PROBE
if ([string]::IsNullOrWhiteSpace($probe)) {
    $probe = Join-Path $PSScriptRoot '..' 'probe'
}
[string] $verify = Join-Path $probe 'verify.ps1'
if (-not (Test-Path -LiteralPath $verify)) {
    Write-Host "FAILED. The capability's probe is not at $probe; set XMIP_CONTRACT_PROBE."
    exit 2
}

& $verify -Directory $PSScriptRoot -Compiler cc -Source src/contract.c -Standard c
exit $LASTEXITCODE
