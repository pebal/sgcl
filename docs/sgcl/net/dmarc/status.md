[sgcl](../../README.md) › [net](../README.md) › [dmarc](README.md)

# sgcl::net::dmarc::status

```cpp
#include "sgcl/net/dmarc.h"   // or "sgcl/net/smtp.h"

namespace sgcl::net::dmarc {
    enum class status : uint8_t { none, pass, fail, temperror, permerror };
}
```

A check's result, as RFC 8601 names DMARC's in an Authentication-Results field; [to_string](to_string.md) writes it.

| Value | Description |
|---|---|
| `none` | the domain has no record (or more than one, or none that parses) |
| `pass` | SPF or DKIM passed for a domain aligned with From's |
| `fail` | neither did: the record's policy applies |
| `temperror` | DNS failed for now |
| `permerror` | no domain to check: no From, more than one From, or From's addresses of more than one domain |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"

using namespace sgcl;

int main() {
    net::spf::result spf;
    println("{}", net::dmarc::check("bad..domain", spf, {}).status == net::dmarc::status::permerror);
}
```

Output:

```text
true
```

## See also

- [result](result.md)
- [dmarc](README.md)
