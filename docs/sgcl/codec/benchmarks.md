[sgcl](../README.md) › [codec](README.md)

# Benchmarks: codec

The setup, the machine and how the timers are read are described with [the benchmarks of the engine](../../garbage_collector/benchmarks.md). The module's cases are in `benchmarks/codec/`. `bench_codec <case> <sgcl|c>` has the module's side and the C libraries' side: libpng 1.6.58, libjpeg-turbo 3.2.0, libwebp 1.6.0 and giflib 6.1.3, Homebrew's, linked by the benchmark only. Go's side is `benchmarks/go/codec` (Go 1.27.1: `image/png`, `image/jpeg`, `image/gif`, `golang.org/x/image/webp`). `benchmarks/codec/run.sh` makes the inputs, runs every case and prints the table below.

**The inputs**, two images of 2400 × 2400 pixels:

- *sonoma*: the system's wallpaper `Sonoma.heic` (macOS), scaled down. Smooth gradients: little detail, long runs, blocks of few coefficients.
- *detail*: libjpeg-turbo's `testorig` (the tests' seed) tiled with mirroring. A photo's detail everywhere.

Each image is saved as:

- PNG in `rgb8`, `rgba8`, `rgb16` and `rgba16`, written by the module's encoder (adaptive filters, level 7);
- PNG in `rgb8` and `rgba8` with every row filtered by Paeth, and one Adam7-interlaced;
- JPEG at quality 90 with 4:2:0 chroma, baseline (the module's encoder, which writes cjpeg's bytes) and progressive (`jpegtran -progressive`);
- WebP by `cwebp -q 90` and `cwebp -lossless`;
- GIF of 256 colors (Go's encoder).

**The cases.** Decoding each file to its pixels: libpng through `png_read_row`, with 16-bit samples swapped to the machine's order as the module gives them; libjpeg-turbo's `tj3` API with the accurate integer DCT and fancy upsampling, the module's own settings; libwebp's `WebPDecodeRGBInto`; giflib's `DGifSlurp`, the first frame expanded to RGBA through its color map. Encoding the `rgb8` image: PNG at each side's default level, and JPEG at quality 90, 4:2:0, with the typical Huffman tables (`jpeg-enc`) and with tables made for the image (`jpeg-enc-opt`, which Go does not have).

**The method.** One case and one side a process, timed for 0.4 s after 0.1 s thrown away. Five rounds, the order of the sides rotated each round. The table gives milliseconds per image: the median, then the smallest and largest of the five.

Apple silicon, 2026-09-29: five rounds, images of 2400 × 2400, the machine quiet (a load of 2 to 4 from the user's own programs, no build or test running); the photo tiled from `tests/codec/fuzz/seeds/jpeg_decode/testorig.jpg`:

| Image | Case | SGCL | C | Go | SGCL / C | SGCL / Go |
|---|---|---|---|---|---|---|
| sonoma | png-rgb8 | 27.55 (27.33–28.30) | 29.36 (29.12–30.32) | 97.06 (94.03–97.57) | 0.94 | 0.28 |
| sonoma | png-rgba8 | 31.97 (31.39–32.38) | 33.46 (33.02–34.12) | 108.36 (104.50–108.83) | 0.96 | 0.30 |
| sonoma | png-rgb16 | 35.99 (35.00–36.42) | 53.32 (52.43–54.49) | 136.31 (133.64–137.41) | 0.68 | 0.26 |
| sonoma | png-rgba16 | 43.05 (42.10–43.50) | 61.63 (60.59–62.51) | 173.44 (167.57–175.34) | 0.70 | 0.25 |
| sonoma | png-paeth-rgb8 | 53.60 (52.57–54.10) | 49.06 (48.62–49.58) | 144.62 (141.40–146.44) | 1.09 | 0.37 |
| sonoma | png-paeth-rgba8 | 57.83 (56.73–58.57) | 53.44 (53.04–53.88) | 158.62 (156.72–159.01) | 1.08 | 0.36 |
| sonoma | png-adam7-rgb8 | 62.49 (61.44–63.37) | 92.23 (91.72–93.27) | 165.48 (163.06–168.90) | 0.68 | 0.38 |
| sonoma | png-enc | 2217.39 (2202.22–2229.10) | 2380.39 (2371.05–2386.34) | 370.47 (366.45–372.75) | 0.93 | 5.99 |
| sonoma | jpeg-base | 9.76 (9.45–9.99) | 6.60 (6.55–6.79) | 28.39 (27.73–28.69) | 1.48 | 0.34 |
| sonoma | jpeg-prog | 19.42 (18.67–19.61) | 19.35 (19.07–19.64) | 48.58 (47.34–49.04) | 1.00 | 0.40 |
| sonoma | jpeg-enc | 10.11 (9.91–10.18) | 8.52 (8.46–8.80) | 71.43 (71.34–71.83) | 1.19 | 0.14 |
| sonoma | jpeg-enc-opt | 19.07 (18.82–19.39) | 16.62 (16.41–16.97) | — | 1.15 | — |
| sonoma | webp-lossy | 16.37 (16.05–17.00) | 14.63 (14.56–15.07) | 58.57 (58.06–58.81) | 1.12 | 0.28 |
| sonoma | webp-lossless | 91.37 (90.57–93.74) | 90.44 (90.14–91.68) | 168.83 (166.97–170.39) | 1.01 | 0.54 |
| sonoma | gif | 21.05 (20.24–21.75) | 36.20 (35.33–36.82) | 30.65 (29.95–31.07) | 0.58 | 0.69 |
| detail | png-rgb8 | 40.87 (40.51–41.30) | 33.15 (32.77–33.51) | 80.46 (79.88–82.25) | 1.23 | 0.51 |
| detail | png-rgba8 | 42.75 (42.29–43.72) | 35.80 (35.59–36.25) | 88.02 (87.69–89.72) | 1.19 | 0.49 |
| detail | png-rgb16 | 50.85 (50.01–51.27) | 71.61 (70.60–72.74) | 146.19 (145.50–148.65) | 0.71 | 0.35 |
| detail | png-rgba16 | 50.56 (49.97–52.29) | 79.52 (79.37–81.08) | 171.88 (171.45–173.03) | 0.64 | 0.29 |
| detail | png-paeth-rgb8 | 42.34 (41.96–43.19) | 34.79 (34.53–35.06) | 83.63 (83.04–85.02) | 1.22 | 0.51 |
| detail | png-paeth-rgba8 | 43.99 (43.66–44.92) | 37.65 (37.53–38.16) | 92.08 (91.49–92.44) | 1.17 | 0.48 |
| detail | png-adam7-rgb8 | 53.50 (52.41–53.96) | 77.28 (76.89–77.80) | 107.77 (106.32–109.34) | 0.69 | 0.50 |
| detail | png-enc | 367.59 (366.91–370.53) | 344.70 (344.43–348.35) | 202.66 (201.95–209.10) | 1.07 | 1.81 |
| detail | jpeg-base | 22.28 (21.92–22.95) | 14.59 (14.49–14.69) | 50.26 (49.07–50.66) | 1.53 | 0.44 |
| detail | jpeg-prog | 45.02 (43.89–45.67) | 38.97 (38.59–39.27) | 88.50 (85.37–89.48) | 1.16 | 0.51 |
| detail | jpeg-enc | 14.53 (14.23–14.80) | 12.23 (12.18–12.41) | 96.63 (94.94–98.02) | 1.19 | 0.15 |
| detail | jpeg-enc-opt | 25.88 (25.28–26.21) | 31.50 (31.47–32.39) | — | 0.82 | — |
| detail | webp-lossy | 70.86 (69.30–71.90) | 59.25 (58.68–59.73) | 154.63 (153.37–160.52) | 1.20 | 0.46 |
| detail | webp-lossless | 62.53 (61.06–63.61) | 42.87 (42.83–43.62) | 90.42 (87.57–92.30) | 1.46 | 0.69 |
| detail | gif | 25.62 (24.39–25.73) | 40.88 (40.62–41.67) | 36.67 (36.36–37.47) | 0.63 | 0.70 |

**Where the module is.** The aim was each case within 1.2 times the C library's time. Within it:

- PNG decoding of the smooth image in every format, and of the photo in `rgba8`, 16 bits and Adam7. Interlaced files and 16 bits decode in two thirds of libpng's time.
- Progressive JPEG decoding.
- JPEG encoding (1.19 on both images), with optimized tables faster than libjpeg-turbo on the photo (0.82).
- Lossy WebP decoding (1.12 and 1.20), lossless WebP decoding of the smooth image, GIF decoding (0.6 of giflib's time) and PNG encoding (0.93 and 1.07 of libpng's, at the module's default level 7 against libpng's 6).

Outside it, each with the step that would close it. Each step is for the user to decide and has not been started:

- **Baseline JPEG decoding, 1.48 and 1.53.** The Huffman decoding and the placing of the coefficients are most of its time. The step: an IDCT for blocks whose coefficients lie in their first four rows and columns, and a coefficient buffer cleared only where it was written (K2b).
- **Lossless WebP decoding of the photo, 1.46.** Most of it is the Select predictor, the one cwebp picks most for photos, and the rest the prefix codes. The step: Select with its sum over the row above computed for the whole row by vectors, only the sum over the left pixel left pixel by pixel (K4a-b). An earlier measurement said 1.62: its photo was tiled from libjpeg-turbo's `testorig.png` rather than the tests' `testorig.jpg`, so cwebp made another file, and the machine was under a load of 10.
- **PNG decoding of the photo in `rgb8`, 1.23, and with every row Paeth, 1.22.** Its three-byte rows are where the unfilter gains least (Paeth on three-byte pixels runs at 0.82 of its plain twin, against 0.54 on eight-byte ones), and the photo's rows inflate from a less compressible stream. The step: a profile of these two files, then Paeth and Average on three-byte pixels two at a time in one register, and the inflate's copies if the profile points there (K4b).

Go is slower than the module in every case but PNG encoding, where it is 6 times faster on the smooth image and 1.8 times on the photo. Go writes at its default level 6, its fast encoder without hash chains. The module's PNG default is 7, the first of its chain levels, chosen for libpng's file sizes ([compress](../compress/README.md): levels 1 to 6 are its fast encoder too). Faster chain levels are on the list for 1.1.0.

**The size of a PNG.** The files the module's PNG encoder writes come within ±0.3 % of libpng's size at the same level, and at 7, the module's default, a little smaller than at libpng's default.

**What the vector instructions do.** Every step below runs on NEON (Apple silicon, ARMv8.0) and SSE2 (x86-64), the minimum of both, with no flag and no run-time check. Each has a plain twin, with the same output bit for bit, which the tests hold the vector road against and which `SGCL_CODEC_PORTABLE` builds alone:

- JPEG: the islow IDCT and FDCT, the color conversion both ways, the chroma subsampling and upsampling, the quantization by reciprocals, the coefficients' order and masks for the Huffman coder;
- PNG: the unfiltering of Sub, Average and Paeth rows;
- WebP: the loop filters and the 4×4 inverse transform of VP8, TM prediction, YUV to RGB and the chroma upsampling, and the predictors of VP8L.

AVX2 waits for an x86 machine to measure it on.
