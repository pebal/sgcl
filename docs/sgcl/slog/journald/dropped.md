[sgcl](../../README.md) › [slog](../README.md) › [journald](README.md)

# sgcl::slog::journald::dropped

```cpp
uint64_t dropped() const noexcept;
```

Returns the entries whose send failed: the journal not taking them, an entry too large for a datagram.

## Parameters

None.

## Return value

The count.

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
    auto journal = slog::journald::open();
    if (journal) {
        println("{}", journal->dropped());
    }
}
```

## See also

- [handle](handle.md)
- [sgcl::slog::journald](README.md)
