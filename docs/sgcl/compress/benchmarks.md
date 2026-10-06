[sgcl](../README.md) › [compress](README.md)

# Benchmarks: compress

On an Apple M-series machine, release builds; MB/s of the uncompressed side, and the compressed size against the
original where a case compresses. DEFLATE against zlib and Go, LZMA and xz against liblzma, bzip2 against libbz2, LZ4
against liblz4, zstd against libzstd, brotli against libbrotli.
The SGCL cells of the words corpus, of the decoders and of random bytes are from a run on 4 October 2026 at `-O3`, in a
clean environment (`env -i`; the shell of the earlier runs set `MallocNanoZone=0`, which turns macOS's nano allocator off);
the md8 rows, the rows over the library's headers and over a program, and the columns of zlib, liblzma and Go are
from the earlier run, not run again (md8 and the program are not in the tree, and the headers have grown from
8.7 to 11.6 MB since, which would not be the bytes of liblzma's column).

## DEFLATE

Compression over two corpora: **md8** is 8 MB of this repository's Markdown (every `*.md`, repeated past the window:
text as people write it), **words** is `benchmarks/compress`'s 8 MB of words drawn from a list of 64 (the same bytes
in `benchmarks/go/compress`), text far more repetitive than any written by hand, where the chains have the most to
walk:

| Case | SGCL | `zlib` (C, the system's 1.2.12) | Go 1.27 |
|---|---|---|---|
| compress md8, level 1 | 139 (0.402) | 123 (0.401) | 183 (0.400) |
| compress md8, level 6 (default) | 95 (0.342) | 36 (0.332) | 104 (0.345) |
| compress md8, level 9 | 25.7 (0.331) | 28.1 (0.331) | — |
| compress words, level 1 | 207 (0.339) | 138 (0.339) | 262 (0.327) |
| compress words, level 6 (default) | 135 (0.282) | 21 (0.265) | 148 (0.280) |
| compress words, level 7 | 14.7 (0.262) | 17.3 (0.263) | — |
| compress words, level 9 | 11.9 (0.262) | 14.3 (0.262) | 12.1 (0.263) |
| decompress in memory | 1312 | 1574 | 270 |
| gzip read as a stream, 64 KB a read | 1074 | 1279 | 249 |
| a zip archive of 10 000 entries opened | 0.46 ms | — | 1.27 ms |

The default level is Go's kind of encoder ([level](level/README.md)), as fast as Go's default within some 10 % and no
larger; zlib's level 6 is a chain walk three to seven times slower for 3 % less. On text the default is some 3 %
larger than zlib's 6 and three times as fast; on binary data (object files) 7 % larger at twice the speed, so an
archive of binaries that should be small asks for 7 to 9. Levels 7 to 9 are zlib's chains, 0.8 to 0.9 of zlib's
speed at zlib's size (the cost of a step of the chain is the next work, with SIMD). The decoder is within 1.2× of
Apple's zlib, which is hand-tuned, and five times Go's.

## LZMA

`benchmarks/compress/lzma.cpp`, the median of five processes of two seconds each, against liblzma 5.8.4 (xz's
library, `lzma_alone_decoder` and `lzma_alone_encoder` at the same preset); the data in memory both ways, into a
buffer made by the call:

| Case | SGCL | `liblzma` |
|---|---|---|
| decompress text (the library's headers, 8.7 MB) | 207 | 204 |
| decompress a program (a test binary, 32 MB) | 648 | 655 |
| decompress random bytes (4 MB, literals only) | 31.9 | 32.5 |
| compress text, level 0 | 65.0 (16.51%) | 57.1 (17.19%) |
| compress text, level 6 | 4.26 (12.537%) | 4.39 (12.522%) |
| compress text, level 9 | 4.18 (12.537%) | 4.36 (12.522%) |

The decoder is level with liblzma; its fast loop runs while a symbol's worth of input (32 bytes) and a match's worth
of room are at hand, and the last bytes of a stream fed in pieces are decoded dry first to see whether a whole symbol
is there. The encoder's sizes are within 0.2 % of liblzma's (on the program 0.2 % smaller at levels 6 and 9); level 0
hashes four bytes where xz's hashes three, which on text is 4 % smaller and 12 % faster.

Against gzip: on the library's own headers (8.7 MB of C++) LZMA makes a third less than gzip at level 9 (1.09 MB
against 1.60), for about three times gzip's work to compress and a sixth of its speed to decompress.

## xz

The same benchmark, against liblzma 5.8.4 (`lzma_stream_decoder` and `lzma_easy_encoder`, CRC-64):

| Case | SGCL | `liblzma` |
|---|---|---|
| decompress text (the library's headers, 8.8 MB) | 224 | 183 |
| decompress a program (a test binary, 32 MB) | 747 | 446 |
| decompress random bytes (4 MB, stored chunks) | 22 500 | 1 009 |
| compress text, level 0 | 66.1 (16.54%) | 55.7 (17.22%) |
| compress text, level 6 | 5.19 (12.554%) | 5.40 (12.529%) |
| compress text, level 9 | 5.14 (12.553%) | 5.33 (12.529%) |

The LZMA2 decoder is the one of [lzma](lzma/README.md), level with liblzma's; the difference here is the check: CRC-64 on
the processor's carry-less multiply (`hash::crc64`) against liblzma's tables, which shows most over stored chunks.
The branch converters run at 2.1–9 GB/s over a 32 MB program (x86 2.1–2.4, ARM64 5.4, ARM-Thumb 4.0, RISC-V 4.4,
IA-64 6.5, PowerPC 7.5, SPARC 8.7, ARM 9.1; Delta 2.1 encoding, 1.4 decoding), each a single pass over the bytes.

## bzip2

The decoder is faster than libbz2 on the same data: 0.66–0.93× its time on e.txt, Newton's Opticks and random data
(measured against `BZ2_bzBuffToBuffDecompress` when the module was written).

`benchmarks/compress/bzip2.cpp` puts the compressor beside libbz2's (`BZ2_bzBuffToBuffCompress`, the default work
factor) at levels 1 and 9 (`encode-1`, `encode-9`) over 8 MB of the library's headers, and the decoder beside
`BZ2_bzBuffToBuffDecompress` on libbz2's stream (`decode-9`). The transform sorts each block's rotations as the
suffixes of the block turned to its least rotation (a Lyndon word, whose rotations sort as its suffixes), by SA-IS in
linear time; a periodic block is sorted written twice. The numbers wait for the quiet machine
(`CASES=compress benchmarks/compare.sh`).

## LZ4

`benchmarks/compress/lz4.cpp` against liblz4 1.10 (`LZ4F_compressFrame`, `LZ4F_decompress`, `LZ4_decompress_safe`):
the library's headers as one frame of 4 MB blocks with the content's checksum, made by liblz4 at level 1, decompressed
whole into a buffer made by the call (`decode-text`, `decode-binary` over a program), as one raw block
(`decode-raw`), through the reader 64 KB a read (`stream-decode`), and compressed into a frame at levels 1, 3, 9 and
12 and with acceleration 8 (`encode-1` … `encode-f8`); Go's standard library has no LZ4. The numbers wait for the
quiet machine (`CASES=compress benchmarks/compare.sh`).

The decoder copies a short run of literals as 16 bytes and a short match as 18 with one test of the token and of the
two ends, and a long match 32 bytes a step; near the ends a careful loop takes over. Level 1 is the method of liblz4's
default and writes the same bytes; 2 probes two tables; 3 to 9 walk the chains with two matches looked ahead; 10 to
12 are the optimal parser, the chains walked along the four bytes of the best match whose link reaches farthest.

## Snappy

`benchmarks/compress/snappy.cpp`: the library's headers as one block and in the framing format, both ways, and a
program decoded as a block. No Snappy library is on the machine the module was written on and Go's standard library
has none, so the other side is LZ4's fast level, liblz4 where the build found it, for orientation only: two formats of
the same kind, not one format against itself. The numbers wait for the quiet machine.

## zstd

`benchmarks/compress/zstd.cpp` against libzstd 1.5.7 (`ZSTD_compress2` with the content's checksum on, as the `zstd`
command writes, `ZSTD_decompressDCtx`, `ZSTD_decompressStream`): 8 MB of the library's headers as one frame made by
libzstd at levels 1, 3, 9 and 19, decompressed whole into a buffer made by the call (`decode-1` … `decode-19`,
`decode-binary` over a program), through the reader 64 KB a read (`stream-decode`), and compressed into a frame at
levels 1, 3, 6, 9, 12, 16 and 19 and at `--fast=5` (`encode-1` … `encode-f5`); Go's standard library has no zstd. The
numbers wait for the quiet machine (`CASES=compress benchmarks/compare.sh`).

The decoder reads a sequence's bits with one refill when they fit in the 56 a refill leaves (they mostly do), the
three states' bits and the two lengths' extra bits each in one read, the next states made before the copies, a
sequence's literals and match moved 32 bytes whatever their lengths (the source of the match fetched ahead), and the
literals of four Huffman streams side by side without tests while every stream has 8 bytes left. Levels 1 and 2 probe two positions a step; 3 and 4 a long
table and a short one; 5 to 12 rows of 16 to 64 positions with an 8-bit tag of the hash beside each, compared eight at
a time, the row of a position fetched eight positions ahead and the candidates' bytes before they are compared; 13 to
15 the lazy parse over a binary tree; 16 to 22 the optimal parser over the tree, a match long enough to take at once
taken at its longest. The negative levels leave the literals raw, as libzstd does.

## brotli

`benchmarks/compress/brotli.cpp` against libbrotli 1.2.0 (`BrotliEncoderCompress`, `BrotliDecoderDecompress`,
`BrotliDecoderDecompressStream`): 8 MB of the library's headers as one stream made by libbrotli at qualities 1, 5 and 11,
decompressed whole into a buffer made by the call (`decode-1` … `decode-11`), through the reader 64 KB a read
(`stream-decode`), and compressed at qualities 0, 1, 2, 4, 5, 7, 9 and 11 (`encode-0` … `encode-11`); Go's standard
library has no brotli. The numbers wait for the quiet machine (`CASES=compress benchmarks/compare.sh`).

The decoder reads a command from a saved point only near the end of its input, decodes runs of literals of one block
without tests while 32 bytes of input remain, and skips the context of a block type whose 64 contexts share one tree.
The encoder takes its matches from zstd's finders, the level for each quality the one whose output comes nearest
libbrotli's size; from quality 2 it looks for the static dictionary's words where a word of the data begins, from 5
it codes the literals by the context of the two bytes before them, and at 10 and 11 it writes a meta-block of the
optimal parser and one of a lazy parse and keeps the smaller.

## See also

- [level](level/README.md): the encoders behind DEFLATE's levels
- [lzma::options](lzma-options.md): the levels of LZMA and xz
- [sgcl::compress](README.md)
