[sgcl](../README.md) › [codec](README.md) › [pnm](pnm/README.md)

# sgcl::codec::pnm::options

```cpp
#include "sgcl/codec/pnm.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    class pnm {
    public:
        struct options {
            pnm::kind kind = pnm::kind::automatic;
            bool plain = false;
        };
    };
}
```

`sgcl::codec::pnm::options` is what [encode](pnm/encode.md) is told: the [kind](pnm-kind.md) of file and whether
it is written plain, the samples as numbers in text, a plain struct written in place as `{.kind =
codec::pnm::kind::pgm, .plain = true}`. Plain files are for reading by eye and for old tools; raw ones are a
fraction of their size and read faster.

## Member objects

| Member | Description |
|---|---|
| `kind` | the format written; `kind::automatic` unless told: PGM, PPM or PAM by the image's format |
| `plain` | P1, P2 or P3 rather than P4, P5 or P6; PAM asked plain is `errc::invalid_argument`; false unless told |

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image gray(16, 16, codec::pixel_format::gray8);
    println("raw {} bytes", codec::pnm::encode(gray)->size());
    println("plain {} bytes", codec::pnm::encode(gray, {.plain = true})->size());
}
```

Output:

```text
raw 269 bytes
plain 525 bytes
```

## See also

- [kind](pnm-kind.md): the formats
- [encode](pnm/encode.md): what takes the options
- [sgcl::codec::pnm](pnm/README.md)
