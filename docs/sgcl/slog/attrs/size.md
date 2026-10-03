[sgcl](../../README.md) › [slog](../README.md) › [attrs](README.md)

# sgcl::slog::attrs::size

```cpp
size_t size() const noexcept;
```

Returns the number of attributes in the range: a group among them counts as one, whatever it holds. They are walked
and counted, not stored: the attributes of a call stand in a logger's list in the place of one entry.

## Parameters

None.

## Return value

The number of attributes.

## Complexity

Linear in the number of attributes.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::memory kept;
    slog::logger(kept).group("req").info("m", "a", 1, "b", 2, slog::group("c", "d", 3));
    println("{}", (*kept.records()[0].begin()).value().as_group().size());
}
```

Output:

```text
3
```

## See also

- [empty](empty.md)
- [sgcl::slog::attrs](README.md)
