param(
    [Parameter(Mandatory = $true)]
    [string]$GamePackage
)
$ErrorActionPreference = 'Stop'
$modSource = $PSScriptRoot
$package = (Resolve-Path -LiteralPath $GamePackage).Path
$compiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
$output = Join-Path $modSource 'build'
foreach ($dependency in @('Sinmai.exe', 'MelonLoader\net35\MelonLoader.dll', 'MelonLoader\net35\0Harmony.dll')) {
    if (-not (Test-Path -LiteralPath (Join-Path $package $dependency))) {
        throw "Missing game dependency: $dependency. Pass the SDEZ 1.66 Package directory."
    }
}
New-Item -ItemType Directory -Path $output -Force | Out-Null
& $compiler /nologo "/out:$output\Tests.exe" "$modSource\tests\Tests.cs" "$modSource\src\TitleFormatter.cs"
if ($LASTEXITCODE -ne 0) { throw 'Test compilation failed' }
& "$output\Tests.exe"
if ($LASTEXITCODE -ne 0) { throw 'Behavior tests failed' }
& $compiler /nologo /target:library "/out:$output\NativeTitleLevel.dll" "/reference:$package\MelonLoader\net35\MelonLoader.dll" "/reference:$package\MelonLoader\net35\0Harmony.dll" "$modSource\src\NativeTitleLevel.cs" "$modSource\src\TitleFormatter.cs"
if ($LASTEXITCODE -ne 0) { throw 'Mod compilation failed' }
Write-Output "Built $output\NativeTitleLevel.dll. Close the game before copying it to Package\Mods."