param([Parameter(Mandatory=$true)][string]$Dependencies,
      [Parameter(Mandatory=$true)][string]$Output,
      [Parameter(Mandatory=$true)][string]$Zig)
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
New-Item -ItemType Directory $Output -Force | Out-Null
$args=@('c++','-target','x86_64-windows-gnu','-std=c++17','-O1',
    '-DCLANBOMBER_DESKTOP','-DENABLE_NLS=0','-DPACKAGE="clanbomber2"',
    '-DPACKAGE_VERSION="2.0"','-DCB_DATADIR="ClanBomber2"',
    '-DCB_LOCALEDIR="ClanBomber2/locale"','-I',"$repo/src",'-I',"$repo/upstream/src")
foreach($name in @('SDL2-2.30.12','SDL2_image-2.8.2','SDL2_mixer-2.8.0','SDL2_ttf-2.22.0')) {
    $base="$Dependencies/$name/x86_64-w64-mingw32"
    $args+=@('-I',"$base/include",'-I',"$base/include/SDL2",'-L',"$base/lib")
}
$args+=(Get-ChildItem "$repo/upstream/src" -Filter '*.cpp' | ForEach-Object FullName)
$args+="$repo/desktop/desktop_paths.cpp"
$args+=@('-lmingw32',"$Dependencies/SDL2-2.30.12/x86_64-w64-mingw32/lib/libSDL2main.a",
    "$Dependencies/SDL2_image-2.8.2/x86_64-w64-mingw32/lib/libSDL2_image.dll.a",
    "$Dependencies/SDL2_mixer-2.8.0/x86_64-w64-mingw32/lib/libSDL2_mixer.dll.a",
    "$Dependencies/SDL2_ttf-2.22.0/x86_64-w64-mingw32/lib/libSDL2_ttf.dll.a",
    "$Dependencies/SDL2-2.30.12/x86_64-w64-mingw32/lib/libSDL2.dll.a",
    '-lshell32','-lole32','-o',"$Output/ClanBomber.exe")
& $Zig @args
if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}
Write-Output 'Windows executable compiled'
