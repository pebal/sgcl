[sgcl](../../README.md) › [slog](../README.md) › [memory](README.md)

# sgcl::slog::memory::clear

```cpp
void clear() const noexcept;
```

Drops the records kept, for every copy of the keeper; a vector taken by [records](records.md) before keeps its own.
The method is `const`: the records are the shared state, not the handle.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the number of records kept.

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
    log.info("one");
    auto before = kept.records();
    kept.clear();
    log.info("two");
    println("{} {} {}", before.size(), kept.size(), kept.records()[0].message());
}
```

Output:

```text
1 1 two
```

## See also

- [records](records.md), [size](size.md)
- [sgcl::slog::memory](README.md)
