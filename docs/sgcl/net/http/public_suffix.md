[sgcl](../../README.md) › [net](../README.md) › [http](README.md)

# sgcl::net::http::public_suffix

```cpp
#include "sgcl/net/http/public_suffix.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    string public_suffix(const string& host, suffix_rules rules = suffix_rules::all) noexcept;
}
```

The public suffix of `host`, the part under which anyone may register a name of their own: `co.uk` for
`www.example.co.uk`, `github.io` for `me.github.io`, `com` for `com` itself, Go's `publicsuffix.PublicSuffix`. A
[cookie_jar](cookie_jar/README.md) refuses a cookie set for one; a program groups hosts by the site they belong to
with [registrable_domain](registrable_domain.md).

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

The suffix, in lower case and A-labels; `""` for no host.

## Complexity

Linear in the length of the host: a lookup of a hash table for each of its labels from the last, ending at the first
the list has no key for.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    for (const char* host : {"www.example.co.uk", "me.github.io", "a.b.c.mm", "www.city.kobe.jp", "Bücher.DE",
                             "localhost", "127.0.0.1"}) {
        println("{}: [{}]", host, net::http::public_suffix(host));
    }
    println("{}", net::http::public_suffix("me.github.io", net::http::suffix_rules::icann));
}
```

Output:

```text
www.example.co.uk: [co.uk]
me.github.io: [github.io]
a.b.c.mm: [c.mm]
www.city.kobe.jp: [kobe.jp]
Bücher.DE: [de]
localhost: [localhost]
127.0.0.1: []
io
```

## See also

- [registrable_domain](registrable_domain.md): the suffix and the label before it
- [is_public_suffix](is_public_suffix.md): whether a host is one
- [cookie_jar](cookie_jar/README.md): what refuses a cookie of a public suffix
- [sgcl::net::http](README.md)
