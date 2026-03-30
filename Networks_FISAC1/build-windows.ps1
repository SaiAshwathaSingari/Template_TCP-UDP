# Build collab_server.exe (Windows / MinGW) with bundled SQLite amalgamation.
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$gcc = "C:\mingw64\bin\gcc.exe"
if (-not (Test-Path $gcc)) {
    Write-Error "MinGW gcc not found at $gcc. Install MinGW-w64 or edit this script."
}
New-Item -ItemType Directory -Path (Join-Path $root "build") -Force | Out-Null
$src = Join-Path $root "server"
$out = Join-Path $root "build\collab_server.exe"
& $gcc -Wall -O2 -DSQLITE_THREADSAFE=1 -I$src -o $out `
    (Join-Path $src "server.c") `
    (Join-Path $src "websocket.c") `
    (Join-Path $src "database.c") `
    (Join-Path $src "sqlite3.c") `
    -lws2_32 -lpthread -lm -lcrypt32
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host "Built: $out"
