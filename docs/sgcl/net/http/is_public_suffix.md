[sgcl](../../README.md) › [net](../README.md) › [http](README.md)

# sgcl::net::http::is_public_suffix

```cpp
#include "sgcl/net/http/public_suffix.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    bool is_public_suffix(const string& host, suffix_rules rules = suffix_rules::all) noexcept;
}
```

Whether `host` is a public suffix, its own [public_suffix](public_suffix.md): `co.uk`, `github.io`, `com`, and any
name of one label (the list's rule `*`), `localhost` among them. A [cookie_jar](cookie_jar/README.md) asks this of a
cookie's `Domain`.

The host is taken as a [url](../url/README.md) gives it, or as a person writes it: ASCII in any case, compared in
lower case, or Unicode, converted to A-labels by IDNA (UTS #46) first; a dot at its end is left out. An IP address, an
empty name, a name with an empty label or one IDNA refuses has none.

## Parameters

| Parameter | Description |
|---|---|
| `host` | a host name |
| `rules` | the sections of the list: both, or the ICANN one alone ([suffix_rules](suffix_rules.md)) |

## Return value

`true` when the host is a public suffix; `false` for a name under one, or no host.

## Complexity

Linear in the length of the host.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    for (const char* host : {"co.uk", "example.co.uk", "github.io", "kobe.jp", "city.kobe.jp", "intranet"}) {
        println("{} {}", host, net::http::is_public_suffix(host));
    }
    println("{}", net::http::is_public_suffix("github.io", net::http::suffix_rules::icann));
}
```

Output:

```text
co.uk true
example.co.uk false
github.io true
kobe.jp false
city.kobe.jp false
intranet true
false
```

## See also

- [public_suffix](public_suffix.md): the suffix of a host
- [sgcl::net::http](README.md)
