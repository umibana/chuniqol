param([Parameter(Mandatory=$true)][string]$GameBin, [switch]$Escape, [switch]$Tracks, [switch]$Levels)
$ErrorActionPreference = 'Stop'
$bin = (Resolve-Path -LiteralPath $GameBin).Path
if (Get-Process chusanApp,amdaemon -ErrorAction SilentlyContinue) {
    throw 'Close the game and amdaemon before installing.'
}
$profiles = @{
    'chusanApp.exe' = '71f8fe2dc8dcf287e5eaa77add22af1137aa5aa7e721049deefd79890d3fae6e'
    'amdaemon.exe' = '014992a5af1b62b5fd42d2cb0c19a38aba4b5437a3ef82d304de2c93065ed70d'
}
foreach ($name in $profiles.Keys) {
    if ((Get-FileHash -LiteralPath (Join-Path $bin $name) -Algorithm SHA256).Hash -ne $profiles[$name]) {
        throw "Unsupported version: $name. Nothing was changed."
    }
}
$dlls = @('chuni-credits.dll', 'chuni-premium.dll')
if ($Escape) { $dlls += 'chuni-escape.dll' }
if ($Tracks) { $dlls += 'chuni-tracks.dll' }
if ($Levels) { $dlls += 'chuni-levels.dll' }
foreach ($name in $dlls) {
    if (!(Test-Path -LiteralPath (Join-Path $PSScriptRoot "build/$name"))) {
        throw "Build $name first."
    }
}
$launchPath = Join-Path $bin 'launch.bat'
$configPath = Join-Path $bin 'config_common.json'
$launch = [IO.File]::ReadAllText($launchPath)
$config = [IO.File]::ReadAllText($configPath)
if ([regex]::Matches($launch, '(?im)^.*\binject_x64\b.*\bamdaemon\.exe\b.*$').Count -ne 1 -or
    [regex]::Matches($launch, '(?im)^.*\binject_x86\b.*\bchusanApp\.exe\b.*$').Count -ne 1) {
    throw 'Unrecognized launch.bat; install manually.'
}
if ([regex]::Matches($config, '"max_credit"\s*:\s*-?\d+').Count -ne 1) {
    throw 'Ambiguous max_credit in config_common.json.'
}
$null = $config | ConvertFrom-Json
$newConfig = [regex]::Replace($config, '("max_credit"\s*:\s*)-?\d+', '${1}99')
$newLaunch = $launch
if ($newLaunch -notmatch '(?i)-k\s+chuni-credits\.dll\b') {
    $newLaunch = [regex]::Replace($newLaunch, '(?im)^(.*\binject_x64\b.*?)\bamdaemon\.exe\b', '${1}-k chuni-credits.dll amdaemon.exe')
}
if ($newLaunch -notmatch '(?i)-k\s+chuni-premium\.dll\b') {
    $newLaunch = [regex]::Replace($newLaunch, '(?im)^(.*\binject_x86\b.*?)\bchusanApp\.exe\b', '${1}-k chuni-premium.dll chusanApp.exe')
}
if ($Escape -and $newLaunch -notmatch '(?i)-k\s+chuni-escape\.dll\b') {
    $newLaunch = [regex]::Replace($newLaunch, '(?im)^(.*\binject_x86\b.*?)\bchusanApp\.exe\b', '${1}-k chuni-escape.dll chusanApp.exe')
}
foreach ($extra in @(@{Enabled=$Tracks; Name='tracks'}, @{Enabled=$Levels; Name='levels'})) {
    if ($extra.Enabled) {
        $dll = 'chuni-' + $extra.Name + '.dll'
        if ($newLaunch -notmatch ('(?i)-k\s+' + [regex]::Escape($dll) + '\b')) {
            $newLaunch = [regex]::Replace($newLaunch, '(?im)^(.*\binject_x86\b.*?)\bchusanApp\.exe\b', ('${1}-k ' + $dll + ' chusanApp.exe'))
        }
    }
}
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
foreach ($path in @($launchPath, $configPath)) {
    Copy-Item -LiteralPath $path -Destination "$path.before-extras-$stamp.bak"
}
foreach ($name in $dlls) {
    $destination = Join-Path $bin $name
    if (Test-Path -LiteralPath $destination) {
        Copy-Item -LiteralPath $destination -Destination "$destination.before-extras-$stamp.bak"
    }
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot "build/$name") -Destination $destination
}
$encoding = [Text.UTF8Encoding]::new($false)
[IO.File]::WriteAllText($configPath, $newConfig, $encoding)
[IO.File]::WriteAllText($launchPath, $newLaunch, $encoding)
$premiumConfig = Join-Path $bin 'chuni-premium.ini'
if (!(Test-Path -LiteralPath $premiumConfig)) {
    [IO.File]::WriteAllText($premiumConfig, "[premium]`r`nenabled=1`r`nticketId=2070`r`n[display]`r`nrestoreCredits=1`r`n", $encoding)
}
if ($Escape) {
    $escapeConfig = Join-Path $bin 'chuni-escape.ini'
    if (!(Test-Path -LiteralPath $escapeConfig)) {
        [IO.File]::WriteAllText($escapeConfig, "[escape]`r`nenabled=1`r`n", $encoding)
    }
}
foreach ($extra in @(@{Enabled=$Tracks; Name='tracks'}, @{Enabled=$Levels; Name='levels'})) {
    if ($extra.Enabled) {
        $path = Join-Path $bin ('chuni-' + $extra.Name + '.ini')
        if (!(Test-Path -LiteralPath $path)) {
            [IO.File]::WriteAllText($path, ('[' + $extra.Name + "]`r`nenabled=1`r`n"), $encoding)
        }
    }
}
Write-Output "Installed to $bin. Backups: *.before-extras-$stamp.bak"
Write-Output 'Start launch.bat and check the mod logs. No EXE was modified.'
