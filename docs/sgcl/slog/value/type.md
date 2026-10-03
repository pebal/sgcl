[sgcl](../../README.md) › [slog](../README.md) › [value](../value.md)

# sgcl::slog::value::type

```cpp
kind type() const noexcept;
```

Returns the [kind](../value-kind.md) of the value, slog's `Value.Kind`: which `as_` accessor gives it. A string is `string` whatever made it (a literal, a `string`, a type's `write_text` or `to_text`), a type described by its fields is `group`, a container of the program `any`.

## Parameters

None.

## Return value

The kind.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::memory kept;
    slog::logger(kept).info("m", "port", 8080, "ratio", 0.5, "name", string("ala"));
    for (auto a : kept.records()[0]) {
        auto k = a.value().type();
        println("{} {} {}", a.key(), k == slog::value::kind::int64, k == slog::value::kind::string);
    }
}
```

Output:

```text
port true false
ratio false false
name false true
```

## See also

- [kind](../value-kind.md)
- [sgcl::slog::value](../value.md)
