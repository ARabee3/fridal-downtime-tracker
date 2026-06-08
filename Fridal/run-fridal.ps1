param(
    [switch]$SkipBrowser
)

$ErrorActionPreference = "Stop"

function Write-Info($msg) {
    Write-Host "[INFO] $msg" -ForegroundColor Cyan
}

function Write-Ok($msg) {
    Write-Host "[OK]   $msg" -ForegroundColor Green
}

function Write-WarnMsg($msg) {
    Write-Host "[WARN] $msg" -ForegroundColor Yellow
}

function Test-Command($name) {
    return [bool](Get-Command $name -ErrorAction SilentlyContinue)
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$backendDir = Join-Path $scriptDir "backend"

if (-not (Test-Path $backendDir)) {
    throw "Backend directory not found at: $backendDir"
}

if (-not (Test-Command "node") -or -not (Test-Command "npm")) {
    Write-Info "Node.js/npm not found. Attempting installation..."

    if (-not (Test-Command "winget")) {
        Write-WarnMsg "winget is not available on this system."
        Write-Host "Install Node.js LTS manually from https://nodejs.org/en/download and run this script again." -ForegroundColor Red
        exit 1
    }

    winget install OpenJS.NodeJS.LTS --accept-source-agreements --accept-package-agreements --silent

    $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")

    if (-not (Test-Command "node") -or -not (Test-Command "npm")) {
        Write-WarnMsg "Node.js was installed but this shell does not see it yet."
        Write-Host "Please close and reopen PowerShell, then run this script again." -ForegroundColor Yellow
        exit 1
    }

    Write-Ok "Node.js/npm installed"
}

Write-Info "Using Node $(node -v) and npm $(npm -v)"

Set-Location $backendDir

if (-not (Test-Path (Join-Path $backendDir "package.json"))) {
    throw "package.json not found in backend directory."
}

Write-Info "Installing backend dependencies (npm install)..."
npm install

Write-Ok "Dependencies are ready"

if (-not $SkipBrowser) {
    Start-Process "http://localhost:3000"
}

Write-Info "Starting FRIDAL backend server..."
Write-Host "Press Ctrl+C to stop."

node server.js
