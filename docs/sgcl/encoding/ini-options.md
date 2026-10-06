[sgcl](../README.md) › [encoding](README.md) › [ini](ini/README.md)

# sgcl::encoding::ini::options

```cpp
#include "sgcl/encoding/ini.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class ini {
    public:
        struct options {
            bool allow_duplicates = false;
            bool allow_no_value = false;
        };
    };
}
```

`sgcl::encoding::ini::options` is what [parse](ini/parse.md) accepts. A plain struct: set the fields that differ and
pass it.

## Rules

- `options` is plain data and holds no pointer: it lives anywhere, and a constant of it may be global.

## Member objects

| Object | Description |
|---|---|
| `allow_duplicates` | a section given twice is one section, its entries merged; a key given twice keeps its first place and takes its last value (configparser's `strict=False`); `false` |
| `allow_no_value` | a line of a key alone is the key with an empty value (configparser's `allow_no_value`, whose value is `None`); `false` |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::ini::options o;
    o.allow_duplicates = true;
    o.allow_no_value = true;
    auto config = encoding::ini::parse("[a]\nx = 1\nflag\n[a]\nx = 2\n", o);
    println("[{}] [{}]", config->get("a", "x", "?"), config->get("a", "flag", "?"));
    println(encoding::ini::parse("[a]\nx = 1\n[a]\n").error().message());
}
```

Output:

```text
[2] []
3:1: the section [a] given twice
```

## See also

- [parse](ini/parse.md)
- [sgcl::encoding::ini](ini/README.md)
