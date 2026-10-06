[sgcl](../README.md) › [encoding](README.md) › [yaml](yaml/README.md)

# sgcl::encoding::yaml::options

```cpp
#include "sgcl/encoding/yaml.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class yaml {
    public:
        struct options {
            uint32_t max_depth = 512;
            size_t max_alias_nodes = size_t(1) << 20;
            bool allow_duplicate_keys = false;
        };
    };
}
```

`sgcl::encoding::yaml::options` is what [parse](yaml/parse.md) and [parse_all](yaml/parse_all.md) accept. A plain struct:
set the fields that differ and pass it.

## Rules

- `options` is plain data and holds no pointer: it lives anywhere, and a constant of it may be global.

## Member objects

| Object | Description |
|---|---|
| `max_depth` | how deep collections may nest: `depth_limit` past it; 512 |
| `max_alias_nodes` | the nodes reached through aliases, each alias counting the nodes of its anchor's node: `limit_exceeded` past it, the billion laughs stopped; 2^20 |
| `allow_duplicate_keys` | a key given twice in one mapping is taken, the last value winning in the first one's place, rather than `duplicate_key`; `false` |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::yaml::options o;
    o.allow_duplicate_keys = true;
    println(encoding::yaml::parse("a: 1\na: 2", o)->operator[]("a").as_int(0));
    println(encoding::yaml::parse("a: 1\na: 2").error().message());
}
```

Output:

```text
2
2:1: a key given twice in a mapping
```

## See also

- [parse](yaml/parse.md)
- [sgcl::encoding::yaml](yaml/README.md)
