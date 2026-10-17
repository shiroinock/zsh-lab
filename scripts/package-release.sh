#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
version=$(cat VERSION)
printf '%s\n' "$version" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$' || { echo 'Invalid VERSION' >&2; exit 1; }
if [ -n "${RELEASE_TAG:-}" ] && [ "$RELEASE_TAG" != "v$version" ]; then
  echo 'Release tag must match VERSION' >&2
  exit 1
fi
make release tests/smoke-ui
[ "$(./zsh-lab --version)" = "zsh-lab $version" ]
[ "$(lipo -archs zsh-lab)" = "arm64" ] || { echo 'Expected arm64 binary' >&2; exit 1; }
codesign --verify --strict zsh-lab
# Permit only the macOS system libraries in each architecture.
otool -L zsh-lab | awk '/compatibility version/ {if ($1 != "/usr/lib/libSystem.B.dylib" && $1 != "/usr/lib/libncurses.5.4.dylib") exit 1}'
mkdir -p dist
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT HUP INT TERM
name="zsh-lab-$version-macos-arm64"
mkdir "$stage/$name"
cp zsh-lab Start.command README.md LICENSE VERSION "$stage/$name/"
archive="$PWD/dist/$name.zip"
# Always create a new archive; never retain obsolete entries from a prior build.
rm -f "$archive"
(cd "$stage" && /usr/bin/zip -qr "$archive" "$name")
mkdir "$stage/verify"
/usr/bin/unzip -q "$archive" -d "$stage/verify"
extracted="$stage/verify/$name/zsh-lab"
[ -x "$extracted" ]
[ "$("$extracted" --version)" = "zsh-lab $version" ]
codesign --verify --strict "$extracted"
./tests/smoke-ui "$extracted"
(cd dist && shasum -a 256 "$name.zip" > SHA256SUMS.txt && shasum -a 256 -c SHA256SUMS.txt)
printf '\nRelease package: %s\n' "$archive"
