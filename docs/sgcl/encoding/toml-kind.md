[sgcl](../README.md) › [encoding](README.md) › [toml](toml/README.md)

# sgcl::encoding::toml::kind

```cpp
#include "sgcl/encoding/toml.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class toml {
    public:
        enum class kind : uint8_t {
            table,
            string,
            integer,
            floating,
            boolean,
            offset_datetime,
            local_datetime,
            local_date,
            local_time,
            array
        };
    };
}
```

`sgcl::encoding::toml::kind` is the kind of a [toml](toml/README.md) value, as [type](toml/type.md) gives it: TOML
1.0's types.

| Value | Description |
|---|---|
| `table` | a table; what a lookup that found nothing gives, empty |
| `string` | a string |
| `integer` | a 64-bit integer, in any of its bases |
| `floating` | a float, `inf` and `nan` among them |
| `boolean` | `true` or `false` |
| `offset_datetime` | a date and a time with an offset: an instant |
| `local_datetime` | a date and a time without an offset |
| `local_date` | a date |
| `local_time` | a time of the clock |
| `array` | an array |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto v = encoding::toml::parse("a = 0x1F\nb = 1979-05-27").value();
    println(v["a"].type() == encoding::toml::kind::integer);
    println(v["b"].type() == encoding::toml::kind::local_date);
}
```

Output:

```text
true
true
```

## See also

- [type](toml/type.md)
- [sgcl::encoding::toml](toml/README.md)
