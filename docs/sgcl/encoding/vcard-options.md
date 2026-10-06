[sgcl](../README.md) › [encoding](README.md) › [vcard](vcard/README.md)

# sgcl::encoding::vcard::options

```cpp
#include "sgcl/encoding/vcard.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class vcard {
    public:
        struct options {
            size_t max_size = size_t(64) << 20;
        };
    };
}
```

`sgcl::encoding::vcard::options` is what [parse](vcard/parse.md) and [parse_all](vcard/parse_all.md) accept. A plain
struct: set the fields that differ and pass it.

## Rules

- `options` is plain data and holds no pointer: it lives anywhere, and a constant of it may be global.

## Member objects

| Object | Description |
|---|---|
| `max_size` | the text's bytes: `limit_exceeded` past it; 64 MiB |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::vcard::options o;
    o.max_size = 16;
    println(encoding::vcard::parse("BEGIN:VCARD\nVERSION:4.0\nEND:VCARD\n", o).error().message());
}
```

Output:

```text
1:1: a text past max_size
```

## See also

- [parse](vcard/parse.md)
- [sgcl::encoding::vcard](vcard/README.md)
