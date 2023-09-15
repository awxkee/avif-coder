#!/bin/bash
set -e

bash build_aom.sh
bash build_dav1d.sh
bash build_de265.sh
bash build_x265.sh
bash build_yuv.sh
bash build_sharpyuv.sh
bash build_heif.sh