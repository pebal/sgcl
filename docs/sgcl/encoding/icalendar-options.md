[sgcl](../README.md) › [encoding](README.md) › [icalendar](icalendar/README.md)

# sgcl::encoding::icalendar::options

```cpp
#include "sgcl/encoding/icalendar.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class icalendar {
    public:
        struct options {
            uint32_t max_depth = 64;
            size_t max_size = size_t(64) << 20;
        };
    };
}
```

`sgcl::encoding::icalendar::options` is what [parse](icalendar/parse.md) and [parse_all](icalendar/parse_all.md)
accept. A plain struct: set the fields that differ and pass it.

## Rules

- `options` is plain data and holds no pointer: it lives anywhere, and a constant of it may be global.

## Member objects

| Object | Description |
|---|---|
| `max_depth` | components inside one another, the VCALENDAR the first: `depth_limit` past it; 64 |
| `max_size` | the text's bytes: `limit_exceeded` past it; 64 MiB |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::icalendar::options o;
    o.max_depth = 1;
    println(encoding::icalendar::parse("BEGIN:VCALENDAR\nBEGIN:VEVENT\nEND:VEVENT\nEND:VCALENDAR\n", o).error().message());
}
```

Output:

```text
2:1: components nested deeper than max_depth
```

## See also

- [parse](icalendar/parse.md)
- [sgcl::encoding::icalendar](icalendar/README.md)
