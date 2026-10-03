[sgcl](../README.md) › [compress](README.md)

# sgcl::compress::limits

```cpp
#include "sgcl/compress/limits.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    struct limits {
        uint64_t max_size = uint64_t(1) << 30;
        uint64_t max_memory = uint64_t(1) << 30;
        uint64_t max_entries = 1000000;
    };
}
```

`sgcl::compress::limits` bounds what data from outside may make the module do. Compressed data may decompress a
thousand times over (a "bomb"), and a header may ask a decoder for gigabytes of memory, so every function that
decompresses data in memory takes a `limits`, 1 GiB of output and 1 GiB of a decoder's memory unless told
otherwise, and stops with `errc::too_large` where the data would pass them.

The `decompress` of every format, `zip::archive::read` and `sevenzip::archive::read` stop at `max_size` bytes;
`limits{UINT64_MAX}` lifts the bound. A stream has no `max_size` of its own: its reader decides how much it reads,
and [io::limit_reader](../io/limit_reader/README.md) over it bounds what a program takes. `max_memory` bounds what a header makes a
decoder allocate — LZMA's and LZMA2's dictionaries, up to 4 GiB by the format, PPMd's model, a 7z folder's decoders
together — in memory and in a stream alike: more is `errc::too_large` before anything is taken. `max_entries`
bounds the files, folders and streams a 7z header may list, whose table is made before any entry is read.

## Rules

- An aggregate of three numbers, passed by `const limits&`: `{.max_size = 100 << 20}` sets one and keeps the
  others' defaults.
- A size the data states (an LZMA header's, an xz block's) is checked against `max_size` before any work; one it
  does not state is checked as the output grows.
- The archives' `extract` functions have their own bound, the `max_size` of their options (the files' bytes
  together, 1 GiB unless set), checked before anything is written.
- A 7z key of more than 2^24 rounds is refused the same way, `errc::too_large`, before any round is run.

## Member objects

| Member | Description |
|---|---|
| `max_size` | the bytes a decompression in memory may make; 1 GiB by default |
| `max_memory` | the memory a decoder may take because the data asks for it; 1 GiB by default |
| `max_entries` | the entries a 7z header may list; 1 000 000 by default |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string text = string("the same words, the same words again and again. ").repeat(2000);
    auto packed = compress::gzip::compress(text);
    println("{} bytes into {}", text.size(), packed.size());

    auto small = compress::gzip::decompress(packed, {.max_size = 1000});
    println("{}", small.error().message());

    auto all = compress::gzip::decompress(packed, compress::limits{UINT64_MAX});
    println("{}", all->size());
}
```

Output:

```text
96000 bytes into 349
offset 64: decompressed data past the limit
96000
```

## See also

- [errc](errc.md): `too_large`
- [io::limit_reader](../io/limit_reader/README.md): a bound on a stream
- [sgcl::compress](README.md)
