#Requires -RunAsAdministrator
<#
.SYNOPSIS
    Prepares a self-hosted Windows GitHub Actions runner for build_windows_bridge.yml.

.DESCRIPTION
    Installs everything the "Build Windows" job expects that GitHub's windows-latest
    image ships by default: Chocolatey, Git, 7-Zip, PowerShell 7, Strawberry Perl,
    NSIS, VS 2022 Build Tools (C++ + Windows SDK 10.0.26100) and WSL2. Also enables
    long paths, excludes the runner work dir from Defender and, if -RunnerUser is
    given, moves the runner service to that account (WSL does not work under
    NETWORK SERVICE).

    Safe to re-run. If enabling WSL needs a reboot, the script says so; reboot and
    run it again to finish.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File .\setup_windows_runner.ps1 -RunnerUser ciuser
#>
param(
    # Local account the runner service should run as. Created (as admin) if missing.
    [string]$RunnerUser,
    [SecureString]$RunnerPassword
)

$ErrorActionPreference = 'Stop'
$WinSdkVersion = '10.0.26100.0'
$rebootNeeded = $false

function Step([string]$msg) { Write-Host "`n==> $msg" -ForegroundColor Cyan }

function Assert-ExitCode([string]$what) {
    # 1641/3010 = success, reboot required
    if ($LASTEXITCODE -notin 0, 1641, 3010) { throw "$what failed with exit code $LASTEXITCODE" }
    if ($LASTEXITCODE -in 1641, 3010) { $script:rebootNeeded = $true }
}

function Update-Path {
    Import-Module "$env:ChocolateyInstall\helpers\chocolateyProfile.psm1" -Force
    Update-SessionEnvironment
}

function Grant-ServiceLogonRight([string]$account) {
    $sid = (New-Object System.Security.Principal.NTAccount($account)).Translate(
        [System.Security.Principal.SecurityIdentifier]).Value
    $cfg = Join-Path $env:TEMP "secpol-$([guid]::NewGuid()).inf"
    $db = [IO.Path]::ChangeExtension($cfg, 'sdb')
    secedit /export /cfg $cfg /areas USER_RIGHTS | Out-Null
    $lines = [Collections.Generic.List[string]](Get-Content $cfg)
    $i = $lines.FindIndex({ param($l) $l -like 'SeServiceLogonRight*' })
    if ($i -ge 0) {
        if ($lines[$i] -like "*$sid*") { Remove-Item $cfg; return }
        $lines[$i] = "$($lines[$i]),*$sid"
    } else {
        $p = $lines.FindIndex({ param($l) $l -eq '[Privilege Rights]' })
        $lines.Insert($p + 1, "SeServiceLogonRight = *$sid")
    }
    Set-Content $cfg $lines -Encoding Unicode
    secedit /configure /db $db /cfg $cfg /areas USER_RIGHTS | Out-Null
    Remove-Item $cfg, $db -ErrorAction SilentlyContinue
}

# --- Chocolatey ----------------------------------------------------------------
Step 'Chocolatey'
if (-not (Get-Command choco -ErrorAction SilentlyContinue)) {
    Set-ExecutionPolicy Bypass -Scope Process -Force
    [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor 3072
    Invoke-Expression ((New-Object Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))
    $env:ChocolateyInstall = "$env:ProgramData\chocolatey"
}
Update-Path

# --- Tools ---------------------------------------------------------------------
Step 'Git, 7-Zip, PowerShell 7, Strawberry Perl, NSIS'
choco install -y --no-progress git 7zip powershell-core strawberryperl nsis
Assert-ExitCode 'choco install tools'
Update-Path

# --- Visual Studio Build Tools -------------------------------------------------
Step "Visual Studio Build Tools (C++ workload, Windows SDK $WinSdkVersion)"
$vsComponents = @(
    'Microsoft.VisualStudio.Workload.VCTools',
    'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
    'Microsoft.VisualStudio.Component.Windows11SDK.26100',
    'Microsoft.VisualStudio.Component.VC.ATL'
)
$vsAddArgs = ($vsComponents | ForEach-Object { "--add $_" }) -join ' '
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$sdkDir = "${env:ProgramFiles(x86)}\Windows Kits\10\Include\$WinSdkVersion"
$vsPath = $null
if (Test-Path $vswhere) {
    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
}
if (-not $vsPath) {
    choco install -y --no-progress visualstudio2022buildtools --package-parameters "$vsAddArgs --includeRecommended --passive --norestart"
    Assert-ExitCode 'choco install visualstudio2022buildtools'
} elseif (-not (Test-Path $sdkDir)) {
    # VS with C++ already present, only the pinned SDK (and maybe ATL) is missing
    $setup = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\setup.exe"
    $p = Start-Process $setup -ArgumentList "modify --installPath `"$vsPath`" $vsAddArgs --quiet --norestart" -Wait -PassThru
    $global:LASTEXITCODE = $p.ExitCode
    Assert-ExitCode 'Visual Studio modify'
} else {
    Write-Host "Already installed at $vsPath"
}

# --- WSL2 ----------------------------------------------------------------------
Step 'WSL2'
foreach ($feature in 'Microsoft-Windows-Subsystem-Linux', 'VirtualMachinePlatform') {
    if ((Get-WindowsOptionalFeature -Online -FeatureName $feature).State -ne 'Enabled') {
        $r = Enable-WindowsOptionalFeature -Online -FeatureName $feature -All -NoRestart
        if ($r.RestartNeeded) { $rebootNeeded = $true }
    }
}
if ($rebootNeeded) {
    Write-Host 'WSL features enabled; WSL update is deferred until after reboot.' -ForegroundColor Yellow
} else {
    wsl.exe --update
    Assert-ExitCode 'wsl --update'
    wsl.exe --set-default-version 2 | Out-Null
}

# --- Long paths ----------------------------------------------------------------
Step 'Long path support'
New-ItemProperty HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem -Name LongPathsEnabled -Value 1 -PropertyType DWORD -Force | Out-Null
git config --system core.longpaths true

# --- Runner service ------------------------------------------------------------
Step 'GitHub Actions runner service'
$services = @(Get-CimInstance Win32_Service -Filter "Name LIKE 'actions.runner.%'")
if (-not $services) {
    Write-Host 'No actions.runner.* service found; skipping service setup.' -ForegroundColor Yellow
}
foreach ($svc in $services) {
    # PathName is "<runner root>\bin\RunnerService.exe"
    $exe = $svc.PathName.Trim('"')
    $workDir = Join-Path (Split-Path (Split-Path $exe)) '_work'
    try {
        Add-MpPreference -ExclusionPath $workDir
        Write-Host "Defender exclusion added: $workDir"
    } catch {
        Write-Host "Could not add Defender exclusion ($($_.Exception.Message))" -ForegroundColor Yellow
    }

    if ($RunnerUser) {
        $user = $RunnerUser -replace '^\.\\', ''
        $account = "$env:COMPUTERNAME\$user"
        if (-not $RunnerPassword) { $RunnerPassword = Read-Host "Password for $account" -AsSecureString }
        $plain = [Runtime.InteropServices.Marshal]::PtrToStringAuto(
            [Runtime.InteropServices.Marshal]::SecureStringToBSTR($RunnerPassword))

        if (-not (Get-LocalUser -Name $user -ErrorAction SilentlyContinue)) {
            New-LocalUser -Name $user -Password $RunnerPassword -PasswordNeverExpires | Out-Null
            Write-Host "Created local user $account"
        }
        if (-not (Get-LocalGroupMember -SID S-1-5-32-544 -Member $account -ErrorAction SilentlyContinue)) {
            Add-LocalGroupMember -SID S-1-5-32-544 -Member $account
        }
        Grant-ServiceLogonRight $account

        $r = Invoke-CimMethod -InputObject $svc -MethodName Change -Arguments @{ StartName = $account; StartPassword = $plain }
        if ($r.ReturnValue -ne 0) { throw "Changing $($svc.Name) logon account failed (code $($r.ReturnValue))" }
        Write-Host "$($svc.Name) now runs as $account"
    } elseif ($svc.StartName -match 'NetworkService|LocalSystem|LocalService') {
        Write-Host "$($svc.Name) runs as $($svc.StartName); WSL validation will fail. Re-run with -RunnerUser <name>." -ForegroundColor Yellow
    }

    if (-not $rebootNeeded) {
        Restart-Service $svc.Name
        Write-Host "Restarted $($svc.Name) so it picks up the new PATH"
    }
}

# --- Verification --------------------------------------------------------------
Step 'Verification'
Update-Path
$checks = [ordered]@{
    'git'                     = { [bool](Get-Command git -ErrorAction SilentlyContinue) }
    'pwsh'                    = { [bool](Get-Command pwsh -ErrorAction SilentlyContinue) }
    'perl'                    = { [bool](Get-Command perl -ErrorAction SilentlyContinue) }
    'makensis'                = { [bool](Get-Command makensis -ErrorAction SilentlyContinue) }
    '7-Zip at Program Files'  = { Test-Path "$env:ProgramFiles\7-Zip\7z.exe" }
    'MSVC C++ tools'          = { [bool](& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath) }
    "Windows SDK $WinSdkVersion" = { Test-Path $sdkDir }
    'WSL'                     = { if ($rebootNeeded) { $false } else { wsl.exe --status | Out-Null; $LASTEXITCODE -eq 0 } }
    'Long paths'              = { (Get-ItemProperty HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem).LongPathsEnabled -eq 1 }
}
$failed = 0
foreach ($name in $checks.Keys) {
    $ok = try { & $checks[$name] } catch { $false }
    if ($ok) { Write-Host "  [ OK ] $name" -ForegroundColor Green }
    else     { Write-Host "  [FAIL] $name" -ForegroundColor Red; $failed++ }
}

if ($rebootNeeded) {
    Write-Host "`nReboot required. Reboot, then run this script again to finish WSL setup." -ForegroundColor Yellow
} elseif ($failed) {
    Write-Host "`n$failed check(s) failed." -ForegroundColor Red
    exit 1
} else {
    Write-Host "`nAll set." -ForegroundColor Green
    if ($RunnerUser) { Write-Host "Log in once as $RunnerUser and run 'wsl --status' so WSL initialises for that account." }
}
