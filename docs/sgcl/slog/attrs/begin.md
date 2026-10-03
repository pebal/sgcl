[sgcl](../../README.md) › [slog](../README.md) › [attrs](README.md)

# sgcl::slog::attrs::begin

```cpp
iterator begin() const noexcept;
```

Returns an [iterator](../attrs-iterator.md) to the first attribute: its `*` gives an [attr](../attr/README.md) by value, and
it compares equal to [end](end.md) past the last one. A `for` over the range walks them.

## Parameters

None.

## Return value

An iterator to the first attribute; equal to `end()` for a range with none.

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
    slog::logger(kept).info("m", slog::group("g", "x", 1, "y", 2));
    slog::attrs g = (*kept.records()[0].begin()).value().as_group();
    for (auto i = g.begin(); i != g.end(); ++i) {
        println("{}", (*i).key());
    }
}
```

Output:

```text
x
y
```

## See also

- [end](end.md), [size](size.md)
- [sgcl::slog::attrs](README.md)
