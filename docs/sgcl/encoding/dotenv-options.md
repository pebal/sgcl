[sgcl](../README.md) › [encoding](README.md) › [dotenv](dotenv/README.md)

# sgcl::encoding::dotenv::options

```cpp
#include "sgcl/encoding/dotenv.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class dotenv {
    public:
        struct options {
            bool expand = true;
            bool use_environment = true;
            size_t max_size = size_t(16) << 20;
        };
    };
}
```

`sgcl::encoding::dotenv::options` is what [parse](dotenv/parse.md) does. A plain struct: set the fields that differ and
pass it.

## Rules

- `options` is plain data and holds no pointer: it lives anywhere, and a constant of it may be global.

## Member objects

| Object | Description |
|---|---|
| `expand` | `$NAME` and `${NAME...}` in unquoted and double-quoted values replaced; off, every `$` is kept as written; `true` |
| `use_environment` | a name that is no key above it looked up in the process environment; off, it is empty: a reading that depends on the file alone; `true` |
| `max_size` | the bytes of the values together, their expansions made: `limit_exceeded` past it, so that lines that double a value (`A=$A$A`) stop at once; 16 MiB |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::dotenv::options o;
    o.expand = false;
    println(encoding::dotenv::parse("A=1\nB=${A}", o)->get("B", "?"));
    println(encoding::dotenv::parse("A=1\nB=${A}")->get("B", "?"));
}
```

Output:

```text
${A}
1
```

## See also

- [parse](dotenv/parse.md)
- [sgcl::encoding::dotenv](dotenv/README.md)
