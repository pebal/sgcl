[sgcl](../README.md) › [encoding](README.md) › [cbor](cbor/README.md)

# sgcl::encoding::cbor::options

```cpp
#include "sgcl/encoding/cbor.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class cbor {
    public:
        struct options {
            uint32_t max_depth = 512;
            bool allow_duplicate_keys = false;
            bool allow_invalid_utf8 = false;
            size_t max_size = size_t(64) << 20;
        };
    };
}
```

`sgcl::encoding::cbor::options` is what [parse](cbor/parse.md) accepts, and [msgpack](msgpack/README.md)'s parse too. The defaults are RFC 8949's valid CBOR: a key
given twice and invalid UTF-8 are errors. A plain struct: set the fields that differ and pass it.

## Rules

- `options` is plain data and holds no pointer: it lives anywhere, and a constant of it may be global.

## Member objects

| Object | Description |
|---|---|
| `max_depth` | how deep arrays, maps and tags may nest: `depth_limit` past it; 512 |
| `allow_duplicate_keys` | a key given twice in one map is taken, the last value winning in the first one's place, rather than `duplicate_key`; `false` |
| `allow_invalid_utf8` | a text string of invalid UTF-8 is taken with its bytes as they are, rather than `invalid_utf8`; `false` |
| `max_size` | the longest item a parse of a stream reads, its declared length refused past it before anything is held: `limit_exceeded`; 64 MiB |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> twice = encoding::hex::decode("a2616101616102").value();
    encoding::cbor::options o;
    o.allow_duplicate_keys = true;
    println(encoding::cbor::parse(twice, o)->to_string());
}
```

Output:

```text
{"a": 2}
```

## See also

- [parse](cbor/parse.md)
- [sgcl::encoding::cbor](cbor/README.md)
