#!/bin/zsh
# The codec module against libpng, libjpeg-turbo, libwebp and giflib, and Go,
# on two images of 2400x2400 (docs/sgcl/codec/benchmarks.md):
#   sonoma  the system's wallpaper Sonoma.heic (macOS) scaled down: smooth
#   detail  libjpeg-turbo's testorig (the tests' seed) tiled with mirroring: a photo's detail
# Every input made here (bench_codec prep, jpegtran, cwebp, Go's GIF
# encoder), then five rounds, the sides' order rotated, one case and one
# side a process; the medians and the smallest and largest of the five.
#
#   benchmarks/codec/run.sh [build dir] [--prep-only]   (default: build; --prep-only makes the inputs and stops)
#
# Needs bench_codec built (with the C libraries: SGCL_CODEC_LIBS_ROOT), Go,
# jpegtran and cwebp (Homebrew's jpeg-turbo and webp), and macOS's sips for
# the first image. The Go side is its own module (golang.org/x/image, its
# go.mod written here when missing); to build it without the network,
# GOMODCACHE pointed at a cache that holds golang.org/x/image v0.46.0 and
# GOPROXY=off GOSUMDB=off.
set -e
HERE=${0:A:h}
ROOT=${HERE:h:h}
BUILD=${1:-$ROOT/build}
BUILD=${BUILD:A}   # absolute: the Go side is built from its own directory
BIN=$BUILD/benchmarks
DATA=$BIN/codec-data
BREW=${SGCL_CODEC_LIBS_ROOT:-/opt/homebrew/opt}
[ -x $BIN/bench_codec ] || { echo "no $BIN/bench_codec: build the benchmarks first"; exit 1; }
# the Go side's module file, written when missing (go.mod and go.sum are not
# kept in the tree), golang.org/x/image resolved by go itself
[ -f $ROOT/benchmarks/go/codec/go.mod ] || printf 'module codecbench\n\ngo 1.26\n\nrequire golang.org/x/image v0.46.0\n' > $ROOT/benchmarks/go/codec/go.mod
(cd $ROOT/benchmarks/go/codec && GOFLAGS="${GOFLAGS:--mod=mod}" go build -o $BIN/codec_go .)

mkdir -p $DATA/sonoma $DATA/detail
sips -s format png -Z 2400 "/System/Library/Desktop Pictures/Sonoma.heic" --out $DATA/sonoma/src.png > /dev/null
$BIN/codec_go prep-detail $ROOT/tests/codec/fuzz/seeds/jpeg_decode/testorig.jpg $DATA/detail/src.png
for d in sonoma detail; do
    $BIN/bench_codec prep $DATA/$d > /dev/null
    $BREW/jpeg-turbo/bin/jpegtran -progressive -copy none -outfile $DATA/$d/prog.jpg $DATA/$d/base.jpg
    $BREW/webp/bin/cwebp -quiet -q 90 $DATA/$d/src.png -o $DATA/$d/lossy.webp
    $BREW/webp/bin/cwebp -quiet -lossless $DATA/$d/src.png -o $DATA/$d/lossless.webp
    $BIN/codec_go prep-gif $DATA/$d/src.png $DATA/$d/image.gif
    $BREW/jpeg-xl/bin/cjxl --quiet -d 1 $DATA/$d/src.png $DATA/$d/lossy.jxl
done
if [ "${2:-}" = --prep-only ]; then
    ls -l $DATA/sonoma $DATA/detail
    exit 0
fi

CASES=(png-rgb8 png-rgba8 png-rgb16 png-rgba16 png-paeth-rgb8 png-paeth-rgba8 png-adam7-rgb8 png-enc
       jpeg-base jpeg-prog jpeg-enc jpeg-enc-opt webp-lossy webp-lossless gif gif-enc gif-enc-nodither gif-enc-exact webp-enc webp-enc-lossless
       tiff-dec tiff-enc tiff-enc-deflate bmp-dec bmp-enc qoi-dec qoi-enc jxl-dec qr-enc meta-read)
ORDERS=("sgcl c go" "c go sgcl" "go sgcl c" "sgcl go c" "c sgcl go")
OUT=$(mktemp)
for round in 1 2 3 4 5; do
    for set in sonoma detail; do
        for side in ${=ORDERS[$round]}; do
            for c in $CASES; do
                [ $side = c ] && [ $c = gif-enc ] && continue   # giflib has no dithering
                [ $side = c ] && case $c in bmp-*|qoi-*) true;; *) false;; esac && continue   # no C library of these here
                if [ $side = go ]; then
                    [ $c = jpeg-enc-opt ] && continue
                    [ $c = webp-enc ] || [ $c = webp-enc-lossless ] && continue   # Go has no WebP encoder
                    case $c in tiff-enc|qoi-*|jxl-dec|qr-enc|meta-read) continue;; esac   # Go has no TIFF LZW encoder, no QOI, no JPEG XL, no QR, no EXIF reader
                    line=$($BIN/codec_go $c $DATA/$set)
                else
                    line=$($BIN/bench_codec $c $side $DATA/$set)
                fi
                echo "$side|$set|$line"
            done
        done
    done
done | tee $OUT

echo "# ms per image: the median (the smallest and largest of the five); ratios of the medians, SGCL's time over the other's"
awk -F'|' '
    { key = $2 "|" $3; n[$1, key]++; v[$1, key, n[$1, key]] = $4; if (!(key in seen)) { seen[key] = 1; order[++k] = key } }
    function sorted(s, c, a,    m, i, j, t) {
        m = n[s, c]
        for (i = 1; i <= m; i++) a[i] = v[s, c, i]
        for (i = 1; i <= m; i++) for (j = i + 1; j <= m; j++) if (a[j] + 0 < a[i] + 0) { t = a[i]; a[i] = a[j]; a[j] = t }
        return m
    }
    function cell(s, c,    a, m) {
        m = sorted(s, c, a)
        if (m == 0) return "—"
        return sprintf("%.2f (%.2f–%.2f)", a[int((m + 1) / 2)], a[1], a[m])
    }
    function med(s, c,    a, m) {
        m = sorted(s, c, a)
        return m ? a[int((m + 1) / 2)] : 0
    }
    END {
        print "| image | case | SGCL | C | Go | SGCL / C | SGCL / Go |"
        print "|---|---|---|---|---|---|---|"
        for (i = 1; i <= k; i++) {
            split(order[i], p, "|")
            s = med("sgcl", order[i]); c = med("c", order[i]); g = med("go", order[i])
            printf "| %s | %s | %s | %s | %s | %s | %s |\n", p[1], p[2], cell("sgcl", order[i]), cell("c", order[i]), cell("go", order[i]),
                (c ? sprintf("%.2f", s / c) : "—"), (g ? sprintf("%.2f", s / g) : "—")
        }
    }' $OUT
rm -f $OUT
