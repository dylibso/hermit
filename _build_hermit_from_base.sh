#!/bin/sh

if [ "$#" -ne 2 ]; then
  echo "Usage: $0 srcdir dstdir" >&2
  exit 1
fi

srcdir="$(realpath "$1")"
srcbase="$(basename "$srcdir")"
dstdir="$(realpath "$2")"
zipdir="$dstdir/$srcbase"
zipape="$zipdir/$srcbase.hermit.com"
dstape="$dstdir/$srcbase.hermit.com"

rm -rf "$dstdir/$srcbase"
cp -r "$srcdir" "$dstdir/"
cp "$dstdir/hermit-base.com" "$zipape"
cd "$zipdir"
zip -r "$zipape" main.wasm hermit.json
mv "$zipape" "$dstape"
