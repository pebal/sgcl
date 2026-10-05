[sgcl](../../README.md) › [net](../README.md) › [http](README.md)

# sgcl::net::http::registrable_domain

```cpp
#include "sgcl/net/http/public_suffix.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    string registrable_domain(const string& host, suffix_rules rules = suffix_rules::all) noexcept;
}
```

The registrable domain of `host`, "eTLD+1", what browsers call its site: the [public suffix](public_suffix.md) and
the label before it, `example.co.uk` for `www.example.co.uk`, Go's `publicsuffix.EffectiveTLDPlusOne`. Two hosts of
one registrable domain belong to one owner; a [cookie_jar](cookie_jar/README.md) counts its limit per registrable
domain.

The list is the [Public Suffix List](https://publicsuffix.org) of 2024-01-07, embedded in the library (generated
by `tools/gen_public_suffix.py` from `tools/public_suffix_list.dat`; Mozilla Public License 2.0, see `NOTICE`), its
rules with `*` wildcards and `!` exceptions, of both its sections by default: the ICANN one, and the private one,
domains whose owners ask that their subdomains be kept apart (`github.io`, `blogspot.com`), which browsers use too.
A host is looked up by the list's algorithm: the matching rule of the most labels, an exception before any, `*` (the
last label) when none matches. A newer list is compiled by giving the generator its file.

The host is taken as a [url](../url/README.md) gives it, or as a person writes it: ASCII in any case, compared in
lower case, or Unicode, converted to A-labels by IDNA (UTS #46) first; a dot at its end is left out. An IP address, an
empty name, a name with an empty label or one IDNA refuses has none.

## Parameters

| Parameter | Description |
|---|---|
| `host` | a host name |
| `rules` | the sections of the list: both, or the ICANN one alone ([suffix_rules](suffix_rules.md)) |

## Return value

The registrable domain, in lower case and A-labels; `""` when the host is a public suffix itself, or for no host
(where Go's returns an error).

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
    for (const char* host : {"www.example.co.uk", "me.github.io", "www.city.kobe.jp", "shop.食狮.中国", "co.uk"}) {
        println("{}: [{}]", host, net::http::registrable_domain(host));
    }
    println("{}", net::http::registrable_domain("me.github.io", net::http::suffix_rules::icann));
}
```

Output:

```text
www.example.co.uk: [example.co.uk]
me.github.io: [me.github.io]
www.city.kobe.jp: [city.kobe.jp]
shop.食狮.中国: [xn--85x722f.xn--fiqs8s]
co.uk: []
github.io
```

## See also

- [public_suffix](public_suffix.md): the suffix alone
- [sgcl::net::http](README.md)
