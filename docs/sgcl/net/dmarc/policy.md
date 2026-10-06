[sgcl](../../README.md) › [net](../README.md) › [dmarc](README.md)

# sgcl::net::dmarc::policy

```cpp
#include "sgcl/net/dmarc.h"   // or "sgcl/net/smtp.h"

namespace sgcl::net::dmarc {
    enum class policy : uint8_t { none, quarantine, reject };
}
```

What a [record](record/README.md) asks a receiver to do with mail that fails: its `policy`, `subdomain_policy` and `nonexistent_policy` (`p=`, `sp=`, `np=`); and what a
[result](result.md)'s disposition says to do with one message.

| Value | Description |
|---|---|
| `none` | nothing: the domain monitors, its reports tell it who sends in its name |
| `quarantine` | treat the message as suspicious: a spam folder, a closer look |
| `reject` | refuse it: 550 at the end of DATA |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"

using namespace sgcl;

int main() {
    net::dmarc::record r("v=DMARC1; p=quarantine; sp=reject");
    println("{} {}", net::dmarc::to_string(r.policy), r.subdomain_policy == net::dmarc::policy::reject);
}
```

Output:

```text
quarantine true
```

## See also

- [record](record/README.md), [result](result.md)
- [dmarc](README.md)
