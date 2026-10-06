[sgcl](../README.md) › [encoding](README.md) › [asn1](asn1/README.md)

# sgcl::encoding::asn1::options

```cpp
#include "sgcl/encoding/asn1.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class asn1 {
    public:
        struct options {
            bool ber = false;
            uint32_t max_depth = 512;
            size_t max_size = size_t(64) << 20;
        };

        static const options der;
        static const options ber;
    };
}
```

`sgcl::encoding::asn1::options` is what [parse](asn1/parse.md) accepts. `asn1::der` is the
defaults and `asn1::ber` the same with `ber` set, the two a program names most: `asn1::parse(bytes, asn1::ber)`. A
plain struct: set the fields that differ and pass it.

## Rules

- `options` is plain data and holds no pointer: it lives anywhere, and a constant of it may be global.
- A task's [async_parse](asn1/parse.md) takes its options by value, so the caller's may be gone when it runs.

## Member objects

| Object | Description |
|---|---|
| `ber` | BER rather than DER: the indefinite length, strings in pieces, lengths in a longer form than they need, a BOOLEAN of any byte, a BIT STRING's unused bits set, the times of X.680 in every form (without seconds, a fraction of the hour or the minute, a comma, an offset, local time read as UTC); `false` |
| `max_depth` | how deep elements may nest inside one another: `depth_limit` past it; 512 |
| `max_size` | the longest element [parse](asn1/parse.md) reads from a stream, its declared length refused past it before anything is held: `limit_exceeded`; 64 MiB |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> deep = encoding::hex::decode("3004300230000000").value();
    encoding::asn1::options o;
    o.max_depth = 2;
    println(encoding::asn1::parse(deep.as_slice(0, 6), o).error().message());
    println(bool(encoding::asn1::parse(deep.as_slice(0, 6))));
}
```

Output:

```text
offset 4: elements nested deeper than max_depth
true
```

## See also

- [parse](asn1/parse.md)
- [sgcl::encoding::asn1](asn1/README.md)
