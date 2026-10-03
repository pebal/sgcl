[sgcl](../../README.md) › [slog](../README.md) › [memory](../memory.md)

# sgcl::slog::memory::size

```cpp
size_t size() const noexcept;
```

Returns the number of records kept so far.

## Parameters

None.

## Return value

The number of records.

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
    slog::logger log(kept);
    log.debug("below the level");
    log.info("one");
    log.error("two");
    println("{}", kept.size());
}
```

Output:

```text
2
```

## See also

- [records](records.md), [clear](clear.md)
- [sgcl::slog::memory](../memory.md)
