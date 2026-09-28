# sgcl::compress::flate

```cpp
#include "sgcl/compress/flate.h"   // or "sgcl/compress/compress.h"

namespace sgcl::compress {
    class flate {
    public:
        struct options { compress::level level; slice<const byte> dictionary; };
        class writer;   // io::writer: what is written, compressed into another writer
        class reader;   // io::reader: what the data read from another reader decompresses to

        static vector<byte> compress(const slice<const byte>& data);          // and (data, options), (text), (text, options)
        static expected<vector<byte>, error> decompress(const slice<const byte>& data);
        static expected<vector<byte>, error> decompress(const slice<const byte>& data, const limits& l);
        static expected<vector<byte>, error> decompress(const slice<const byte>& data, const options& o, const limits& l = {});
    };
}
```

DEFLATE (RFC 1951) with nothing around it: the data of a zip entry, of PNG (inside zlib) and of gzip. `compress` makes the shortest of the format's three forms for every block (its own Huffman codes, the fixed ones, or stored), from a search of 32 KB of history whose effort the [level](README.md#level) sets; `decompress` takes any valid DEFLATE, whoever made it.

A **dictionary** is data both sides agree on in advance, which the first matches may refer to: short messages of one kind (JSON of one schema, HTTP headers) compress much better against a sample of them (Go's `NewWriterDict`). The reader must be given the same bytes.

## Rules

- The output is valid DEFLATE that any decoder reads, but not byte for byte what zlib or Go make for the same input: the format leaves the encoder its choices. Its size is within 0.2% of zlib's at every level.
- `flate::reader` and `decompress` stop at the end of the DEFLATE data and give nothing after it; the reader reads its input a block at a time, so it may take bytes past that end from its source. A format with more after it (a zip entry, a gzip trailer) gives the reader only its part ([io::limit_reader](../io/stream.md)), or uses [gzip](gzip.md) and [zlib](zlib.md), which handle their trailers themselves.
- A decoded stream is checked as it is read: a code that is not one, a distance past the start or a stored block whose length does not match its complement is `errc::corrupt` at the read that reaches it, and the bytes before it were handed out.

## writer

```cpp
explicit writer(const io::writer& out);
writer(const io::writer& out, const options& o);
expected<size_t, io::error> write(const slice<const byte>& data);   // and every form of io::mixin::writer, async_ included
expected<void, io::error> flush();                                   // + async_flush
expected<void, io::error> close();                                   // + async_close; async_write as every io writer has it
bool is_closed() const noexcept;
const optional<io::error>& last_error() const noexcept;             // the first error given, kept
void reset(const io::writer& out);
```

`flush()` ends the current block and aligns the output with an empty stored block (a *sync flush*), so that the other side can decode everything written so far: a stream of messages, a response sent in parts. It costs a few bytes each time. `close()` writes the last block and leaves `out` open; writing after it is `io::errc::closed`. Every error the writer gives is kept as its first (a failure of `out`, a gzip header it cannot write, a write or flush after close): every write, flush and close after it gives that error at once and writes nothing, so a stream is written freely and checked once, at the close; `last_error()` holds it (`reset` clears it). After the first error, its own included, the writer refuses everything further and `close()` returns that error; whoever wants to react earlier checks the result of a single `write` or `flush`, or `last_error()`.

## reader

```cpp
explicit reader(const io::reader& in);
reader(const io::reader& in, const options& o);                     // the dictionary
expected<size_t, io::error> read(const slice<byte>& out);           // and every form of io::mixin::reader, async_ included
const optional<error>& last_error() const noexcept;
expected<void, io::error> close();                                  // closes in
void reset(const io::reader& in);
```

```cpp
auto packed = compress::flate::compress("hello, hello, hello");
compress::flate::reader r{io::buffer(packed)};
auto text = r.read_all_text();                                      // "hello, hello, hello"
```
