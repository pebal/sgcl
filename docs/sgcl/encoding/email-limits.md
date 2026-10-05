[sgcl](../README.md) › [encoding](README.md) › [email](email/README.md) › limits

# sgcl::encoding::email::limits

```cpp
#include "sgcl/encoding/email.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class email {
    public:
        struct limits {
            size_t max_header_bytes = 256 * 1024;
            size_t max_depth = 32;
            size_t max_parts = 10000;
        };
    };
}
```

`sgcl::encoding::email::limits` is what a [parse](email/parse.md) takes before it refuses a message: the bytes of
one entity's head, the nesting of multiparts and messages inside messages, the parts of the whole message. Each
is checked once, where its thing is found. Python's `email` package has none; Go bounds a multipart form's parts.
A plain struct, its fields set by name.

## Member objects

| Member | Description |
|---|---|
| `max_header_bytes` | the bytes of the head of the message or of one part, its empty line included; past it `errc::limit_exceeded`; 256 KB by default |
| `max_depth` | how deep multiparts and messages may nest; past it `errc::depth_limit`; 32 by default |
| `max_parts` | the parts of the whole message, at every depth; past it `errc::limit_exceeded`; 10000 by default |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


int main() {
    string many = "Content-Type: multipart/mixed; boundary=b\r\n\r\n";
    for (int i : range(5)) {
        many = string::concat(many, "--b\r\n\r\npart\r\n");
    }
    encoding::email::limits l;
    l.max_parts = 3;
    println("{}", encoding::email::parse(many, l).error().message());
    println("{}", encoding::email::parse(many)->body().parts().size());
}
```

Output:

```text
offset 89: too many parts
5
```

## See also

- [parse](email/parse.md), [load](email/load.md): what take them
- [email](email/README.md)
