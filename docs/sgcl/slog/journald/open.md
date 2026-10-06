[sgcl](../../README.md) › [slog](../README.md) › [journald](README.md)

# sgcl::slog::journald::open

```cpp
static expected<journald, io::error> open(const string& identifier = {}) noexcept;
```

Connects a datagram socket to `/run/systemd/journal/socket`. The entries carry `identifier` as their
`SYSLOG_IDENTIFIER`, the program's name when it is empty.

## Parameters

| Parameter | Description |
|---|---|
| `identifier` | the name of the program in the journal |

## Return value

The handler, or the `io::error` of the connect (no journal: `is_not_found()`).

## Complexity

A socket and a connect.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    auto journal = slog::journald::open();
    println("{}", journal ? "the journal" : "no journal");
}
```

## See also

- [handle](handle.md)
- [sgcl::slog::journald](README.md)
