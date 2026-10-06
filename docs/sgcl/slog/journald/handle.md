[sgcl](../../README.md) › [slog](../README.md) › [journald](README.md)

# sgcl::slog::journald::handle

```cpp
void handle(const record& r) const;
```

Sends the record as one entry of the journal: its fields in one datagram, from the calling thread. What a
[logger](../logger/README.md) calls for every record at its level.

## Parameters

| Parameter | Description |
|---|---|
| `r` | the record |

## Return value

None.

## Complexity

Linear in the record.

## Exceptions

None: a send that fails is counted in [dropped](dropped.md).

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    auto journal = slog::journald::open();
    if (!journal) {
        return 0;
    }
    slog::memory kept;
    slog::logger(kept).info("kept for later", "id", 7);
    journal->handle(kept.records()[0]);
    println("{}", journal->dropped());
}
```

## See also

- [open](open.md)
- [sgcl::slog::journald](README.md)
