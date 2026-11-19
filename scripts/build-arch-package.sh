#!/usr/bin/env bash
set -euo pipefail
if (( EUID == 0 )); then
    echo 'Run as a regular user; makepkg does not build as root.' >&2
    exit 1
fi
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
version=${PACKAGE_VERSION-0.1.0}
# Arch pkgver cannot contain hyphens or shell metacharacters.
[[ "$version" =~ ^[0-9][a-zA-Z0-9._]*$ ]] || { echo 'Invalid package version' >&2; exit 1; }
mkdir -p "$root/work"
# Each invocation has an isolated source tree; never package ignored/private files.
stage=$(mktemp -d "$root/work/arch-package.XXXXXXXX")
source_dir="$stage/kde-synth-$version"
mkdir -p "$source_dir" "$root/bin/packages"
for entry in CMakeLists.txt go.mod README.md THIRD_PARTY.md src audio tests assets packaging scripts docs; do
    cp -a "$root/$entry" "$source_dir/"
done
if [[ ! -f "$root/LICENSE" ]]; then
    echo 'Choose a project license and add LICENSE before creating a distributable package.' >&2
    exit 1
fi
cp "$root/LICENSE" "$source_dir/"
license=$(cat "$root/packaging/arch/license-id")
tar -czf "$stage/kde-synth-$version.tar.gz" -C "$stage" "kde-synth-$version"
checksum=$(sha256sum "$stage/kde-synth-$version.tar.gz" | cut -d ' ' -f 1)
project_url=${PROJECT_URL:-https://github.com/dev-idkwhoami/kde-synth}
# Restrict substituted metadata to characters safe in the shell PKGBUILD.
[[ "$project_url" =~ ^https://[a-zA-Z0-9./_-]+$ ]] || { echo 'Invalid PROJECT_URL' >&2; exit 1; }
[[ "$license" =~ ^[a-zA-Z0-9.-]+$ ]] || { echo 'Invalid license ID' >&2; exit 1; }
sed -e "s|@PROJECT_URL@|$project_url|g" -e "s|@LICENSE@|$license|g" \
    -e "s|@PACKAGE_VERSION@|$version|g" -e "s|@SOURCE_SHA256@|$checksum|g" "$root/packaging/arch/PKGBUILD.in" > "$stage/PKGBUILD"
(cd "$stage" && PKGDEST="$root/bin/packages" makepkg --cleanbuild --clean "$@")
(cd "$root/bin/packages" && sha256sum kde-synth-"$version"-1-*.pkg.tar.zst > SHA256SUMS)
printf 'Packages and checksums: %s/bin/packages\n' "$root"
