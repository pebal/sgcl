# sgcl::compress

Compression and archives: what Go has in `compress/flate`, `compress/gzip`, `compress/zlib`, `compress/bzip2`, `compress/lzw`, `archive/zip` and `archive/tar`, XZ Utils' `.lzma` and `.xz` (LZMA, LZMA2 and their filters), and 7-Zip's `.7z`. `#include "sgcl/compress/compress.h"` brings the module in; it depends on [`core`](../core/README.md), [`io`](../io/README.md) (streams, files), [`async`](../async/README.md) (the task forms, through io), [`hash`](../hash/README.md) (CRC-32, CRC-64 and Adler-32), [`crypto`](../crypto/README.md) (SHA-256, the check of xz; AES-256, SHA-256 and random salts and IVs for 7z's passwords) and [`time`](../time/README.md) (the times in headers and archives), and `net` will take gzip from it for HTTP's `Content-Encoding`. The index of the whole interface is [`docs/sgcl/`](../README.md).

Every algorithm is written from its specification (RFC 1950, 1951 and 1952, PKWARE's APPNOTE 6.3.10, POSIX pax, the LZMA SDK's lzma-specification.txt and 7zFormat.txt, xz-file-format 1.2) or, where there is none (PPMd var. H, BCJ2, 7zAES, Deflate64), from the LZMA SDK's Methods.txt and the formats' published descriptions; zlib, libbz2, liblzma, 7-Zip (`7zz`), libarchive (`bsdtar`), Python and Go are the oracles of the tests and nothing more.

## One shape for every format

A format of one stream is one type with the whole of the data in memory either way, and a stream each way (the archives — [zip](zip.md), [tar](tar.md), [sevenzip](sevenzip.md) — have an archive to read and a writer instead):

```cpp
using namespace sgcl;

auto packed = compress::gzip::compress(text);                 // vector<byte>
auto back = compress::gzip::decompress(packed);               // expected<vector<byte>, compress::error>
if (!back) eprintln("{}", back.error().message());

io::file log = io::create("log.gz");
compress::gzip::writer w(log, {.level = compress::level::smallest});
w.write("a line\n");                                          // any io::writer's forms, async_ included
(void)w.close();                                              // the trailer; the file stays open

compress::gzip::reader r(io::open("log.gz"));
auto all = r.read_all_text();                                 // any io::reader's forms (io::buffered_reader over it for lines)
```

- **`compress(data)`** takes bytes (a [`slice<const byte>`](../core/slice.md)) or text (a [`string`](../core/string.md)) and never fails. **`decompress(data)`** returns [`expected<vector<byte>, compress::error>`](error.md): corrupt data, a checksum that does not match, data cut short.
- **`writer`** is an [io writer](../io/stream.md) that compresses what is written to it into another writer, **`reader`** an io reader of what the data read from another reader decompresses to. Both have the blocking forms and the task's forms (`async_write`, `async_read`...) and the reader's `last_error()`; flate's, zlib's and gzip's have `reset(stream)` to take a new stream with the memory they have; the reader's `close()` closes its source, the writer's `close()` ends the data and leaves its output open (a zip archive or an HTTP body goes on after it).
- **Work and waiting.** Compressing is work for the processor, not a wait: the task's forms do it on the worker and let it go every 64 KB (a yield), writing and reading alike, so that a large stream does not starve the other tasks; only the stream's own reads and writes wait.
- **Memory of the task's forms.** A reader's input and a writer's output are plain memory while a thread reads or writes. A task's read or write of the stream underneath may run on the [blocking pool](../async/blocking.md) (a stream with no async operation of its own, a file read at an offset), so it is given managed memory whose slice holds it, as in [io](../io/README.md): a reader's first task read moves its input into a managed block once (16 KB; bzip2's and tar's 32 KB), and a writer copies what it writes out into a managed block it keeps (1, 8 or 32 KB, the smallest that holds the write, larger when one write needs more). A reader only ever read by tasks makes no plain input at all.
- **What follows the data.** `flate` and `zlib` stop at the end of their stream and take nothing after it, as zlib and Go do; `gzip` takes what follows as the next member (anything else there is `errc::invalid_header`, fewer than a header's ten bytes `errc::unexpected_end`), as Go does, unless told `gzip::single_member`.

## The formats

| type | format | ways | where it is found |
|---|---|---|---|
| [`flate`](flate.md) | DEFLATE, RFC 1951 | both | inside zip, PNG and gzip; HTTP's `deflate` as browsers read it |
| [`zlib`](zlib.md) | RFC 1950: DEFLATE, Adler-32, a preset dictionary | both | PNG's image data, PDF, many protocols |
| [`gzip`](gzip.md) | RFC 1952: DEFLATE, CRC-32, a name, a time, several members | both | `.gz`, `.tar.gz`, HTTP's `Content-Encoding: gzip` |
| [`bzip2`](bzip2.md) | bzip2 1.0 | reading | `.bz2`, `.tar.bz2`; inside 7z |
| [`lzma`](lzma.md) | LZMA alone: the range coder, an optimal parser, xz's levels 0..9 and `-e` | both | `.lzma`; the base of `.xz` and 7z |
| [`xz`](xz.md) | xz-file-format 1.2: LZMA2, the branch converters and Delta, CRC-32/CRC-64/SHA-256 checks, several streams | both | `.xz`, `.tar.xz` |
| [`lzw`](lzw.md) | LZW, bits least or most significant first | both | GIF (the `codec` module to come), PDF |
| [`zip`](zip.md) | APPNOTE 6.3.10: stored and deflated entries (Deflate64 read), ZIP64 | both | `.zip`, `.jar`, `.docx` |
| [`tar`](tar.md) | ustar, pax and GNU's long names | both | `.tar`, with gzip `.tar.gz` |
| [`sevenzip`](sevenzip.md) | 7z: LZMA, LZMA2, PPMd, BZip2 (read), Deflate, Deflate64 (read), Copy; BCJ, BCJ2 and the other converters, Delta; solid; passwords (7zAES) | both | `.7z` |

## level

```cpp
class level {
    static constexpr int store = 0, fastest = 1, standard = 6, smallest = 9, huffman_only = -2;
    constexpr level(int n = standard);   // 0..9 or huffman_only; else invalid_argument (in a constant, a compile error)
    constexpr int value() const noexcept;
};
```

How hard a compressor works, as zlib and Go count it: 0 stores the data in blocks as it is, 1 is the fastest, 9 the smallest, 6 the default everywhere; `huffman_only` codes the bytes without looking for repeats (for data that has none worth the search, such as the residuals of an image filter). An `int` converts to it, so options take `{.level = 9}`.

DEFLATE's levels are two encoders: **levels 1–6 as Go's fast encoder, 7–9 zlib's chains; Go's 6 is our 6.** Levels 1 to 6 look each position up in tables of the latest position under a hash of four bytes (and of seven from level 3), with no chains; the default, 6, is as fast as Go's default and a little smaller. Levels 7 to 9 walk chains of earlier positions under a hash of three bytes with lazy matching, zlib's own 7 to 9: zlib's size, at zlib's pace. On text the default is some 3 % larger than zlib's 6 and three times as fast; on binary data (object files) 7 % larger at twice the speed, so an archive of binaries that should be small asks for 7 to 9.

| level | encoder | hashes | a position looks at | lazy | positions inside a match |
|---|---|---|---|---|---|
| 0 | stored blocks, straight from the input | — | — | — | — |
| 1 | table | 4 bytes, 2^14 | 1 candidate; a run of misses skipped faster | no | every 4th |
| 2 | table | 4 bytes, 2^15 | 1 candidate; misses skipped faster | no | every 2nd |
| 3 | table | 4 bytes 2^15, 7 bytes 2^15 | 2 candidates | no | every 2nd |
| 4 | table | 4 bytes 2^16, 7 bytes 2^15 | 2 candidates | the next position | all |
| 5 | table | 4 bytes 2^16, 7 bytes 2^16 | 2 candidates | the next position | all |
| 6 (default) | table | 4 bytes 2^16, 7 bytes 2^16 (2 a bucket) | 3 candidates | the next position | all |
| 7 | chains | 3 bytes, 2^16 | 256 of the chain (64 past a match of 8) | up to 32 | all |
| 8 | chains | 3 bytes, 2^17 | 1024 (256 past 32) | up to 128 | all |
| 9 | chains | 3 bytes, 2^17 | 4096 (1024 past 32) | up to 258 | all |

From 1 to 9 the output does not grow and the time does not shrink (a test holds the order on text, PNG's filtered rows and binary data). Data an image filter left is compressed at 7 and up with zlib's filtered strategy, which the table levels have no need of: PNG's encoder writes at 7 unless asked.

## limits

```cpp
struct limits {
    uint64_t max_size = 1ull << 30;     // the bytes a decompression in memory may make
    uint64_t max_memory = 1ull << 30;   // the memory a decoder may take because the data asks for it
    uint64_t max_entries = 1000000;     // the entries a 7z header may list
};
```

Data from outside may decompress a thousand times over. Every `decompress` of data in memory, and `zip::archive::read` and `sevenzip::archive::read`, stops at `max_size` bytes (1 GiB unless told otherwise) with `errc::too_large`; `limits{UINT64_MAX}` lifts it. A stream has no limit of its own: its reader decides how much it reads, and [`io::limit_reader`](../io/stream.md) over it bounds what a program takes. `max_memory` bounds what a header makes a decoder allocate — LZMA's and LZMA2's dictionaries, up to 4 GiB by the format, PPMd's model, a 7z folder's decoders together — in memory and in a stream alike: more is `errc::too_large` before anything is taken. `max_entries` bounds the files, folders and streams a 7z header may list, and a 7z key of more than 2^24 rounds is refused the same way.

## Errors

[`compress::error`](error.md) is the error of every format, one type under each format's name (`gzip::error`, `zip::error`): the code, the byte of the compressed input where it was found, the stream's own error when one failed underneath, and `message()`. Read through an `io::reader`, a format's failure is an `io::error` of the `compress` category, and the reader's `last_error()` holds the whole of it.

## Performance

On an Apple M-series machine, MB/s of the uncompressed side and the compressed size against the original. Compression over two corpora: **md8** is 8 MB of this repository's Markdown (every `*.md`, repeated past the window: text as people write it), **words** is `benchmarks/compress`'s 8 MB of words drawn from a list of 64 (the same bytes in `benchmarks/go/compress`), text far more repetitive than any written by hand, where the chains have the most to walk:

| case | sgcl | zlib (C, the system's 1.2.12) | Go 1.27 |
|---|---|---|---|
| compress md8, level 1 | 139 (0.402) | 123 (0.401) | 183 (0.400) |
| compress md8, level 6 (default) | 95 (0.342) | 36 (0.332) | 104 (0.345) |
| compress md8, level 9 | 25.7 (0.331) | 28.1 (0.331) | — |
| compress words, level 1 | 197 (0.339) | 138 (0.339) | 262 (0.327) |
| compress words, level 6 (default) | 124 (0.282) | 21 (0.265) | 148 (0.280) |
| compress words, level 7 | 13.8 (0.262) | 17.3 (0.263) | — |
| compress words, level 9 | 11.2 (0.262) | 14.3 (0.262) | 12.1 (0.263) |
| decompress in memory | 1238 | 1574 | 270 |
| gzip read as a stream, 64 KB a read | 1069 | 1279 | 249 |
| a zip archive of 10 000 entries opened | 0.59 ms | — | 1.27 ms |

These are DEFLATE's; LZMA's and xz's against liblzma are in [lzma](lzma.md#performance) and [xz](xz.md#performance). The default level is Go's kind of encoder, as fast as Go's default within some 10 % and no larger; zlib's level 6 is a chain walk three to six times slower for 3 % less. Levels 7 to 9 are zlib's chains, 0.8 to 0.9 of zlib's speed at zlib's size (the cost of a step of the chain is the next work, with SIMD). The decoder is within 1.3× of Apple's zlib, which is hand-tuned, and four times Go's.

## See also

- [io](../io/README.md): the streams the readers and writers are; [hash](../hash/README.md): the checksums
- `tests/compress`: every format against its oracle both ways, every archive of Go's test data read as Go reads it; `tests/compress/fuzz`: the harnesses (libFuzzer's ABI, with a driver of its own where libFuzzer is missing)
