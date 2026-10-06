[sgcl](../README.md) › [encoding](README.md) › [toml](toml/README.md)

# sgcl::encoding::toml::options

```cpp
#include "sgcl/encoding/toml.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class toml {
    public:
        struct options {
            uint32_t max_depth = 512;
        };
    };
}
```

`sgcl::encoding::toml::options` is what [parse](toml/parse.md) accepts. A plain struct: set the fields that differ and
pass it.

## Rules

- `options` is plain data and holds no pointer: it lives anywhere, and a constant of it may be global.

## Member objects

| Object | Description |
|---|---|
| `max_depth` | how deep tables and arrays may nest below the root, by headers, dotted keys or inline: `depth_limit` past it; 512 |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::toml::options o;
    o.max_depth = 2;
    println(encoding::toml::parse("a.b.c = 1", o)->operator[]("a")["b"]["c"].as_int(0));
    println(encoding::toml::parse("a.b.c.d = 1", o).error().message());
}
```

Output:

```text
1
1:5: tables and arrays nested deeper than max_depth
```

## See also

- [parse](toml/parse.md)
- [sgcl::encoding::toml](toml/README.md)
