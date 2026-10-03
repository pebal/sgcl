[sgcl](../../README.md) › [slog](../README.md) › [memory](README.md)

# sgcl::slog::memory::memory

```cpp
memory() noexcept;
```

Constructs an empty keeper of records, with a state of its own that its copies share.

## Parameters

None.

## Complexity

Constant: one managed allocation.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::memory kept;
    slog::memory same = kept;
    slog::logger(same).info("logged through the copy");
    println("{} {}", kept.size(), same.size());
}
```

Output:

```text
1 1
```

## See also

- [records](records.md)
- [sgcl::slog::memory](README.md)
