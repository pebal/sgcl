# sgcl::compress::bzip2

```cpp
#include "sgcl/compress/bzip2.h"   // or "sgcl/compress/compress.h"

namespace sgcl::compress {
    class bzip2 {
    public:
        class reader;   // io::reader: what the data read from another reader decompresses to
        static expected<vector<byte>, error> decompress(const slice<const byte>& data, const limits& l = {});
    };
}
```

bzip2 1.0, for reading: `.bz2` and `.tar.bz2` files. As in Go, there is no compressor — bzip2 is read far more often than written today, and a new archive is better made with gzip or zip.

- Several streams one after another (what `pbzip2` makes, and `cat a.bz2 b.bz2`) are one stream, as `bunzip2` reads them.
- Every block's CRC and the stream's CRC are checked (`errc::checksum`); a block larger than its header allows, selectors or code lengths the format does not allow are `errc::corrupt`. Blocks of the old "randomised" form (before bzip2 0.9.5, never made since) are `errc::unsupported`, as in Go.
- The reader decodes straight into the caller's buffer; its task form works in pieces of 64 KB of output with a yield between them.
- `decompress` has the [limit](README.md#limits) of every decompression in memory, 1 GiB unless told otherwise.

It is faster than libbz2 on the same data: 0.66–0.93× its time on e.txt, Newton's Opticks and random data (measured against `BZ2_bzBuffToBuffDecompress` when the module was written).

## reader

```cpp
explicit reader(const io::reader& in);
expected<size_t, io::error> read(const slice<byte>& out);   // and every form of io::mixin::reader, async_ included
const optional<error>& last_error() const noexcept;
expected<void, io::error> close();                          // closes in
```

```cpp
compress::bzip2::reader r(io::open("words.bz2"));
auto text = r.read_all_text();
```
