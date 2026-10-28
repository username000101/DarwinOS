#!/usr/bin/env bash
# ------------------------------------------------------------------------
# darwinOS - Portable Linux ISO Image Builder (Dual BIOS + UEFI)
# ------------------------------------------------------------------------
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_SOURCE_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
ROOT_BINARY_DIR="${ROOT_BINARY_DIR:-/home/${USER:-$(id -un)}/build/darwinos}"
SYSROOT_DIR="${ROOT_BINARY_DIR}/sysroot"
DIST_DIR="${ROOT_SOURCE_DIR}/dist"
ISO_NAME="darwinOS.iso"
OUTPUT_ISO="${DIST_DIR}/${ISO_NAME}"
ISO_STAGING="${ROOT_BINARY_DIR}/iso_staging"

# ANSI Colors
CYAN='\033[0;36m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BOLD='\033[1m'
RESET='\033[0m'

log_info() { echo -e "${CYAN}[ISO INFO]${RESET} $*"; }
log_ok()   { echo -e "${GREEN}[ISO OK]${RESET} $*"; }
log_warn() { echo -e "${YELLOW}[ISO WARN]${RESET} $*"; }
log_err()  { echo -e "${RED}[ISO ERROR]${RESET} $*"; }

echo -e "\n${BOLD}${CYAN}=== Starting Portable Dual-Boot (BIOS + UEFI) ISO Generation ===${RESET}\n"

# 1. Check prerequisites
for tool in xorriso mtools mkfs.vfat; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        log_err "Missing required tool: $tool"
        echo "Please install it via: sudo pacman -S --noconfirm xorriso mtools dosfstools grub"
        exit 1
    fi
done

# 2. Check sysroot
if [ ! -d "$SYSROOT_DIR" ] || [ ! -d "$SYSROOT_DIR/System/Library/Frameworks" ]; then
    log_err "sysroot is missing or incomplete at: $SYSROOT_DIR"
    echo "Please run './build.sh all' first."
    exit 1
fi

mkdir -p "$DIST_DIR"
rm -rf "$ISO_STAGING"
mkdir -p "$ISO_STAGING"

# 3. Populate base directory structure from sysroot
log_info "Populating ISO file structure from sysroot..."
cp -a "$SYSROOT_DIR/." "$ISO_STAGING/"

# 4. Integrate system version and configuration files
if [ -f "$ROOT_SOURCE_DIR/SystemLibrary/SystemVersion.plist" ]; then
    mkdir -p "$ISO_STAGING/System/Library/CoreServices"
    cp -fv "$ROOT_SOURCE_DIR/SystemLibrary/SystemVersion.plist" "$ISO_STAGING/System/Library/CoreServices/"
fi

mkdir -p "$ISO_STAGING/boot/grub"
mkdir -p "$ISO_STAGING/etc"

# Prepare patched efiemu runtime (fixes missing 'memset' and '_GLOBAL_OFFSET_TABLE_' symbols in upstream GRUB i386-pc efiemu runtime)
log_info "Preparing GRUB efiemu runtime..."
grub_i386_pc="/usr/lib/grub/i386-pc"
if [ -f "$grub_i386_pc/efiemu64.o" ]; then
    mkdir -p "$ISO_STAGING/boot/grub/i386-pc"
    tmp_efiemu="$(mktemp -d)"
    cat << 'EOF' > "$tmp_efiemu/memset.c"
void *memset(void *s, int c, unsigned long n) {
    unsigned char *p = (unsigned char *)s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}
EOF
    cat << 'EOF' > "$tmp_efiemu/stubs.s"
.globl _GLOBAL_OFFSET_TABLE_
_GLOBAL_OFFSET_TABLE_:
.quad 0
EOF
    gcc -fno-builtin -fno-tree-loop-distribute-patterns -m64 -nostdlib -c "$tmp_efiemu/memset.c" -o "$tmp_efiemu/memset64.o"
    gcc -c -m64 "$tmp_efiemu/stubs.s" -o "$tmp_efiemu/stubs64.o"
    ld -r -m elf_x86_64 "$grub_i386_pc/efiemu64.o" "$tmp_efiemu/memset64.o" "$tmp_efiemu/stubs64.o" -o "$ISO_STAGING/boot/grub/i386-pc/efiemu64.o"
    if [ -f "$grub_i386_pc/efiemu32.o" ]; then
        gcc -fno-builtin -fno-tree-loop-distribute-patterns -m32 -nostdlib -c "$tmp_efiemu/memset.c" -o "$tmp_efiemu/memset32.o"
        gcc -c -m32 "$tmp_efiemu/stubs.s" -o "$tmp_efiemu/stubs32.o"
        ld -r -m elf_i386 "$grub_i386_pc/efiemu32.o" "$tmp_efiemu/memset32.o" "$tmp_efiemu/stubs32.o" -o "$ISO_STAGING/boot/grub/i386-pc/efiemu32.o"
    fi
    rm -rf "$tmp_efiemu"
    log_ok "Patched efiemu runtime (efiemu64.o, efiemu32.o) installed into ISO staging."
fi

# Patch macho.mod for x86_64-efi (fixes GRUB upstream 64-bit vmaddr truncation page fault)
if [ -f "/usr/lib/grub/x86_64-efi/macho.mod" ]; then
    mkdir -p "$ISO_STAGING/boot/grub/x86_64-efi"
    python3 - "$ISO_STAGING/boot/grub/x86_64-efi/macho.mod" << 'PYEOF'
import sys
outpath = sys.argv[1]
with open('/usr/lib/grub/x86_64-efi/macho.mod', 'rb') as f:
    data = bytearray(f.read())
off1 = 0x40 + 0x1116
off2 = 0x40 + 0x1151
off3 = 0x40 + 0x11e0
off4 = 0x40 + 0x126f
if data[off1:off1+4] == bytes.fromhex('488b7b18'):
    data[off1:off1+4] = bytes.fromhex('8b7b1890')
if data[off2:off2+4] == bytes.fromhex('488b7318'):
    data[off2:off2+4] = bytes.fromhex('8b731890')
if data[off3:off3+4] == bytes.fromhex('4c8b6318'):
    data[off3:off3+4] = bytes.fromhex('448b6318')
if data[off4:off4+4] == bytes.fromhex('48037b18'):
    data[off4:off4+4] = bytes.fromhex('037b1890')
with open(outpath, 'wb') as f:
    f.write(data)
PYEOF
    log_ok "Patched x86_64-efi macho.mod installed into ISO staging."
fi

# Patch xnu.mod for x86_64-efi (bypasses crashing SetVirtualAddressMap in UEFI)
if [ -f "/usr/lib/grub/x86_64-efi/xnu.mod" ]; then
    mkdir -p "$ISO_STAGING/boot/grub/x86_64-efi"
    python3 - "$ISO_STAGING/boot/grub/x86_64-efi/xnu.mod" << 'PYEOF'
import sys
outpath = sys.argv[1]
with open('/usr/lib/grub/x86_64-efi/xnu.mod', 'rb') as f:
    data = bytearray(f.read())
off = 0x40 + 0x1990
if data[off:off+2] == bytes.fromhex('ffd0'):
    data[off:off+2] = bytes.fromhex('9090')
with open(outpath, 'wb') as f:
    f.write(data)
PYEOF
    log_ok "Patched x86_64-efi xnu.mod installed into ISO staging."
fi

if [ -f "$ROOT_SOURCE_DIR/ISO/overlays/boot/boot/loader.conf" ]; then
    cp -fv "$ROOT_SOURCE_DIR/ISO/overlays/boot/boot/loader.conf" "$ISO_STAGING/boot/"
fi

if [ -f "$ROOT_SOURCE_DIR/ISO/settings/rc.conf" ]; then
    cp -fv "$ROOT_SOURCE_DIR/ISO/settings/rc.conf" "$ISO_STAGING/etc/"
fi

# Ensure /etc/fstab exists
cat << 'EOF' > "$ISO_STAGING/etc/fstab"
# Device        Mountpoint      FStype  Options Dump    Pass#
/dev/iso9660/DARWINOS  /               cd9660  ro      0       0
tmpfs           /tmp            tmpfs   rw,mode=1777 0  0
EOF

# Copy lightweight fonts for instant 60 FPS gfxterm rendering (avoids 2.4MB unicode.pf2 disk bottleneck)
mkdir -p "$ISO_STAGING/boot/grub/fonts"
for fnt in ascii.pf2 euro.pf2; do
    if [ -f "/usr/share/grub/$fnt" ]; then
        cp -fv "/usr/share/grub/$fnt" "$ISO_STAGING/boot/grub/fonts/"
    fi
done

# 5. Create GRUB configuration supporting both Legacy BIOS and UEFI
# Timeout=0 for instantaneous direct kernel boot without menu delays
cat << 'EOF' > "$ISO_STAGING/boot/grub/grub.cfg"
set timeout=0
set default=0

menuentry "darwinOS (Verbose Kernel Boot)" {
    insmod all_video
    insmod macho
    insmod xnu
    if [ "$grub_platform" = "pc" ]; then
        insmod efiemu
        efiemu_loadcore /boot/grub/i386-pc/efiemu64.o
        efiemu_prepare
    fi
    xnu_kernel64 /System/Library/Kernels/kernel -v rd=disk0s3 serial=1 debug=0x1 keepsyms=1 kextlog=0xffffffff
    xnu_kext /System/Library/Extensions/IOACPIFamily.kext
    xnu_kext /System/Library/Extensions/IOPCIFamily.kext
    xnu_kext /System/Library/Extensions/IOHIDFamily.kext
    xnu_kext /System/Library/Extensions/IOHIDSystem.kext
    xnu_kext /System/Library/Extensions/ApplePS2Controller.kext
    xnu_kext /System/Library/Extensions/IOStorageFamily.kext
    xnu_kext /System/Library/Extensions/IOATAFamily.kext
    xnu_kext /System/Library/Extensions/AppleIntelPIIXATA.kext
    xnu_kext /System/Library/Extensions/RavynAHCIPort.kext
    xnu_kext /System/Library/Extensions/AppleFileSystemDriver.kext
    xnu_kext /System/Library/Extensions/IOGOPFramebuffer.kext
    xnu_kext /System/Library/Extensions/corecrypto.kext
    xnu_kext /System/Library/Extensions/pthread.kext
    xnu_kext /System/Library/Extensions/hfs_encodings.kext
    xnu_kext /System/Library/Extensions/hfs.kext
    xnu_kext /System/Library/Extensions/msdosfs.kext
    xnu_kextdir /System/Library/Extensions
    boot
}

menuentry "darwinOS (GUI Desktop - WindowServer)" {
    insmod all_video
    insmod macho
    insmod xnu
    if [ "$grub_platform" = "pc" ]; then
        insmod efiemu
        efiemu_loadcore /boot/grub/i386-pc/efiemu64.o
        efiemu_prepare
    fi
    xnu_kernel64 /System/Library/Kernels/kernel
    xnu_kextdir /System/Library/Extensions
    boot
}

menuentry "darwinOS (Single User Safe Mode)" {
    insmod all_video
    insmod macho
    insmod xnu
    if [ "$grub_platform" = "pc" ]; then
        insmod efiemu
        efiemu_loadcore /boot/grub/i386-pc/efiemu64.o
        efiemu_prepare
    fi
    xnu_kernel64 /System/Library/Kernels/kernel -v -s -x serial=1 debug=0x1 keepsyms=1
    xnu_kextdir /System/Library/Extensions
    boot
}
EOF

# 6. Generate Hybrid Dual-Bootable ISO
log_info "Generating Dual-Boot Hybrid ISO..."
rm -f "$OUTPUT_ISO"

if command -v grub-mkrescue >/dev/null 2>&1; then
    log_info "Using grub-mkrescue for full BIOS + UEFI compatibility..."
    grub-mkrescue \
        -o "$OUTPUT_ISO" \
        "$ISO_STAGING" \
        -- -volid "DARWINOS" -hfsplus on
else
    log_warn "grub-mkrescue not found, using direct xorriso..."
    xorriso -as mkisofs \
        -iso-level 3 \
        -full-iso9660-filenames \
        -volid "DARWINOS" \
        -hfsplus \
        -output "$OUTPUT_ISO" \
        "$ISO_STAGING"
fi

ISO_SIZE=$(du -h "$OUTPUT_ISO" | cut -f1)
log_ok "Bootable ISO successfully generated: $OUTPUT_ISO ($ISO_SIZE)"
echo ""
echo -e "${BOLD}${GREEN}=== ISO Build Complete! ===${RESET}"
echo "You can test it directly in QEMU via:"
echo "  ./build.sh run"
