[sgcl](../README.md) › [compress](README.md)

# sgcl::compress::bzip2

```cpp
#include "sgcl/compress/bzip2.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class bzip2;
}
```

`sgcl::compress::bzip2` is bzip2 1.0, for reading: `.bz2` and `.tar.bz2` files, and the BZip2 folders of 7z. The
data is in blocks of up to 900 KB, each sorted by the Burrows-Wheeler transform and coded with Huffman codes. As in
Go, there is no compressor — bzip2 is read far more often than written today, and a new archive is better made with
gzip, xz or zip. It is a class of static functions and the types of the format: the whole of the data in memory
([decompress](bzip2/decompress.md)), or a stream, a [reader](bzip2-reader.md) of what the data read from another
reader decompresses to.

## Rules

- Several streams one after another (what `pbzip2` makes, and `cat a.bz2 b.bz2`) are one stream, as `bunzip2`
  reads them.
- Every block's CRC and the stream's CRC are checked (`errc::checksum`); a block larger than its header allows,
  selectors or code lengths the format does not allow are `errc::corrupt`. Blocks of the old "randomised" form
  (before bzip2 0.9.5, never made since) are `errc::unsupported`, as in Go.
- [decompress](bzip2/decompress.md) has the [limits](limits.md) of every decompression in memory, 1 GiB of output
  unless told otherwise.
- A block comes out only when the whole of it has been read: a stream's first bytes wait for up to 900 KB of the
  data.

## Member types

| Type | Definition |
|---|---|
| `error` | [compress::error](error.md) |
| [reader](bzip2-reader.md) | an io reader: what the data read from another reader decompresses to |

## Member functions

#### Operations

| Function | Description |
|---|---|
| [decompress](bzip2/decompress.md) | the whole of the data decompressed, every stream, checked against the limits (static) |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // "hello, hello, hello" as bzip2 -9 writes it
    auto packed = encoding::hex::decode("425a68393141592653599c453ed3000003910040040244a0002122"
                                        "3030065227e454585dc914e142427114fb4c");
    auto text = compress::bzip2::decompress(*packed);
    if (!text) {
        println("{}", text.error().message());
        return 1;
    }
    println("{}", string(slice<const byte>(*text)));
}
```

Output:

```text
hello, hello, hello
```

## See also

- [tar](tar.md): `.tar.bz2`, read
- [sevenzip](sevenzip.md): BZip2 folders, read
- [benchmarks](benchmarks.md): against libbz2
- [sgcl::compress](README.md)
