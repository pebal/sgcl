[sgcl](../README.md) › [codec](README.md) › [qr](qr/README.md)

# sgcl::codec::qr::level

```cpp
#include "sgcl/codec/qr.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    class qr {
    public:
        enum class level : uint8_t { low, medium, quartile, high };
    };
}
```

`sgcl::codec::qr::level` is the error correction of a QR code: how much of the symbol may be lost, dirt or a logo
over it, and still read. More correction is a larger symbol for the same data.

| Value | Description |
|---|---|
| `low` | L: about 7 % of the codewords recoverable |
| `medium` | M: about 15 %; the default |
| `quartile` | Q: about 25 % |
| `high` | H: about 30 % |

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    for (codec::qr::level l : {codec::qr::level::low, codec::qr::level::medium,
                               codec::qr::level::quartile, codec::qr::level::high}) {
        codec::qr code = codec::qr::encode(string(100, 'x'), {.level = l, .boost_level = false});
        println("version {}", code.version());
    }
}
```

Output:

```text
version 5
version 6
version 8
version 10
```

## See also

- [options](qr-options.md): what holds the level
- [correction](qr/correction.md): the level a symbol has
- [sgcl::codec::qr](qr/README.md)
