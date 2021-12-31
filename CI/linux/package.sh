#!/bin/bash

set -e

script_dir=$(dirname "$0")
source "$script_dir/../ci_includes.generated.sh"

mkdir -p "$script_dir/../../package/usr/lib/obs-plugins/"
mkdir -p "$script_dir/../../package/usr/share/obs/obs-plugins/$PLUGIN_NAME/"

cp "$script_dir/../../build/$PLUGIN_NAME.so" "$script_dir/../../package/usr/lib/obs-plugins/"
cp "$script_dir/../../data/locale/"*.ini "$script_dir/../../package/usr/share/obs/obs-plugins/$PLUGIN_NAME/"

cd "$script_dir/../../package"
rm -f "$PLUGIN_NAME-$PLUGIN_VERSION.zip"
zip -r "$PLUGIN_NAME-$PLUGIN_VERSION.zip" "./usr"
cd ..