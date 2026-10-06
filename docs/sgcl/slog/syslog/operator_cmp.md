[sgcl](../../README.md) › [slog](../README.md) › [syslog](README.md)

# sgcl::slog::operator==, operator!= (sgcl::slog::syslog)

```cpp
friend bool operator==(const syslog& a, const syslog& b) noexcept;
```

Checks whether two handles stand for the same handler. The `!=` is the one C++ writes from this `==`.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the handles to compare |

## Return value

`true` when `a` and `b` are the same handler.

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
    io::buffer out;
    slog::syslog a(out, {.app_name = "a", .hostname = "h"});
    slog::syslog b = a;
    println("{}", a == b);
}
```

Output:

```text
true
```

## See also

- [(constructor)](syslog.md)
- [sgcl::slog::syslog](README.md)
