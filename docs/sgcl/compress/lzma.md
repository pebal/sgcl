# sgcl::compress::lzma

```cpp
#include "sgcl/compress/lzma.h"   // or "sgcl/compress/compress.h"

namespace sgcl::compress {
    class lzma {
    public:
        struct options {
            compress::level level;       // 0..9 as xz -0..-9; 6 unless told otherwise
            bool extreme = false;        // xz -e
            uint32_t dictionary = 0;     // 4 KiB .. 1.5 GiB; 0: the level's
            uint8_t lc = 3, lp = 0, pb = 2;
        };
        class writer;   // io::writer: what is written, compressed into another writer
        class reader;   // io::reader: what the data read from another reader decompresses to
        static constexpr size_t HeaderSize = 13;

        static vector<byte> compress(const slice<const byte>& data);          // and (data, options), (text), (text, options)
        static expected<vector<byte>, error> decompress(const slice<const byte>& data);
        static expected<vector<byte>, error> decompress(const slice<const byte>& data, const limits& l);
    };
}
```

LZMA in its first file format, "LZMA alone": the `.lzma` files of `xz --format=lzma`, `lzma` and the LZMA SDK. A header of 13 bytes — lc, lp and pb in one byte, the dictionary's size, the size decompressed or all ones when it is not known — then the range coder's data, ended by a marker when the size is not known. LZMA finds repeats as far back as its dictionary reaches (8 MiB at the default level, against DEFLATE's 32 KB) and codes every bit against probabilities it learns as it goes: on the library's own headers (8.7 MB of C++) it makes a third less than [gzip](gzip.md) at level 9 (1.09 MB against 1.60), for about three times gzip's work to compress and a sixth of its speed to decompress. The `.xz` container, LZMA2 and 7z follow in the same module.

## Levels

The levels are xz's, with its dictionaries and its two modes:

| level | dictionary | parser | match finder |
|---|---|---|---|
| 0 | 256 KiB | fast | hash chain, 4 candidates |
| 1 | 1 MiB | fast | hash chain, 8 |
| 2 | 2 MiB | fast | hash chain, 24 |
| 3 | 4 MiB | fast | hash chain, 48 |
| 4 | 4 MiB | optimal | binary tree, nice length 16 |
| 5 | 8 MiB | optimal | binary tree, 32 |
| 6 (default) | 8 MiB | optimal | binary tree, 64 |
| 7, 8, 9 | 16, 32, 64 MiB | optimal | binary tree, 64 |

The fast parser takes the longest match found, a repeat of a recent distance when it is nearly as long, and a literal instead when the next position has a better match. The optimal parser prices every way of coding the next few thousand bytes (literals, matches, repeats of the four last distances, and a match followed by a literal and the same distance again) against the coder's current probabilities, and takes the cheapest. `extreme` searches deeper (nice length 273, 512 candidates; 192 at levels 3 and 5), as xz's `-e` does. Level 0 is not "stored" here: it is the fastest LZMA.

## Rules

- `compress` knows the size and writes it into the header with no end marker, as the LZMA SDK does; a `writer` does not, and writes all ones and the marker, as xz does. `decompress` and `reader` take both, and the size known followed by the marker as well. Every decoder of the format reads either.
- The output is valid LZMA that any decoder reads, not byte for byte what xz makes: the format leaves the encoder its choices. On the text and the program measured its size is within 0.3% of xz's at every level but 0, where it is smaller (see Performance).
- `.lzma` has no checksum. A decoder finds data the format does not allow — a distance before the start, a marker before the size in the header, a range coder that does not end at zero, data cut short (`errc::corrupt`, `errc::unexpected_end`) — but a flipped bit may also decode to other bytes without an error. Data that must arrive intact goes in a container with a check (`.xz`, 7z, or a hash of its own).
- **Memory.** The decoder's dictionary is as large as the header asks, or as the data when the header gives its size, and a stream's reader allocates it; [`limits.max_memory`](README.md#limits) (1 GiB unless told otherwise) is checked against it before anything is taken — more is `errc::too_large` at offset 0, in memory and as a stream alike. `decompress` has `max_size` as well, and a size in the header past it fails before any work. The encoder's tables take about 10 times its dictionary at levels 4 to 9 (the binary tree: 80 MiB at level 6, 576 MiB at level 9) and 6 times at levels 0 to 3; a writer adds a window of about 1.25 times the dictionary. `compress` sizes the tables to the data when it is smaller than the dictionary, and codes the data where it lies.
- Options out of range (lc past 8, lp or pb past 4, a dictionary under 4 KiB or past 1.5 GiB, `level::huffman_only`) are the program's mistake: `compress` throws `std::invalid_argument`, and a writer's first write reports `errc::invalid_argument`.
- Nothing after the LZMA data is read by `decompress`; the reader reads its input 64 KB at a time, so it may take bytes past the end from its source.

## Performance

On an Apple M-series machine (`benchmarks/compress/lzma.cpp`, the median of five processes of two seconds each), MB/s of the uncompressed side, against liblzma 5.8.4 (xz's library, `lzma_alone_decoder` and `lzma_alone_encoder` at the same preset); the data in memory both ways, into a buffer made by the call:

| case | sgcl | liblzma |
|---|---|---|
| decompress text (the library's headers, 8.7 MB) | 207 | 204 |
| decompress a program (a test binary, 32 MB) | 648 | 655 |
| decompress random bytes (4 MB, literals only) | 30.9 | 32.5 |
| compress text, level 0 | 65.0 (16.51%) | 57.1 (17.19%) |
| compress text, level 6 | 4.26 (12.537%) | 4.39 (12.522%) |
| compress text, level 9 | 4.18 (12.537%) | 4.36 (12.522%) |

The decoder is level with liblzma; its fast loop runs while a symbol's worth of input (32 bytes) and a match's worth of room are at hand, and the last bytes of a stream fed in pieces are decoded dry first to see whether a whole symbol is there. The encoder's sizes are within 0.2% of liblzma's (on the program 0.2% smaller at levels 6 and 9); level 0 hashes four bytes where xz's hashes three, which on text is 4% smaller and 12% faster.

## Members

### compress, decompress

```cpp
static vector<byte> compress(const slice<const byte>& data);
static vector<byte> compress(const slice<const byte>& data, const options& o);
static vector<byte> compress(const string& text);
static vector<byte> compress(const string& text, const options& o);
static expected<vector<byte>, error> decompress(const slice<const byte>& data);
static expected<vector<byte>, error> decompress(const slice<const byte>& data, const limits& l);
static constexpr size_t HeaderSize = 13;
```

The whole of the data at once: `compress` writes the size into the header and no end marker, and `decompress` checks the data against [`limits`](README.md#limits) (`max_memory` for the dictionary, `max_size` for the output). `HeaderSize` is the header's 13 bytes.

### options

```cpp
struct options {
    compress::level level;       // 0..9 as xz -0..-9; 6 unless told otherwise
    bool extreme = false;        // xz -e
    uint32_t dictionary = 0;     // 4 KiB .. 1.5 GiB; 0: the level's
    uint8_t lc = 3, lp = 0, pb = 2;
};
```

The level and its mode of the [table](#levels), a dictionary of another size, and the literal and position bits of the header (lc up to 8, lp and pb up to 4).

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

The writer copies what is written into a window of its own and codes it as the parser can see far enough ahead of each position (about 4 KB), so the output comes some way behind the input and all of it only at `close()`, which writes the end marker and the coder's last bytes and leaves `out` open. LZMA has no flush: nothing written is decodable before the close. The tables and the window are taken at the first write. The task's forms work in portions of 64 KB of input with a yield between them. Every error the writer gives is kept as its first (a failure of `out`, options out of range, a write after close): every write and close after it gives that error at once and writes nothing, so a stream is written freely and checked once, at the close; `last_error()` holds it (`reset` clears it).

### reader

```cpp
explicit reader(const io::reader& in);
reader(const io::reader& in, const limits& l);                      // max_memory for the dictionary
expected<size_t, io::error> read(const slice<byte>& out);           // and every form of io::mixin::reader, async_ included
const optional<error>& last_error() const noexcept;
expected<void, io::error> close();                                  // closes in
void reset(const io::reader& in);
```

The reader decodes into its dictionary, a window the output goes round, and hands the bytes out from there; the task's form lets the worker go every 64 KB handed out. A failure is the error of the read that reaches it and of every read after, `last_error()` the whole of it.

## Examples

### In memory

```cpp
#include "sgcl/compress/compress.h"
#include "sgcl/io/io.h"

using namespace sgcl;

int main() {
    string text = "the same words, the same words, the same words again";
    vector<byte> packed = compress::lzma::compress(text, {.level = 9});
    println("{} bytes into {}", text.size(), packed.size());

    auto back = compress::lzma::decompress(packed);  // data from outside: checked
    if (!back) {
        println(back.error().message());
        return 1;
    }
    println(back->size());
}
```

Output:

```text
52 bytes into 43
52
```

### Streams

A writer into a buffer, a reader out of it:

```cpp
#include "sgcl/compress/compress.h"
#include "sgcl/io/io.h"

using namespace sgcl;

int main() {
    string text = "the same words, the same words, the same words again";
    io::buffer sink;
    compress::lzma::writer w(sink, {.level = 1});
    w.write(text);
    w.write(text);
    if (auto done = w.close(); !done) {  // the first error of any write, kept
        println(done.error().message());
        return 1;
    }
    compress::lzma::reader r(sink);
    string all = r.read_all_text();
    println(all.size());
}
```

Output:

```text
104
```

## See also

[The module](README.md); [`gzip`](gzip.md) for what must be fast or read by browsers; `tests/compress/lzma.cpp` (liblzma as the oracle both ways) and `tests/compress/fuzz/lzma_fuzz.cpp`.
