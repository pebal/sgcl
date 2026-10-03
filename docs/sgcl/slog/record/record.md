[sgcl](../../README.md) › [slog](../README.md) › [record](../record.md)

# sgcl::slog::record::record

```cpp
record() noexcept = default;
```

Constructs an empty record: the time of the epoch in the local zone, level `info`, an empty message, no source and
no attributes. A logger makes the records a handler gets; this one is for a test or a placeholder.

## Parameters

None.

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
    slog::record r;
    println("{} {} {} {}", r.level() == slog::level::info, r.message().size(), r.has_source(),
            r.empty());
}
```

Output:

```text
true 0 false true
```

## See also

- [clone](clone.md)
- [sgcl::slog::record](../record.md)
