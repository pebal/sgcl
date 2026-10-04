[sgcl](../README.md) › [compress](README.md)

# Benchmarks: compress

On an Apple M-series machine, release builds; MB/s of the uncompressed side, and the compressed size against the
original where a case compresses. DEFLATE against zlib and Go, LZMA and xz against liblzma, bzip2 against libbz2.
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
| decompress random bytes (4 MB, stored chunks) | 9 430 | 1 009 |
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

## See also

- [level](level/README.md): the encoders behind DEFLATE's levels
- [lzma::options](lzma-options.md): the levels of LZMA and xz
- [sgcl::compress](README.md)
