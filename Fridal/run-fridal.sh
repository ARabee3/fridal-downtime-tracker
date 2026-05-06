#!/usr/bin/env bash
set -euo pipefail

SKIP_BROWSER=0
if [[ "${1:-}" == "--skip-browser" ]]; then
  SKIP_BROWSER=1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BACKEND_DIR="$SCRIPT_DIR/backend"

info() { echo "[INFO] $*"; }
ok() { echo "[OK]   $*"; }
warn() { echo "[WARN] $*"; }

need_sudo() {
  if [[ "${EUID}" -ne 0 ]]; then
    if command -v sudo >/dev/null 2>&1; then
      sudo "$@"
    else
      echo "This action needs root privileges, and 'sudo' is not available." >&2
      exit 1
    fi
  else
    "$@"
  fi
}

install_node_if_missing() {
  if command -v node >/dev/null 2>&1 && command -v npm >/dev/null 2>&1; then
    return 0
  fi

  info "Node.js/npm not found. Attempting installation..."

  if command -v apt-get >/dev/null 2>&1; then
    need_sudo apt-get update
    need_sudo apt-get install -y nodejs npm
  elif command -v dnf >/dev/null 2>&1; then
    need_sudo dnf install -y nodejs npm
  elif command -v yum >/dev/null 2>&1; then
    need_sudo yum install -y nodejs npm
  elif command -v pacman >/dev/null 2>&1; then
    need_sudo pacman -Sy --noconfirm nodejs npm
  elif command -v zypper >/dev/null 2>&1; then
    need_sudo zypper --non-interactive install nodejs20 npm20 || need_sudo zypper --non-interactive install nodejs npm
  else
    warn "Unsupported Linux package manager."
    echo "Install Node.js LTS manually from https://nodejs.org/ and rerun this script." >&2
    exit 1
  fi

  if ! command -v node >/dev/null 2>&1 || ! command -v npm >/dev/null 2>&1; then
    warn "Node.js installation did not complete correctly."
    echo "Open a new shell and run this script again." >&2
    exit 1
  fi

  ok "Node.js/npm installed"
}

if [[ ! -d "$BACKEND_DIR" ]]; then
  echo "Backend directory not found at: $BACKEND_DIR" >&2
  exit 1
fi

install_node_if_missing

info "Using Node $(node -v) and npm $(npm -v)"

cd "$BACKEND_DIR"

if [[ ! -f package.json ]]; then
  echo "package.json not found in backend directory." >&2
  exit 1
fi

info "Installing backend dependencies (npm install)..."
npm install
ok "Dependencies are ready"

if [[ "$SKIP_BROWSER" -eq 0 ]]; then
  if command -v xdg-open >/dev/null 2>&1; then
    xdg-open "http://localhost:3000" >/dev/null 2>&1 || true
  fi
fi

info "Starting Fridail backend server..."
echo "Press Ctrl+C to stop."

node server.js
