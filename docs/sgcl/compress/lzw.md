# sgcl::compress::lzw

```cpp
#include "sgcl/compress/lzw.h"   // or "sgcl/compress/compress.h"

namespace sgcl::compress {
    class lzw {
    public:
        enum class order : uint8_t { lsb, msb };   // GIF; TIFF and PDF
        class writer;   // io::writer
        class reader;   // io::reader

        static vector<byte> compress(const slice<const byte>& data, order o, int literal_width);
        static expected<vector<byte>, error> decompress(const slice<const byte>& data, order o, int literal_width, const limits& l = {});
    };
}
```

LZW as Go's `compress/lzw` has it: codes of up to 12 bits after a clear code and an end code, least significant bit first for GIF, most significant first for TIFF and PDF; literals of 2 to 8 bits (GIF's image data uses 2..8, TIFF and PDF 8).

- `literal_width` outside 2..8 is the program's mistake: `std::invalid_argument`. So is a byte the width cannot hold (5 with a width of 2) given to `compress`; the writer's `write` reports it as `errc::invalid_argument`.
- The compressor sends a clear code when the table fills, in place of its last code, as Go does: its output is Go's, byte for byte.
- The decompressor stops at the end code and reads nothing after it. A table that fills with no clear stays full and gains nothing until a clear comes (the "deferred clear" of GIF encoders, as Go reads it).
- TIFF's "early change" variant (the width grows one code early) is not read.
- The writer and the reader take `(stream, order, literal_width)` and have every form of an io stream, the task's forms included:

```cpp
writer(const io::writer& out, order o, int literal_width);
expected<size_t, io::error> write(const slice<const byte>& data);   // + async_write
expected<void, io::error> close();                                  // the end code; out stays open; + async_close
bool is_closed() const noexcept;

reader(const io::reader& in, order o, int literal_width);
expected<size_t, io::error> read(const slice<byte>& out);           // + async_read
const optional<error>& last_error() const noexcept;
expected<void, io::error> close();                                  // closes in
```

```cpp
auto packed = compress::lzw::compress(pixels, compress::lzw::order::lsb, 8);
auto back = compress::lzw::decompress(packed, compress::lzw::order::lsb, 8);
```
