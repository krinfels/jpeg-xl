#!/bin/bash

set -e

self_dir=$(cd "$(dirname "$0")" &>/dev/null; pwd -P)

rm -rf "$self_dir/out" "$self_dir/cjxl-output.txt" "$self_dir/djxl-output.txt"

mkdir "$self_dir/out"

images_dir="stock_snap"

for entry in "$self_dir/$images_dir"/*
do
  # todo: print i/n status
  echo "Processing image: $entry"

  # encoding
  echo "$entry" >> "$self_dir/cjxl-output.txt"
  "$self_dir"/../cmake-build-debug/tools/cjxl "$entry" "$self_dir/out/output.jxl" \
  2>&1 | grep 'Compressed\|$images_dir' >> "$self_dir/cjxl-output.txt"

  # decoding
  echo "$entry" >> "$self_dir/djxl-output.txt"
  "$self_dir"/../cmake-build-debug/tools/djxl -j "$self_dir/out/output.jxl" "$self_dir/out/output.jpg" 2>> "$self_dir/djxl-output.txt"

  # comparing checksums
  input_checksum=$(sha256sum "$entry" | cut -d " " -f 1 )
  decoded_checksum=$(sha256sum "$self_dir/out/output.jpg" | cut -d " " -f 1 )

  [[ "$input_checksum" == "$decoded_checksum" ]] || {
    echo "Checksum mismatch!"
    echo "Input image:   $input_checksum"
    echo "Decoded image: $decoded_checksum"
    exit 1
  }

done
