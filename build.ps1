[CmdletBinding()]
param(
    [ValidateSet('x64', 'Win32')][string]$Platform = 'x64',
    [string]$PlatformToolset = 'v143'
)
$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (!(Test-Path -LiteralPath $vswhere)) { throw 'Install Visual Studio with Desktop development with C++ and a Windows SDK.' }
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$installation) { throw 'C++ build tools are missing. Install Desktop development with C++, MSVC v143, and Windows 10/11 SDK in Visual Studio Installer.' }
$msbuild = Join-Path $installation 'MSBuild/Current/Bin/MSBuild.exe'
if (!(Test-Path -LiteralPath $msbuild)) { throw "MSBuild not found: $msbuild" }
$outputRoot = Join-Path $projectRoot "artifacts/$Platform"
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
$testOutput = Join-Path $outputRoot 'tests'
& $msbuild (Join-Path $projectRoot 'tests/security_doors_tests.vcxproj') /nologo /v:minimal /t:Rebuild `
    '/p:Configuration=Release' "/p:Platform=$Platform" "/p:PlatformToolset=$PlatformToolset" `
    "/p:OutDir=$testOutput/" "/p:IntDir=$testOutput/obj/"
if ($LASTEXITCODE -ne 0) { throw 'Security door tests failed to build.' }
& (Join-Path $testOutput 'security_doors_tests.exe')
if ($LASTEXITCODE -ne 0) { throw 'Security door tests failed.' }
foreach ($configuration in @('Release', 'Release_Version')) {
    $buildOutput = Join-Path $outputRoot $configuration
    $intermediates = Join-Path $projectRoot "artifacts/obj/$Platform/$configuration"
    & $msbuild (Join-Path $projectRoot 'SickoMenu.sln') /m:2 /nologo /v:minimal /t:Rebuild `
        "/p:Configuration=$configuration" "/p:Platform=$Platform" "/p:PlatformToolset=$PlatformToolset" `
        "/p:OutDir=$buildOutput/" "/p:IntDir=$intermediates/" "/flp:logfile=$outputRoot/$configuration.log;verbosity=normal"
    if ($LASTEXITCODE -ne 0) { throw "Build failed: $configuration ($Platform). See $outputRoot/$configuration.log" }
}
$package = Join-Path $outputRoot 'package'
New-Item -ItemType Directory -Path $package -Force | Out-Null
foreach ($item in @(@('Release/SickoMenu.dll', 'SickoMenu.dll'), @('Release_Version/version.dll', 'version.dll'))) {
    $binary = Join-Path $outputRoot $item[0]
    if (!(Test-Path -LiteralPath $binary)) { throw "Missing build output: $binary" }
    Copy-Item -LiteralPath $binary -Destination (Join-Path $package $item[1]) -Force
}
Copy-Item -LiteralPath (Join-Path $projectRoot 'LICENSE'), (Join-Path $projectRoot 'MERGE_NOTES.md') -Destination $package -Force
$hashes = @('SickoMenu.dll', 'version.dll') | ForEach-Object {
    $hash = Get-FileHash -LiteralPath (Join-Path $package $_) -Algorithm SHA256
    "$($hash.Hash.ToLowerInvariant())  $_"
}
$hashes | Set-Content -LiteralPath (Join-Path $package 'SHA256SUMS.txt') -Encoding ascii
$archive = Join-Path $outputRoot "SickoMenu-merged-$Platform.zip"
Compress-Archive -Path (Join-Path $package '*') -DestinationPath $archive -Force
Write-Output "Built and packaged: $archive"
