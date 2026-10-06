[sgcl](../../README.md) › [net](../README.md) › [ntp](README.md)

# sgcl::net::ntp::errc

```cpp
#include "sgcl/net/ntp.h"

namespace sgcl::net::ntp {
    enum class errc {
        kiss_of_death = 1,
        unsynchronized,
        malformed
    };
}
```

The refusals of a server's answer, in the category `"ntp"` ([category](category.md)).

| Value | Description |
|---|---|
| `kiss_of_death` | "kiss-o'-death": stratum 0 with a code, the path: DENY or RSTR (go away), RATE (ask less often) |
| `unsynchronized` | "the server is not synchronized": leap 3, or a stratum past 15 |
| `malformed` | "malformed NTP packet": not a server's answer, shorter than 48 bytes |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ntp.h"

using namespace sgcl;

int main() {
    error_code e = net::ntp::errc::kiss_of_death;
    println("{}", e.message());
}
```

Output:

```text
kiss-o'-death
```

## See also

- [query](query.md)
- [category](category.md)
