[sgcl](../../README.md) › [slog](../README.md) › [record](../record.md)

# sgcl::slog::record::size

```cpp
size_t size() const noexcept;
```

Returns the number of attributes at the top of the record's tree: a group counts as one, whatever it holds.

## Parameters

None.

## Return value

The number of attributes at the top.

## Complexity

Linear in that number: the attributes are counted.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::memory kept;
    slog::logger log(kept);
    log.info("flat", "a", 1, "b", 2);
    log.group("g").info("grouped", "a", 1, "b", 2);
    for (const auto& r : kept.records()) {
        println("{} {}", r.message(), r.size());
    }
}
```

Output:

```text
flat 2
grouped 1
```

## See also

- [empty](empty.md), [begin](begin.md)
- [sgcl::slog::record](../record.md)
