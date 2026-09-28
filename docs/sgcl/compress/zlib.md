# sgcl::compress::zlib

```cpp
#include "sgcl/compress/zlib.h"   // or "sgcl/compress/compress.h"

namespace sgcl::compress {
    class zlib {
    public:
        struct options { compress::level level; slice<const byte> dictionary; };
        class writer;   // as flate::writer
        class reader;   // as flate::reader, and dictionary_id()

        static vector<byte> compress(const slice<const byte>& data);          // and with options, and text
        static expected<vector<byte>, error> decompress(const slice<const byte>& data);   // with limits, with options
        static optional<uint32_t> dictionary_id(const slice<const byte>& data) noexcept;
    };
}
```

zlib (RFC 1950): DEFLATE between a two-byte header (the method, the window, a hint of the level) and the Adler-32 of the data, which the reader checks at the end (`errc::checksum`). PNG's image data, PDF's streams and many protocols are zlib.

A stream made with a **preset dictionary** names it in its header by the dictionary's Adler-32. Reading it without that dictionary, or with another, is `errc::dictionary_required`; `dictionary_id(data)` reads the name from the header of data in memory, and `reader::dictionary_id()` once the reader has read the header, so that a program with several dictionaries picks the right one.

```cpp
auto packed = compress::zlib::compress(message, {.dictionary = sample});
if (auto id = compress::zlib::dictionary_id(packed); id && *id != sample_id) { ... }
auto back = compress::zlib::decompress(packed, {.dictionary = sample});
```

The writer and the reader are [flate's](flate.md), with the header and the trailer around the data.
