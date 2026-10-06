[sgcl](../../../README.md) › [net](../../README.md) › [dmarc](../README.md) › [record](README.md)

# sgcl::net::dmarc::record::parse

```cpp
static expected<record, io::error> parse(const string& text);
```

A record of a TXT record's text, read as RFC 7489 §6.3 and §6.6.3 have it read: the tag list must start with
`v=DMARC1`; `p=` must name a policy, but a record with `rua=` and no valid `p=` is taken as `p=none`; a bad value of
another tag leaves that tag's default, and tags the RFC does not name are passed over, so that a record a later
specification extends still reads.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the record's text, `"v=DMARC1; p=reject; rua=mailto:d@example.com"` |

## Return value

The record; `net::errc::malformed_dmarc` for a text that does not start with `v=DMARC1`, is no tag list (a tag
twice, a tag without `=`), or has no valid `p=` and no `rua=`.

## Complexity

Linear in the text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"

using namespace sgcl;

int main() {
    auto r = net::dmarc::record::parse("v=DMARC1; p=bogus; pct=150; rua=mailto:d@example.com");
    println("{} {}", net::dmarc::to_string(r->policy), r->percent);
    println("{}", net::dmarc::record::parse("v=DMARC1; p=bogus").error().message());
}
```

Output:

```text
none 100
dmarc no valid p= tag: malformed DMARC record
```

## See also

- [to_string](to_string.md), [(constructor)](record.md)
- [record](README.md)
