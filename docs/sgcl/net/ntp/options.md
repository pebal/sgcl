[sgcl](../../README.md) › [net](../README.md) › [ntp](README.md)

# sgcl::net::ntp::options

```cpp
#include "sgcl/net/ntp.h"

namespace sgcl::net::ntp {
    struct options {
        duration timeout = std::chrono::seconds(5);
        async::stop_token stop;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::ntp::options` is how a [query](query.md) waits.

## Member objects

| Member | Description |
|---|---|
| `timeout` | the answer waited for at most; 5 s by default; zero: none |
| `stop` | ends the wait with `ECANCELED` |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ntp.h"

using namespace sgcl;

int main() {
    net::ntp::options o;
    o.timeout = std::chrono::milliseconds(500);
    println("{}", bool(net::ntp::query("pool.ntp.org", o)));
}
```

Output:

```text
true
```

## See also

- [query](query.md)
