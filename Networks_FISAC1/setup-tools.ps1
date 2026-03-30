# Prepends MinGW and project SQLite CLI to PATH for this PowerShell session.
# SQLite CLI was also installed via winget (SQLite.SQLite); restart the terminal to use those aliases.

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$mingw = "C:\mingw64\bin"
$sqlite = Join-Path $root "tools\sqlite"

if (Test-Path $mingw) {
    $env:PATH = "$mingw;$env:PATH"
    Write-Host "Prepended: $mingw"
}
if (Test-Path $sqlite) {
    $env:PATH = "$sqlite;$env:PATH"
    Write-Host "Prepended: $sqlite"
}

Write-Host "gcc: " -NoNewline
& gcc --version 2>$null | Select-Object -First 1
Write-Host "sqlite3 (project): " -NoNewline
$p = Join-Path $sqlite "sqlite3.exe"
if (Test-Path $p) { & $p -version } else { Write-Host "(not extracted)" }
