#!/usr/bin/env bash
set -euo pipefail

PKG_NAME="fd_usb_02b10_ros2"
RULE_NAME="99-fd-usb-02b10.rules"
SYMLINK_NAME="fd-usb-02b10"
REQUIRED_GROUP="video"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TARGET_USER="${SUDO_USER:-$USER}"

# 1. Locate the rule file (supports both source repo and installed colcon workspace)
if [[ -f "${SCRIPT_DIR}/../udev/${RULE_NAME}" ]]; then
    RULE_PATH="${SCRIPT_DIR}/../udev/${RULE_NAME}"
elif [[ -f "${SCRIPT_DIR}/../../share/${PKG_NAME}/udev/${RULE_NAME}" ]]; then
    RULE_PATH="${SCRIPT_DIR}/../../share/${PKG_NAME}/udev/${RULE_NAME}"
else
    echo "Error: Could not locate ${RULE_NAME}."
    echo "Expected it in ../udev/ or ../../share/${PKG_NAME}/udev/ relative to this script."
    exit 1
fi

# 2. Install the udev rule
echo "[1/3] Installing ${RULE_NAME} to /etc/udev/rules.d/..."
sudo cp "${RULE_PATH}" "/etc/udev/rules.d/${RULE_NAME}"
sudo chmod 644 "/etc/udev/rules.d/${RULE_NAME}"

# 3. Reload udev rules
echo "[2/3] Reloading udev rules..."
sudo udevadm control --reload-rules
sudo udevadm trigger --subsystem-match=video4linux

# 4. Ensure user is in the 'video' group (required by MODE="0660", GROUP="video")
echo "[3/3] Checking '${REQUIRED_GROUP}' group membership for user '${TARGET_USER}'..."
NEED_RELOGIN=false
if id -nG "${TARGET_USER}" | grep -qw "${REQUIRED_GROUP}"; then
    echo "      User '${TARGET_USER}' is already in the '${REQUIRED_GROUP}' group."
else
    echo "      Adding '${TARGET_USER}' to the '${REQUIRED_GROUP}' group..."
    sudo usermod -aG "${REQUIRED_GROUP}" "${TARGET_USER}"
    NEED_RELOGIN=true
fi

echo ""
echo "Installation complete!"
if [[ -e "/dev/${SYMLINK_NAME}" ]]; then
    echo "Device detected at /dev/${SYMLINK_NAME} -> $(readlink -f "/dev/${SYMLINK_NAME}")"
else
    echo "Unplug and replug the camera to create /dev/${SYMLINK_NAME}."
fi

if [[ "${NEED_RELOGIN}" == true ]]; then
    echo "IMPORTANT: Log out and log back in (or reboot) for '${REQUIRED_GROUP}' group permissions to apply."
fi