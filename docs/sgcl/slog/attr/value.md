[sgcl](../../README.md) › [slog](../README.md) › [attr](../attr.md)

# sgcl::slog::attr::value

```cpp
slog::value value() const noexcept;
```

Returns the value of the attribute, a view of the record's ([value](../value.md)).

## Parameters

None.

## Return value

The value.

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
    slog::logger(kept).info("m", "took", 1500 * millisecond);
    auto v = (*kept.records()[0].begin()).value();
    println("{} {} {}", v.type() == slog::value::kind::duration, v.text(), v.json());
}
```

Output:

```text
true 1.5s 1500000000
```

## See also

- [key](key.md)
- [sgcl::slog::value](../value.md)
- [sgcl::slog::attr](../attr.md)
