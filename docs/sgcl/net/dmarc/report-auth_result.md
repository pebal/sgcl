[sgcl](../../README.md) › [net](../README.md) › [dmarc](README.md) › [report](report/README.md) › auth_result

# sgcl::net::dmarc::report::auth_result

```cpp
#include "sgcl/net/dmarc.h"   // or "sgcl/net/smtp.h"

namespace sgcl::net::dmarc {
    struct report {
        struct auth_result {
            string domain;
            string selector;
            string scope;
            string result;
        };
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::dmarc::report::auth_result` is one result of SPF or DKIM in a report's [row](report-row.md), as the
receiver saw it before DMARC's alignment.

## Member objects

| Member | Description |
|---|---|
| `domain` | the domain checked: a signature's `d=`, SPF's domain |
| `selector` | a DKIM signature's selector; empty for SPF |
| `scope` | SPF's scope, `"mfrom"` or `"helo"`; empty for DKIM |
| `result` | the result as the receiver writes it: `"pass"`, `"fail"`, `"softfail"`, `"temperror"`, ... |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"

using namespace sgcl;

int main() {
    string xml = "<feedback><report_metadata><date_range><begin>0</begin><end>1</end></date_range></report_metadata>"
                 "<policy_published><domain>example.com</domain><p>none</p></policy_published>"
                 "<record><row><source_ip>192.0.2.1</source_ip><count>1</count></row>"
                 "<auth_results><dkim><domain>example.com</domain><selector>s1</selector><result>pass</result></dkim>"
                 "<spf><domain>example.com</domain><scope>mfrom</scope><result>softfail</result></spf>"
                 "</auth_results></record></feedback>";
    auto r = net::dmarc::report::parse(xml);
    auto& row = r->rows[0];
    println("{} {} {}", row.dkim_results[0].domain, row.dkim_results[0].selector, row.dkim_results[0].result);
    println("{} {} {}", row.spf_results[0].domain, row.spf_results[0].scope, row.spf_results[0].result);
}
```

Output:

```text
example.com s1 pass
example.com mfrom softfail
```

## See also

- [row](report-row.md)
- [dmarc](README.md)
