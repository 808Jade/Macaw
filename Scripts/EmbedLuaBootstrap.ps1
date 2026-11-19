param(
    [Parameter(Mandatory = $true)][string]$SourcePath,
    [Parameter(Mandatory = $true)][string]$HeaderPath
)

$ErrorActionPreference = 'Stop'
$Bytes = [Text.Encoding]::UTF8.GetBytes([IO.File]::ReadAllText([IO.Path]::GetFullPath($SourcePath)))
$Output = [IO.Path]::GetFullPath($HeaderPath)
$Header = [Text.StringBuilder]::new()

[void]$Header.AppendLine('#pragma once')
[void]$Header.AppendLine()
[void]$Header.AppendLine('namespace Engine {')
[void]$Header.AppendLine()
[void]$Header.AppendLine('    inline constexpr unsigned char LuaBootstrapSource[]{')

for ($Offset = 0; $Offset -lt $Bytes.Length; $Offset += 16) {
    $End = [Math]::Min($Offset + 16, $Bytes.Length)
    $Values = for ($Index = $Offset; $Index -lt $End; ++$Index) {
        '0x{0:X2},' -f $Bytes[$Index]
    }
    [void]$Header.AppendLine('        ' + ($Values -join ' '))
}

[void]$Header.AppendLine('    };')
[void]$Header.AppendLine()
[void]$Header.AppendLine('}')

[void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($Output))
[IO.File]::WriteAllText($Output, $Header.ToString())
