#!/bin/bash

set -e

self_dir=$(
  cd "$(dirname "$0")" &>/dev/null
  pwd -P
)

rm -rf "$self_dir/out" "$self_dir/cbrunsli-output.txt"

mkdir "$self_dir/out"

images_dir="stock_snap"

for entry in "$self_dir/$images_dir"/*; do
  # todo: print i/n status
  echo "Processing image: $entry"

  # encoding
  echo "$entry" >>"$self_dir/cbrunsli-output.txt"
  "$self_dir"/../third_party/brunsli/out/artifacts/cbrunsli "$entry" "$self_dir/output.brn"
  pixels=$(identify -format '%w %h' "$entry" | sed 's/$/*p/g' | dc)
  bytes=$(ls -l "$self_dir/output.brn" | cut -d ' ' -f5)
  bpp_form="3 k $bytes 8 * $pixels / p"
  bpp=$(echo "$bpp_form" | dc | sed 's/^\./0./')
  echo "$bytes $bpp" >>"$self_dir/cbrunsli-output.txt"

done
