[sgcl](../../README.md) › [net](../README.md) › [http](README.md)

# sgcl::net::http::suffix_rules

```cpp
#include "sgcl/net/http/public_suffix.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    enum class suffix_rules : uint8_t {
        all,
        icann,
    };
}
```

Which sections of the Public Suffix List a lookup ([public_suffix](public_suffix.md),
[registrable_domain](registrable_domain.md), [is_public_suffix](is_public_suffix.md)) takes. The ICANN section is
the suffixes of the domain name registries (`com`, `co.uk`, `kobe.jp`); the private one, domains whose owners ask that
their subdomains be kept apart, a hosting service's or a CDN's (`github.io`, `blogspot.com`). Browsers use both, and
so does a [cookie_jar](cookie_jar/README.md); the ICANN section alone answers who registered a name, Go's `icann`
flag.

| Value | Description |
|---|---|
| `all` | both sections, the default |
| `icann` | the ICANN section alone |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    println("{}", net::http::registrable_domain("me.github.io"));
    println("{}", net::http::registrable_domain("me.github.io", net::http::suffix_rules::icann));
}
```

Output:

```text
me.github.io
github.io
```

## See also

- [public_suffix](public_suffix.md)
- [sgcl::net::http](README.md)
