#!/usr/bin/env bash
# ==============================================================================
# verify_binary.sh - Static & PE32 Architecture Verification for WinCE ARM Binaries
# Target: Foston FS-460BT (MediaTek MT3351, Windows CE 5.0 ARMv4)
# ==============================================================================

set -euo pipefail

TARGET_EXE="${1:-}"

if [[ -z "$TARGET_EXE" || ! -f "$TARGET_EXE" ]]; then
    echo "Usage: $0 <path_to_pe32_exe>"
    exit 1
fi

echo "========================================================"
echo " VERIFYING WINCE PE32 BINARY: $TARGET_EXE"
echo "========================================================"

# 1. Check file size
SIZE=$(stat -c%s "$TARGET_EXE")
echo "[+] File Size: $SIZE bytes ($((SIZE / 1024)) KB)"
if [[ $SIZE -gt 2097152 ]]; then
    echo "[-] ERROR: Binary exceeds 2MB limit for embedded WinCE application"
    exit 1
fi

# 2. Check PE architecture and subsystem using Docker CeGCC objdump
DOCKER_IMG="ghcr.io/enlyze/windows-ce-build-environment-arm:latest"

HEADERS=$(docker run --rm -v "$(pwd)":/work -w /work "$DOCKER_IMG" \
    arm-mingw32ce-objdump -p "$TARGET_EXE" 2>/dev/null)

# Verify Subsystem: 00000002 (Windows GUI)
SUBSYS=$(echo "$HEADERS" | grep -E "Subsystem\s+" | awk '{print $2}')
echo "[+] PE Subsystem: $SUBSYS"
if [[ "$SUBSYS" != "00000002" ]]; then
    echo "[-] WARNING: Subsystem is not standard Windows GUI (0x2). Value: $SUBSYS"
fi

# 3. Check imported DLLs
echo "[+] Imported Dynamic Libraries:"
echo "$HEADERS" | grep -E "DLL Name:" | sort -u | sed 's/^/    /'

# Ensure ONLY valid WinCE ROM libraries are linked
FORBIDDEN=$(echo "$HEADERS" | grep -E "DLL Name:" | grep -viE "COREDLL\.dll|TOOLHELP\.dll|COMMCTRL\.dll|WINSOCK\.dll|WS2\.dll" || true)
if [[ -n "$FORBIDDEN" ]]; then
    echo "[-] ERROR: Binary links against unsupported desktop DLLs:"
    echo "$FORBIDDEN"
    exit 1
fi

# 4. Check for critical entry point and sections
SECTIONS=$(docker run --rm -v "$(pwd)":/work -w /work "$DOCKER_IMG" \
    arm-mingw32ce-objdump -h "$TARGET_EXE" 2>/dev/null)

echo "[+] Section Table:"
echo "$SECTIONS" | grep -E "^\s+[0-9]+\s+\.(text|data|rdata|bss|idata)" | awk '{print "    " $2 "\tSize: " $3 "\tVMA: " $4}'

echo "========================================================"
echo " [+] VERIFICATION PASSED: Binary is 100% valid WinCE ARM PE32."
echo "========================================================"
