#!/usr/bin/env bash
# Re-vendor the vjFaLk/esphome-xteink component at a given upstream commit, optionally
# swap its bundled FreeInk SDK for another SDK commit (using upstream's own
# scripts/sync-sdk.sh, same six libraries), then re-apply our local patches from
# patches/xteink-*.patch (in name order).
#   scripts/sync-xteink.sh [upstream-sha] [freeink-sdk-sha]
# Defaults are the pinned values below. Pass "-" as the SDK sha to keep the SDK
# that upstream bundles. After a successful run: build, test on the device,
# update the pins below, commit.
set -euo pipefail
cd "$(dirname "$0")/.."
PINNED=7ea36f1a8be450b865179b43e9eb95ac1b50359b
PINNED_SDK=b964af2a0ee92a90cf4bf51407750de79d7a4c90   # freeink-sdk main, 2026-09-18
SHA="${1:-$PINNED}"
SDK_SHA="${2:-$PINNED_SDK}"
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
git clone -q https://github.com/vjFaLk/esphome-xteink.git "$T/up"
git -C "$T/up" checkout -q "$SHA"
if [ "$SDK_SHA" != "-" ]; then
  "$T/up/scripts/sync-sdk.sh" "$SDK_SHA"
fi
rm -rf esphome/components/xteink
cp -r "$T/up/components/xteink" esphome/components/xteink
cp "$T/up/LICENSE" esphome/components/xteink/LICENSE.upstream
echo "$SHA" > esphome/components/xteink/UPSTREAM_COMMIT
shopt -s nullglob
for p in patches/xteink-*.patch; do
  echo "applying $p"
  git apply --whitespace=nowarn "$p"
done
echo "vendored esphome-xteink @ $SHA, SDK @ $(cat esphome/components/xteink/sdk/SDK_COMMIT), with $(ls patches/xteink-*.patch 2>/dev/null | wc -l) patch(es)"
