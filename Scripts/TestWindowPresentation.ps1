param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$WorkspaceRoot = Split-Path -Parent $PSScriptRoot
$Locator = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$Installation = & $Locator -latest -prerelease -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1

if (-not $Installation) {
    throw 'Visual Studio C++ tools were not found.'
}

$BuildDirectory = Join-Path $WorkspaceRoot 'Build/WindowPresentationTests'
$BinaryDirectory = Join-Path $WorkspaceRoot "Binaries/$Configuration"
$ResponseFile = Join-Path $BuildDirectory "Arguments-$Configuration.rsp"
$Executable = Join-Path $BuildDirectory "WindowPresentationTests-$Configuration.exe"
New-Item -ItemType Directory -Force -Path $BuildDirectory | Out-Null
Push-Location -LiteralPath $WorkspaceRoot

try {
    if (-not $SkipBuild) {
        & (Join-Path $Installation 'MSBuild/Current/Bin/MSBuild.exe') Macaw.slnx /t:Platform "/p:Configuration=$Configuration" /p:Platform=x64 /p:VcpkgManifestInstall=false /v:minimal /nologo

        if ($LASTEXITCODE -ne 0) {
            throw 'Platform build failed.'
        }
    }

    $CompilerOptions = @('/nologo', '/std:c++20', '/EHsc', '/utf-8', '/DNOMINMAX', '/DWIN32_LEAN_AND_MEAN', '/Zc:preprocessor', '/permissive-')
    $CompilerOptions += if ($Configuration -eq 'Debug') { @('/MDd', '/D_DEBUG') } else { @('/MD', '/DNDEBUG') }

    foreach ($Module in @('Platform', 'Core')) {
        $CompilerOptions += '/I"' + (Join-Path $WorkspaceRoot "Source/$Module/Public") + '"'
        $CompilerOptions += '/I"' + (Join-Path $WorkspaceRoot "Intermediate/$Module/Generated") + '"'
    }

    $CompilerOptions += '"' + (Join-Path $WorkspaceRoot 'Tests/WindowPresentationTests.cpp') + '"'
    $CompilerOptions += '/Fo"' + (Join-Path $BuildDirectory "WindowPresentationTests-$Configuration.obj") + '"'
    $CompilerOptions += '/Fe"' + $Executable + '"'
    $CompilerOptions += @('/link', ('/LIBPATH:"' + $BinaryDirectory + '"'))
    $CompilerOptions += if ($Configuration -eq 'Debug') { '/DEBUG' } else { '/LTCG' }
    $CompilerOptions += @('Platform.lib', 'Core.lib', 'd3d11.lib', 'dxgi.lib', 'dwmapi.lib', 'user32.lib', 'gdi32.lib', 'bcrypt.lib')
    Set-Content -LiteralPath $ResponseFile -Value ($CompilerOptions -join ' ') -Encoding utf8
    $DeveloperShell = Join-Path $Installation 'VC/Auxiliary/Build/vcvars64.bat'
    & $env:ComSpec /d /s /c ('call "{0}" >nul && cl.exe @"{1}"' -f $DeveloperShell, $ResponseFile)

    if ($LASTEXITCODE -ne 0) {
        throw 'Window presentation test compilation failed.'
    }

    & $Executable

    if ($LASTEXITCODE -ne 0) {
        throw 'Window presentation tests failed.'
    }
} finally {
    Pop-Location
}
