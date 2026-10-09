param([Parameter(Mandatory=$true)][string]$GameBin, [switch]$PreserveMode)
$ErrorActionPreference='Stop'
$bin=(Resolve-Path -LiteralPath $GameBin).Path
if (Get-Process chusanApp,amdaemon -ErrorAction SilentlyContinue) {throw 'Close the game and amdaemon before installing.'}
$source=Join-Path $PSScriptRoot 'build/chuni-aime.dll'
if (!(Test-Path -LiteralPath $source)) {throw 'Build and test chuni-aime.dll first.'}
if (!(Test-Path -LiteralPath (Join-Path $bin 'chusanhook_x64.dll'))) {throw 'Expected x64 Segatools hook is missing.'}
$iniPath=Join-Path $bin 'segatools.ini'
$ini=[IO.File]::ReadAllText($iniPath)
$sections=[regex]::Matches($ini,'(?ims)^\[aimeio\][ \t]*\r?\n(.*?)(?=^\[|\z)')
if ($sections.Count -ne 1) {throw 'Expected exactly one [aimeio] section.'}
$section=$sections[0]
$paths=[regex]::Matches($section.Groups[1].Value,'(?im)^[ \t]*path[ \t]*=[ \t]*([^\r\n;]*)')
if ($paths.Count -gt 1) {throw 'Multiple active AimeIO paths; inspect configuration.'}
if ($paths.Count -eq 1 -and $paths[0].Groups[1].Value.Trim() -notin @('','chuni-aime.dll')) {
    throw 'Another AimeIO driver is configured; no files changed.'
}
$newline=if ($ini.Contains("`r`n")) {"`r`n"} else {"`n"}
if ($paths.Count) {
    $newSection=[regex]::Replace($section.Value,'(?im)^[ \t]*path[ \t]*=[^\r\n]*','path=chuni-aime.dll')
} else {
    $newSection=[regex]::Replace($section.Value,'(?im)^(\[aimeio\][ \t]*)(?:\r?\n|$)',('$1'+$newline+'path=chuni-aime.dll'+$newline))
}
$newIni=$ini.Substring(0,$section.Index)+$newSection+$ini.Substring($section.Index+$section.Length)
$config=Join-Path $bin 'chuni-aime.ini'
if (Test-Path -LiteralPath $config) {
    $text=[IO.File]::ReadAllText($config)
    # Only the module's single setting is edited; preserve all other contents.
    $aimeSections=[regex]::Matches($text,'(?ims)^\[aime\][ \t]*\r?\n(.*?)(?=^\[|\z)')
    if ($aimeSections.Count -ne 1) {throw 'Expected exactly one [aime] section in chuni-aime.ini.'}
    $aimeSection=$aimeSections[0]
    $enabled=[regex]::Matches($aimeSection.Value,'(?im)^[ \t]*enabled[ \t]*=')
    if ($enabled.Count -gt 1) {throw 'Multiple enabled settings in chuni-aime.ini.'}
    if ($enabled.Count) {
        $replacement=[regex]::Replace($aimeSection.Value,'(?im)^[ \t]*enabled[ \t]*=[^\r\n]*','enabled=1')
    } else {
        $replacement=[regex]::Replace($aimeSection.Value,'(?im)^(\[aime\][ \t]*)(?:\r?\n|$)',('$1'+$newline+'enabled=1'+$newline))
    }
    $newConfig=$text.Substring(0,$aimeSection.Index)+$replacement+$text.Substring($aimeSection.Index+$aimeSection.Length)
    if ($PreserveMode) {$newConfig=$text}
} else {
    $newConfig="; enabled=1: automatic current-card scan.`r`n; enabled=0: manual scan using [aime] scan in segatools.ini (default ENTER).`r`n[aime]`r`nenabled=1`r`n"
    if ($PreserveMode) {$newConfig=$newConfig.Replace('enabled=1'+"`r`n",'enabled=0'+"`r`n")}
}
$stamp=Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$dll=Join-Path $bin 'chuni-aime.dll'
foreach ($path in @($iniPath,$config,$dll)) {
    if (Test-Path -LiteralPath $path) {Copy-Item -LiteralPath $path -Destination "$path.before-aime-$stamp.bak"}
}
Copy-Item -LiteralPath $source -Destination $dll -Force
$encoding=[Text.UTF8Encoding]::new($false)
[IO.File]::WriteAllText($config,$newConfig,$encoding)
[IO.File]::WriteAllText($iniPath,$newIni,$encoding)
Write-Output "Installed AimeIO into $bin. Backups: *.before-aime-$stamp.bak"
Write-Output 'Disable automatic scanning with [aime] enabled=0 in chuni-aime.ini.'
