[sgcl](../../README.md) › [net](../README.md) › [spf](README.md)

# sgcl::net::spf::status

```cpp
#include "sgcl/net/spf.h"   // or "sgcl/net/smtp.h"

namespace sgcl::net::spf {
    enum class status : uint8_t { none, neutral, pass, fail, softfail, temperror, permerror };
}
```

A check's result as RFC 7208 §2.6 names it; [to_string](to_string.md) writes it as Received-SPF and
Authentication-Results do. What a receiver does with each is its own policy: RFC 7208 §8 says what the results
mean, not what to do.

| Value | Description |
|---|---|
| `none` | no record, or no domain to check |
| `neutral` | the domain says nothing of the host: `?`, or no term matched |
| `pass` | the host may send for the domain |
| `fail` | it may not: `-` |
| `softfail` | probably not: `~`, a domain still testing its record |
| `temperror` | DNS failed for now, or the check took too long: try again later |
| `permerror` | the record cannot be read, there is more than one, or a limit was passed |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    auto r = net::spf::check(net::ip_address("192.0.2.25"), "a@bad..example", "h");
    println("{}", r.status == net::spf::status::none);
}
```

Output:

```text
true
```

## See also

- [result](result.md)
- [to_string](to_string.md)
- [spf](README.md)
