[sgcl](../README.md) › compress

# sgcl::compress

```cpp
#include "sgcl/compress.h"   // namespace sgcl::compress
```

Compression and archives: what Go has in `compress/flate`, `compress/gzip`, `compress/zlib`, `compress/bzip2`,
`compress/lzw`, `archive/zip` and `archive/tar`, XZ Utils' `.lzma` and `.xz` (LZMA, LZMA2 and their filters), and
7-Zip's `.7z`. The module depends on [core](../core/README.md), [io](../io/README.md) (the streams, the files),
[async](../async/README.md) (the task forms, through io), [hash](../hash/README.md) (CRC-32, CRC-64 and Adler-32),
[crypto](../crypto/README.md) (SHA-256, the check of xz; AES-256, SHA-256 and random salts and IVs for 7z's
passwords) and [time](../time/README.md) (the times in headers and archives); [net](../net/README.md) is to take gzip
from it for HTTP's `Content-Encoding`. The index of the whole interface is [the modules](../README.md).

A format of one stream ([flate](flate.md), [zlib](zlib.md), [gzip](gzip.md), [bzip2](bzip2.md), [lzw](lzw.md),
[lzma](lzma.md), [xz](xz.md)) is one class with one shape: the whole of the data in memory either way, `compress`
and `decompress`, and a stream each way, a `writer` that compresses what is written to it into another
[io writer](../io/writer.md) and a `reader` of what the data read from another io reader decompresses to. The
archives ([zip](zip.md), [tar](tar.md), [sevenzip](sevenzip.md)) are namespaces with an archive to read, a writer,
and `extract` and `create`, what `unzip`, `tar -x` and `7zz x` do to a directory, with every name checked before
anything is written. Data from outside is bounded by [limits](limits.md), and every failure is a
[compress::error](error.md) that says what and at which byte.

Every algorithm is written from its specification (RFC 1950, 1951 and 1952, PKWARE's APPNOTE 6.3.10, POSIX pax, the
LZMA SDK's lzma-specification.txt and 7zFormat.txt, xz-file-format 1.2) or, where there is none (PPMd var. H, BCJ2,
7zAES, Deflate64), from the LZMA SDK's Methods.txt and the formats' published descriptions; zlib, libbz2, liblzma,
7-Zip (`7zz`), libarchive (`bsdtar`), Python and Go are the oracles of the tests and nothing more.

## The rules

1. **compress never fails, decompress says why.** `compress(data)` takes bytes (a
   [slice](../core/slice.md)`<const byte>`) or text (a [string](../core/string.md), a literal) and returns the
   compressed bytes; an option the format cannot hold is the program's mistake, `std::invalid_argument`.
   `decompress(data)` returns an [expected](../core/expected.md)`<vector<byte>, compress::error>`: corrupt data, a
   checksum that does not match, data cut short, output past the [limits](limits.md).
2. **A writer and a reader are io streams.** Both have the blocking forms and the task's forms (`async_write`,
   `async_read`...) and every form of an io stream (`read_all_text`, `copy_to`, an `io::buffered_reader` over a
   reader for lines). A writer's `close()` ends the data and leaves its output open (a zip archive or an HTTP body
   goes on after it); a reader's `close()` closes its source. A writer keeps its first error, so a stream is written
   freely and checked once, at the close; a reader's `last_error()` holds the whole [error](error.md) of the data.
   The writers and readers of flate, zlib, gzip, lzma and xz have `reset(stream)`, a new stream with the memory
   they have.
3. **Work and waiting.** Compressing is work for the processor, not a wait: the task's forms do it on the worker
   and let it go every 64 KB (a yield), writing and reading alike, so that a large stream does not starve the other
   tasks; only the stream's own reads and writes wait.
4. **Memory of the task's forms.** A reader's input and a writer's output are plain memory while a thread reads or
   writes. A task's read or write of the stream underneath may run on the
   [blocking pool](../async/spawn_blocking.md) (a stream with no async operation of its own, a file read at an
   offset), so it is given managed memory whose slice holds it, as in [io](../io/README.md): a reader's first task
   read moves its input into a managed block once (16 KB; bzip2's and tar's 32 KB), and a writer copies what it
   writes out into a managed block it keeps (1, 8 or 32 KB, the smallest that holds the write, larger when one write
   needs more). A reader only ever read by tasks makes no plain input at all.
5. **What follows the data.** `flate` and `zlib` stop at the end of their stream and take nothing after it, as zlib
   and Go do; `gzip`, `bzip2` and `xz` take what follows as the next member or stream, as gunzip, bunzip2 and xz
   do (gzip's unless told `gzip::single_member`). A reader reads its input a block at a time, so it may take bytes
   past the end from its source: a format that keeps other data after the stream gives the reader only its part
   ([io::limit_reader](../io/limit_reader.md)).
6. **Data from outside is bounded.** Every `decompress` of data in memory, and `zip::archive::read` and
   `sevenzip::archive::read`, stops at the [limits](limits.md)' `max_size` (1 GiB unless told otherwise) with
   `errc::too_large`; `max_memory` bounds what a header makes a decoder allocate, in memory and in a stream alike.
   A stream has no `max_size`: its reader decides how much it reads. The archives' `extract` checks every name
   before anything is written: an entry, or a link's target, that would leave the directory is
   `errc::insecure_path`, and nothing is written (a 7z link's target is checked when the link is reached); so is
   a total past the `max_size` of its options.
7. **Errors.** [compress::error](error.md) is the error of every format, one type under each format's name
   (`gzip::error`, `zip::error`): the code, the byte of the compressed input where it was found, the stream's own
   error when one failed underneath, and `message()`. Read through an `io::reader`, a format's failure is an
   `io::error` of the [compress category](compress_category.md). Out of memory is never an error of the module
   ([collector](../core/collector.md#the-memory-limit)).
8. **Files.** What works on files by name (`gzip::compress_file`, `extract`, `create`) has an `async_` form that
   runs the work on the [blocking pool](../async/spawn_blocking.md), and leaves nothing half made behind a failure.

## Functions

| Function | Header | Description |
|---|---|---|
| [compress_category](compress_category.md) | `error.h` | the error category of the module's codes, `"compress"` |
| [make_error_code](make_error_code.md) | `error.h` | a code of the module as an `error_code` |
| [sevenzip::create, async_create](sevenzip-create.md) | `sevenzip.h` | a directory into a `.7z` |
| [sevenzip::extract, async_extract](sevenzip-extract.md) | `sevenzip.h` | a `.7z` into a directory, every name checked first |
| [tar::create, async_create](tar-create.md) | `tar.h` | a directory into a `.tar`, `.tar.gz` or `.tar.xz` |
| [tar::extract, async_extract](tar-extract.md) | `tar.h` | a `.tar` (gzip, xz or bzip2 around it) into a directory, every name checked first |
| [zip::create, async_create](zip-create.md) | `zip.h` | a directory into a `.zip` |
| [zip::extract, async_extract](zip-extract.md) | `zip.h` | a `.zip` into a directory, every name checked first |

## Classes

| Class | Header | Description |
|---|---|---|
| [bzip2](bzip2.md) | `bzip2.h` | bzip2 1.0, read: `.bz2`, `.tar.bz2`; inside 7z |
| [error](error.md) | `error.h` | what went wrong in compressed data or an archive, and at which byte |
| [flate](flate.md) | `flate.h` | DEFLATE, RFC 1951, both ways: inside zip, PNG and gzip; HTTP's `deflate` as browsers read it |
| [gzip](gzip.md) | `gzip.h` | RFC 1952, both ways: DEFLATE, CRC-32, a name, a time, several members; `.gz`, HTTP's `Content-Encoding: gzip` |
| [gzip_header](gzip_header.md) | `gzip.h` | the header of a gzip member: a name, a comment, a time, an extra field |
| [level](level.md) | `level.h` | how hard a compressor works: 0 to 9, `huffman_only` |
| [limits](limits.md) | `limits.h` | the bounds on what data from outside makes: the output, a decoder's memory, a 7z header's entries |
| [lzma](lzma.md) | `lzma.h` | LZMA alone, both ways: the range coder, an optimal parser, xz's levels 0 to 9 and `-e`; `.lzma` |
| [lzw](lzw.md) | `lzw.h` | LZW, both ways, bits least or most significant first: GIF, TIFF, PDF |
| [xz](xz.md) | `xz.h` | xz-file-format 1.2, both ways: LZMA2, the branch converters and Delta, CRC-32, CRC-64 or SHA-256; `.xz`, `.tar.xz` |
| [zlib](zlib.md) | `zlib.h` | RFC 1950, both ways: DEFLATE, Adler-32, a preset dictionary; PNG's image data, PDF |

## Namespaces

| Namespace | Header | Description |
|---|---|---|
| [sevenzip](sevenzip.md) | `sevenzip.h` | 7z, both ways: LZMA, LZMA2, PPMd, BZip2 (read), Deflate, Deflate64 (read), Copy; BCJ, BCJ2 and the other converters, Delta; solid; passwords (7zAES) |
| [tar](tar.md) | `tar.h` | ustar, pax and GNU's long names, both ways: `.tar`, with gzip `.tar.gz`, with xz `.tar.xz` |
| [zip](zip.md) | `zip.h` | APPNOTE 6.3.10, both ways: stored and deflated entries (Deflate64 read), ZIP64; `.zip`, `.jar`, `.docx` |

## Enumerations

| Enumeration | Header | Description |
|---|---|---|
| [errc](errc.md) | `error.h` | the codes of the module's errors |

## See also

- [Benchmarks](benchmarks.md): DEFLATE against zlib and Go, LZMA and xz against liblzma, bzip2 against libbz2
- [io](../io/README.md): the streams the readers and writers are; [hash](../hash/README.md): the checksums
- `tests/compress`: every format against its oracle both ways, every archive of Go's test data read as Go reads it;
  `tests/compress/fuzz`: the harnesses (libFuzzer's ABI, with a driver of its own where libFuzzer is missing)
- [The modules](../README.md)
