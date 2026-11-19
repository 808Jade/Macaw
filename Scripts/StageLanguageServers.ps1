param(
    [Parameter(Mandatory = $true)][string]$RuntimeDirectory,
    [Parameter(Mandatory = $true)][string]$WorkspaceRoot
)

$ErrorActionPreference = 'Stop'
$RuntimePath = [IO.Path]::GetFullPath($RuntimeDirectory)
$WorkspacePath = [IO.Path]::GetFullPath($WorkspaceRoot)
$ToolsPath = Join-Path $WorkspacePath 'vcpkg_installed/x64-windows/tools'
$DeclarationDirectory = Join-Path $WorkspacePath 'Resources/Lua'
$LuaDestination = Join-Path $RuntimePath 'LuaLanguageServer'
$HlslDestination = Join-Path $RuntimePath 'HlslLanguageServer'

function Copy-LanguageServerFiles {
    param([string]$SourcePath, [string]$DestinationPath, [string[]]$CopyOptions)

    & robocopy.exe $SourcePath $DestinationPath @CopyOptions /XJ /R:1 /W:1 /NFL /NDL /NJH /NJS /NP

    if ($LASTEXITCODE -ge 8) {
        throw "Language server staging failed from '$SourcePath' to '$DestinationPath' with robocopy exit code $LASTEXITCODE."
    }
}

if (-not (Test-Path -LiteralPath (Join-Path $DeclarationDirectory 'Macaw.d.lua') -PathType Leaf)) {
    throw "Macaw.d.lua is missing in '$DeclarationDirectory'."
}

Copy-LanguageServerFiles (Join-Path $ToolsPath 'lua-language-server') $LuaDestination @('/E')
Copy-LanguageServerFiles $DeclarationDirectory $LuaDestination @('Macaw.d.lua')
Copy-LanguageServerFiles (Join-Path $ToolsPath 'hlsl-language-server') $HlslDestination @('/E')

exit 0
