[sgcl](../README.md) › [slog](README.md) › [value](value.md) › kind

# sgcl::slog::value::kind

```cpp
#include "sgcl/slog/record.h"   // or "sgcl/slog.h"

namespace sgcl::slog {
    class value {
    public:
        enum class kind : uint8_t {
            null, boolean, int64, uint64, float64, string, duration, time, group, any
        };
    };
}
```

The kind of a [value](value.md), slog's `Kind`, as [type](value/type.md) says it: which `as_` accessor gives the
value. Every kind is written by [text](value/text.md) and [json](value/json.md).

| Value | Description |
|---|---|
| `null` | no value: `nullptr`, an empty `optional`, a null `exception_ptr`, a default-constructed value; `<nil>` in text, `null` in JSON |
| `boolean` | a `bool`: [as_bool](value/as_bool.md) |
| `int64` | a signed integer: [as_int](value/as_int.md) |
| `uint64` | an unsigned integer: [as_uint](value/as_uint.md) |
| `float64` | a `float` or a `double`: [as_double](value/as_double.md) |
| `string` | a text, whatever made it — a literal, a string, an error's message, a type's own text: [as_string](value/as_string.md) |
| `duration` | a `duration` or a `std::chrono` duration: [as_duration](value/as_duration.md) |
| `time` | a `time::datetime`: [as_time](value/as_time.md) |
| `group` | a [group](group.md), a logger's group, a type described by its fields: [as_group](value/as_group.md) |
| `any` | Go's `KindAny`: a container, a map, a variant or a `json` of the program, read through `text()` and `json()` |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::memory kept;
    slog::logger(kept).info("m", "n", nullptr, "b", false, "u", 7u, "s", "x",
                            slog::group("g", "a", 1));
    for (auto a : kept.records()[0]) {
        println("{} {}", a.key(), int(a.value().type()));
    }
    println("{}", (*kept.records()[0].begin()).value().type() == slog::value::kind::null);
}
```

Output:

```text
n 0
b 1
u 3
s 5
g 8
true
```

## See also

- [type](value/type.md)
- [sgcl::slog::value](value.md)
