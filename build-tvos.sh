#!/usr/bin/env bash
#
# Configure bstone for Apple tvOS (device, arm64, tvOS 26 deployment target)
# using the Xcode generator. Produces an Xcode project you open, sign with your
# Team, and Run onto a paired Apple TV.
#
# Usage:
#   ./build-tvos.sh [build-dir]
#
# Optional environment overrides:
#   BSTONE_TVOS_DEVELOPMENT_TEAM=ABCDE12345   # your Apple Developer Team ID
#   BSTONE_TVOS_BUNDLE_ID=org.example.bstone  # bundle identifier
#   BSTONE_TVOS_SYSROOT=appletvsimulator      # build for the Simulator instead
#
set -euo pipefail

cd "$(dirname "$0")"

BUILD_DIR="${1:-build-tvos}"

CMAKE="$(command -v cmake || echo /opt/homebrew/bin/cmake)"

EXTRA_ARGS=()
if [[ -n "${BSTONE_TVOS_DEVELOPMENT_TEAM:-}" ]]; then
	EXTRA_ARGS+=("-DBSTONE_TVOS_DEVELOPMENT_TEAM=${BSTONE_TVOS_DEVELOPMENT_TEAM}")
fi
if [[ -n "${BSTONE_TVOS_BUNDLE_ID:-}" ]]; then
	EXTRA_ARGS+=("-DBSTONE_TVOS_BUNDLE_ID=${BSTONE_TVOS_BUNDLE_ID}")
fi
if [[ -n "${BSTONE_TVOS_SYSROOT:-}" ]]; then
	EXTRA_ARGS+=("-DCMAKE_OSX_SYSROOT=${BSTONE_TVOS_SYSROOT}")
fi

"$CMAKE" -B "$BUILD_DIR" -G Xcode \
	-DCMAKE_TOOLCHAIN_FILE="$(pwd)/cmake/tvos.toolchain.cmake" \
	"${EXTRA_ARGS[@]}"

echo
echo "Configured tvOS project in '$BUILD_DIR'."
echo "Open '$BUILD_DIR/bstone_solution.xcodeproj' in Xcode, select the 'bstone'"
echo "scheme + your Apple TV, set the signing Team, and Run."
echo
echo "Or build from the CLI:"
echo "  $CMAKE --build $BUILD_DIR --config Debug -- -allowProvisioningUpdates"
