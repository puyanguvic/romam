#!/bin/sh
set -eu

CFG_PATH="$1"
LOG_LEVEL="${2:-INFO}"
BOOT_LOG="/tmp/routerd-bootstrap.log"
ROUTERD_LOG="/tmp/routerd.log"

has_yaml() {
  python3 -c 'import yaml' >/dev/null 2>&1
}

install_with_pip() {
  if python3 -m pip --version >/dev/null 2>&1; then
    python3 -m pip install --no-cache-dir pyyaml
    return $?
  fi
  python3 -m ensurepip --upgrade || true
  if python3 -m pip --version >/dev/null 2>&1; then
    python3 -m pip install --no-cache-dir pyyaml
    return $?
  fi
  return 1
}

install_with_pkg_manager() {
  if command -v apk >/dev/null 2>&1; then
    apk add --no-cache py3-yaml
    return $?
  fi
  if command -v apt-get >/dev/null 2>&1; then
    apt-get update
    DEBIAN_FRONTEND=noninteractive apt-get install -y python3-yaml
    return $?
  fi
  if command -v dnf >/dev/null 2>&1; then
    dnf install -y python3-pyyaml
    return $?
  fi
  if command -v yum >/dev/null 2>&1; then
    yum install -y python3-pyyaml
    return $?
  fi
  return 1
}

if ! has_yaml; then
  {
    echo "[bootstrap] PyYAML missing, trying pip/package-manager install"
    install_with_pip || install_with_pkg_manager || true
    if has_yaml; then
      echo "[bootstrap] PyYAML install succeeded"
    else
      echo "[bootstrap] ERROR: PyYAML install failed"
    fi
  } >>"$BOOT_LOG" 2>&1
fi

if ! has_yaml; then
  exit 1
fi

PYTHONPATH=/irp/src nohup python3 -m irp.routerd --config "$CFG_PATH" --log-level "$LOG_LEVEL" >"$ROUTERD_LOG" 2>&1 &
