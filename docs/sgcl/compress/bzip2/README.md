[sgcl](../../README.md) › [compress](../README.md)

# sgcl::compress::bzip2

```cpp
#include "sgcl/compress/bzip2.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class bzip2;
}
```

`sgcl::compress::bzip2` is bzip2 1.0, both ways: `.bz2` and `.tar.bz2` files, and the BZip2 folders of 7z. The data is
in blocks of up to 900 KB, each sorted by the Burrows-Wheeler transform (the block's rotations in order, its last
column kept), moved to front and coded with Huffman codes chosen for every 50 symbols. It is a class of static
functions and the types of the format: the whole of the data in memory either way ([compress](compress.md),
[decompress](decompress.md)), and a stream each way, a [writer](../bzip2-writer/README.md) and a
[reader](../bzip2-reader/README.md). Go's standard library reads bzip2 and has no compressor.

**Levels** are bzip2's ([level](../bzip2-level/README.md)): 1 to 9, the block size in 100 KB, 9 the default. The work
for a byte hardly changes with the level; a larger block finds more of the data's repeats.

## Rules

- [compress](compress.md) writes one stream of the level's blocks, as `bzip2 -N` does; a [writer](../bzip2-writer/README.md)
  ends a stream at each flush and begins another, which the readers read on as one.
- Several streams one after another (what `pbzip2` makes, and `cat a.bz2 b.bz2`) are one stream, as `bunzip2`
  reads them.
- Every block's CRC and the stream's CRC are checked (`errc::checksum`); a block larger than its header allows,
  selectors or code lengths the format does not allow are `errc::corrupt`. Blocks of the old "randomised" form
  (before bzip2 0.9.5, never made since) are `errc::unsupported`, as in Go.
- [decompress](decompress.md) has the [limits](../limits.md) of every decompression in memory, 1 GiB of output
  unless told otherwise.
- A block comes out only when the whole of it has been read: a stream's first bytes wait for up to 900 KB of the
  data.

## Member types

| Type | Definition |
|---|---|
| `error` | [compress::error](../error/README.md) |
| [level](../bzip2-level/README.md) | the block size in 100 KB, 1 to 9 |
| [options](../bzip2-options.md) | the level |
| [writer](../bzip2-writer/README.md) | an io writer: what is written, compressed into another writer |
| [reader](../bzip2-reader/README.md) | an io reader: what the data read from another reader decompresses to |

## Member functions

#### Operations

| Function | Description |
|---|---|
| [compress](compress.md) | the whole of the data compressed into one stream (static) |
| [decompress](decompress.md) | the whole of the data decompressed, every stream, checked against the limits (static) |

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

- [bzip2::writer](../bzip2-writer/README.md), [bzip2::reader](../bzip2-reader/README.md): the streams
- [tar](../tar.md): `.tar.bz2`, read
- [sevenzip](../sevenzip.md): BZip2 folders, read
- [benchmarks](../benchmarks.md): against libbz2
- [sgcl::compress](../README.md)
