#!/bin/sh
set -euo pipefail

# Helper: create a tarball for makepkg excluding Windows-specific files,
# then run makepkg -si. This script lives in pkgs/arch and will create the
# source tarball inside pkgs/arch so the repository root stays untouched.

script_dir=$(cd "$(dirname "$0")" && pwd)
pkgdir="$script_dir"

if [ ! -f "$pkgdir/PKGBUILD" ]; then
  echo "PKGBUILD not found in $pkgdir"
  exit 1
fi

pkgname=$(grep '^pkgname=' "$pkgdir/PKGBUILD" | head -n1 | cut -d= -f2 | tr -d '"')
pkgver=$(grep '^pkgver=' "$pkgdir/PKGBUILD" | head -n1 | cut -d= -f2 | tr -d '"')
tarball="${pkgname}-${pkgver}.tar.gz"


echo "Creating ${tarball} in ${pkgdir} from contents of ${pkgdir} (excluding PKGBUILD and helper)..."
tar -C "$pkgdir" \
  --exclude='./PKGBUILD' \
  --exclude='./build_pkg.sh' \
  --exclude='*windows*' \
  -czf "$pkgdir/$tarball" --transform "s,^,${pkgname}-${pkgver}/," .

echo "Changing to $pkgdir and building package (no automatic install)"
cd "$pkgdir"

# Build only — avoid makepkg's automatic install step so we can control privilege tool
makepkg -f

# Find built packages
pkgfiles=$(ls ./*.pkg.tar.* 2>/dev/null || true)
if [ -z "$pkgfiles" ]; then
  echo "No package files were created. Build may have failed."
  exit 1
fi

# Choose installer: env PKG_INSTALLER overrides; otherwise prefer doas, then sudo
if [ -n "${PKG_INSTALLER-}" ]; then
  installer="$PKG_INSTALLER"
else
  if command -v doas >/dev/null 2>&1; then
    installer=doas
  elif command -v sudo >/dev/null 2>&1; then
    installer=sudo
  else
    installer=""
  fi
fi

if [ -z "$installer" ]; then
  echo "No privilege escalation tool found (doas or sudo). Built packages are:"
  echo "$pkgfiles"
  echo "Install manually with: pacman -U <pkgfile>"
  exit 0
fi

echo "Installing packages using $installer"
case "$installer" in
  doas|sudo)
    $installer pacman -U --noconfirm $pkgfiles || {
      echo "Installer failed; you can install manually: pacman -U $pkgfiles"
      exit 1
    }
    ;;
  *)
    echo "Unknown installer: $installer"
    exit 1
    ;;
esac

echo "Done. Package files are in $pkgdir."
