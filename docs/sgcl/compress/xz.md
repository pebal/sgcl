# sgcl::compress::xz

```cpp
#include "sgcl/compress/xz.h"   // or "sgcl/compress/compress.h"

namespace sgcl::compress {
    class xz {
    public:
        enum class check : uint8_t { none = 0, crc32 = 1, crc64 = 4, sha256 = 10 };
        enum class filter : uint8_t { x86, arm, armt, arm64, powerpc, sparc, ia64, riscv };
        struct options {
            compress::level level;               // 0..9 as xz -0..-9; 6 unless told otherwise
            bool extreme = false;                // xz -e
            xz::check check = xz::check::crc64;  // xz's default
            optional<xz::filter> bcj;            // a branch converter before LZMA2
            uint16_t delta = 0;                  // Delta's distance, 1..256; 0: none
            uint32_t dictionary = 0;             // 4 KiB .. 1.5 GiB; 0: the level's
        };
        class writer;   // io::writer: what is written, compressed into another writer
        class reader;   // io::reader: what the data read from another reader decompresses to

        static vector<byte> compress(const slice<const byte>& data);          // and (data, options), (text), (text, options)
        static expected<vector<byte>, error> decompress(const slice<const byte>& data);
        static expected<vector<byte>, error> decompress(const slice<const byte>& data, const limits& l);
    };
}
```

The `.xz` format of XZ Utils (`.xz`, `.txz`, `.tar.xz`): [LZMA](lzma.md) in its second form, LZMA2, inside a container that checks what it holds. A stream is a header naming the check, blocks — each a header listing its filters, the compressed data, and the check of the data decompressed — an index of the blocks' sizes, and a footer. The same levels as `compress::lzma` (xz's `-0` to `-9` and `-e`), and the same memory.

**LZMA2** codes the data in chunks of up to 2 MiB, each of LZMA with a range coder of its own or stored as it is when LZMA would not make it smaller (random data grows by 0.01% instead of LZMA's 1.4%).

**Filters** go before LZMA2 and keep the length. A branch converter turns the relative targets of calls and jumps in machine code into absolute ones, so that calls to one function from many places look alike: an ARM64 program of 32 MB compresses 12% smaller with the ARM64 converter (a universal macOS binary 3% with either converter, each fitting half of it) (x86, ARM, ARM-Thumb, ARM64, PowerPC, SPARC, IA-64, RISC-V; the formats' own conversions, bit for bit those of xz). Delta codes each byte as its difference from the byte `delta` before it — uncompressed sound or images whose samples are that many bytes apart. Both at once: the converter first. On reading, every chain xz writes is taken (up to three filters before LZMA2, start offsets of the converters included).

## Rules

- **Checks.** Every block's check is compared at the block's end (`errc::checksum`); the headers, the index and the footer carry CRC-32s of their own, and the index must list exactly the blocks read (`errc::corrupt`). `check::none` is read and written, as xz does: the data is then only as safe as LZMA2's structure makes it. Check types the format reserves but does not define are `errc::unsupported` (xz warns and goes on).
- **Streams.** Streams one after another, with zero bytes between them in fours (stream padding), are read as one, as xz reads them; padding not a multiple of four is `errc::corrupt`, anything else after a stream `errc::invalid_header`. Several blocks in a stream (what `xz -T` writes) are read in order.
- **Writing.** `compress` writes one block with its sizes in its header; a `writer` one block without them, as single-threaded xz does. Empty data is a stream of no block (32 bytes), as xz makes it. Several blocks and several threads are not written in this version.
- **Memory.** A block's dictionary is as large as its header asks, or as the block when the header gives the block's size, and is held against [`limits.max_memory`](README.md#limits) before anything is taken — more is `errc::too_large` at the block's offset, in memory and in a stream alike. `decompress` has `max_size` as well; a block's size in its header past it fails before any work.
- Options out of range (a Delta distance past 256, a check that is not one of the four, a dictionary under 4 KiB or past 1.5 GiB, `level::huffman_only`) are the program's mistake: `compress` throws `std::invalid_argument`, and a writer's first write reports `errc::invalid_argument`.

## Performance

On an Apple M-series machine (`benchmarks/compress/lzma.cpp`, the median of five processes of two seconds each), MB/s of the uncompressed side, against liblzma 5.8.4 (`lzma_stream_decoder` and `lzma_easy_encoder`, CRC-64); the data in memory both ways, into a buffer made by the call:

| case | sgcl | liblzma |
|---|---|---|
| decompress text (the library's headers, 8.8 MB) | 224 | 183 |
| decompress a program (a test binary, 32 MB) | 747 | 446 |
| decompress random bytes (4 MB, stored chunks) | 23 400 | 1 009 |
| compress text, level 0 | 66.1 (16.54%) | 55.7 (17.22%) |
| compress text, level 6 | 5.19 (12.554%) | 5.40 (12.529%) |
| compress text, level 9 | 5.14 (12.553%) | 5.33 (12.529%) |

The LZMA2 decoder is the one of [`lzma`](lzma.md), level with liblzma's; the difference here is the check: CRC-64 on the processor's carry-less multiply (`hash::crc64`) against liblzma's tables, which shows most over stored chunks. The branch converters run at 2.1–9 GB/s over a 32 MB program (x86 2.1–2.4, ARM64 5.4, ARM-Thumb 4.0, RISC-V 4.4, IA-64 6.5, PowerPC 7.5, SPARC 8.7, ARM 9.1; Delta 2.1 encoding, 1.4 decoding), each a single pass over the bytes.

## Members

### compress, decompress

```cpp
static vector<byte> compress(const slice<const byte>& data);
static vector<byte> compress(const slice<const byte>& data, const options& o);
static vector<byte> compress(const string& text);
static vector<byte> compress(const string& text, const options& o);
static expected<vector<byte>, error> decompress(const slice<const byte>& data);
static expected<vector<byte>, error> decompress(const slice<const byte>& data, const limits& l);
```

The whole of the data at once: `compress` writes one stream of one block, and `decompress` reads every stream there is, checked against [`limits`](README.md#limits) (`max_memory` for a block's dictionary, `max_size` for the output).

### check, filter, options

```cpp
enum class check : uint8_t { none = 0, crc32 = 1, crc64 = 4, sha256 = 10 };
enum class filter : uint8_t { x86, arm, armt, arm64, powerpc, sparc, ia64, riscv };
struct options {
    compress::level level;               // 0..9 as xz -0..-9; 6 unless told otherwise
    bool extreme = false;                // xz -e
    xz::check check = xz::check::crc64;  // xz's default
    optional<xz::filter> bcj;            // a branch converter before LZMA2
    uint16_t delta = 0;                  // Delta's distance, 1..256; 0: none
    uint32_t dictionary = 0;             // 4 KiB .. 1.5 GiB; 0: the level's
};
```

The check of each block, CRC-64 unless told; the level and dictionary of [`lzma`](lzma.md#levels); and the filters before LZMA2: a branch converter for the named processor, Delta over bytes `delta` apart, or both.

### writer

```cpp
explicit writer(const io::writer& out);
writer(const io::writer& out, const options& o);
expected<size_t, io::error> write(const slice<const byte>& data);   // and every form of io::mixin::writer, async_ included
expected<void, io::error> close();                                   // + async_close
bool is_closed() const noexcept;
const optional<io::error>& last_error() const noexcept;             // the first error given, kept
void reset(const io::writer& out);                                   // a new stream, the window and the tables kept
```

What is written goes through the filters into LZMA2 and out as the parser can see far enough ahead (about 4 KB), so the output comes some way behind the input; xz has no flush, and nothing is decodable before `close()`, which writes the rest, the block's check, the index and the footer, and leaves `out` open. The task's forms work in portions of 64 KB with a yield between them. Every error the writer gives is kept as its first (a failure of `out`, options out of range, a write after close): every write and close after it gives that error at once and writes nothing, so a stream is written freely and checked once, at the close; `last_error()` holds it (`reset` clears it).

### reader

```cpp
explicit reader(const io::reader& in);
reader(const io::reader& in, const limits& l);                      // max_memory for the dictionaries
expected<size_t, io::error> read(const slice<byte>& out);           // and every form of io::mixin::reader, async_ included
const optional<error>& last_error() const noexcept;
expected<void, io::error> close();                                  // closes in
void reset(const io::reader& in);
```

The reader decodes a block into its dictionary, a window the output goes round, and through the block's filters, and hands the bytes out from there; the block's check is compared after its last byte is handed out, so a damaged block's bytes are handed out before the error. A converter keeps its last few bytes (fewer than an instruction) until more come, whatever the size of the reads, and its state goes on across them. The task's form lets the worker go every 64 KB handed out. A failure is the error of the read that reaches it and of every read after, `last_error()` the whole of it; the reader reads its input 64 KB at a time, so it may take bytes past the end from its source.

## Examples

### In memory

```cpp
#include "sgcl/compress/compress.h"
#include "sgcl/io/io.h"

using namespace sgcl;

int main() {
    string text = "the same words, the same words, the same words again";
    vector<byte> packed = compress::xz::compress(text, {.check = compress::xz::check::sha256});
    println("{} bytes into {}", text.size(), packed.size());

    auto back = compress::xz::decompress(packed);  // data from outside: checked
    if (!back) {
        println(back.error().message());
        return 1;
    }
    println(back->size());
}
```

Output:

```text
52 bytes into 116
52
```

### A stream with a converter

A program's code with the converter of its processor, through a writer and back through a reader:

```cpp
#include "sgcl/compress/compress.h"
#include "sgcl/io/io.h"

using namespace sgcl;

int main() {
    string text = "the same words, the same words, the same words again";
    io::buffer sink;
    compress::xz::writer w(sink, {.level = 9, .bcj = compress::xz::filter::arm64});
    w.write(text);
    if (auto done = w.close(); !done) {  // the first error of any write, kept
        println(done.error().message());
        return 1;
    }
    compress::xz::reader r(sink);
    string all = r.read_all_text();
    println(all == text);
}
```

Output:

```text
true
```

## See also

[The module](README.md); [`lzma`](lzma.md) for the levels and the parser; `tests/compress/xz.cpp` (liblzma as the oracle both ways, every converter against liblzma's byte for byte) and `tests/compress/fuzz/xz_fuzz.cpp`.
