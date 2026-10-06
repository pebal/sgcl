[sgcl](../../../README.md) › [net](../../README.md) › [dmarc](../README.md)

# sgcl::net::dmarc::record

```cpp
#include "sgcl/net/dmarc.h"   // or "sgcl/net/smtp.h"

namespace sgcl::net::dmarc {
    struct record;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::dmarc::record` is a DMARC record (RFC 7489 §6.3): the policy for a domain's failing mail, its
subdomains' and its non-existent names' when they differ, how strictly SPF's and DKIM's domains must agree with
From's, the share of failing mail the policy is applied to, and where reports go. A plain value of the record's
fields: [parse](parse.md) reads one as RFC 7489 §6.6.3 has it read, [to_string](to_string.md) writes one to publish.

## Member objects

| Member | Description |
|---|---|
| `policy` | `p=`, the domain's [policy](../policy.md); `none` by default |
| `subdomain_policy` | `sp=`, the subdomains' policy; none by default: `policy` |
| `nonexistent_policy` | `np=` (RFC 9091), the policy of names under the domain that do not exist; none by default: `subdomain_policy`, then `policy` |
| `strict_dkim` | `adkim=s`: a DKIM signature's `d=` must be From's domain itself, not only of its organization; `false` by default |
| `strict_spf` | `aspf=s`: the same for SPF's domain; `false` by default |
| `percent` | `pct=`, the percentage of failing mail the policy is applied to; 100 by default |
| `failure_options` | `fo=`, when failure reports are asked for (`"0"`, `"1"`, `"d"`, `"s"`, joined by `:`); `"0"` by default |
| `aggregate_reports` | `rua=`, where aggregate reports go (`"mailto:dmarc@example.com"`), in their order |
| `failure_reports` | `ruf=`, where failure reports go |
| `report_interval` | `ri=`, the time between aggregate reports, in whole seconds; 24 hours by default |
| `report_format` | `rf=`, the format of failure reports; `"afrf"` by default |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](record.md) | a record of the defaults, or of a literal's text |
| [parse](parse.md) | a record of a TXT record's text |
| [to_string](to_string.md) | the record's text, the tags at their defaults left out |
| [operator==](operator_cmp.md) | whether two records say the same |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"

using namespace sgcl;

int main() {
    net::dmarc::record r;
    r.policy = net::dmarc::policy::quarantine;
    r.percent = 25;
    r.aggregate_reports.push_back("mailto:dmarc@example.com");
    println("_dmarc.example.com. IN TXT \"{}\"", r.to_string());
}
```

Output:

```text
_dmarc.example.com. IN TXT "v=DMARC1; p=quarantine; pct=25; rua=mailto:dmarc@example.com"
```

## See also

- [lookup](../lookup.md), [check](../check.md)
- [dmarc](../README.md)
