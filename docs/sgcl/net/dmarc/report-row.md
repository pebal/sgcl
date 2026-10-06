[sgcl](../../README.md) › [net](../README.md) › [dmarc](README.md) › [report](report/README.md) › row

# sgcl::net::dmarc::report::row

```cpp
#include "sgcl/net/dmarc.h"   // or "sgcl/net/smtp.h"

namespace sgcl::net::dmarc {
    struct report {
        struct row {
            ip_address source;
            uint64_t count = 0;
            dmarc::policy disposition = dmarc::policy::none;
            string dkim;
            string spf;
            vector<string> reasons;
            string header_from;
            string envelope_from;
            string envelope_to;
            vector<auth_result> dkim_results;
            vector<auth_result> spf_results;
        };
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::dmarc::report::row` is one record of an aggregate [report](report/README.md): a source address, how many
messages came from it with the same identifiers, what the receiver did with them and the DMARC results it evaluated,
and the SPF and DKIM results under them.

## Member objects

| Member | Description |
|---|---|
| `source` | `source_ip`: the sending host |
| `count` | the messages |
| `disposition` | what was done: `none`, `quarantine`, `reject` |
| `dkim`, `spf` | `policy_evaluated`: DMARC's view of each, `"pass"` or `"fail"` (aligned and passed, or not) |
| `reasons` | the types of the overrides the receiver applied (`"forwarded"`, `"mailing_list"`, `"local_policy"`, ...) |
| `header_from`, `envelope_from`, `envelope_to` | the identifiers |
| `dkim_results` | each signature's [auth_result](report-auth_result.md): domain, selector, result |
| `spf_results` | SPF's: domain, scope (`"mfrom"`, `"helo"`), result |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"

using namespace sgcl;

int main() {
    string xml = "<feedback><report_metadata><date_range><begin>0</begin><end>1</end></date_range></report_metadata>"
                 "<policy_published><domain>example.com</domain><p>none</p></policy_published>"
                 "<record><row><source_ip>198.51.100.7</source_ip><count>3</count><policy_evaluated>"
                 "<disposition>none</disposition><dkim>fail</dkim><spf>fail</spf>"
                 "<reason><type>forwarded</type></reason></policy_evaluated></row>"
                 "<identifiers><header_from>example.com</header_from></identifiers></record></feedback>";
    auto r = net::dmarc::report::parse(xml);
    net::dmarc::report::row row = r->rows[0];
    println("{} {} {}", row.source, row.count, row.reasons[0]);
}
```

Output:

```text
198.51.100.7 3 forwarded
```

## See also

- [report](report/README.md), [auth_result](report-auth_result.md)
- [dmarc](README.md)
