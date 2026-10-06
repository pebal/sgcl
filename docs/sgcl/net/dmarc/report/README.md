[sgcl](../../../README.md) › [net](../../README.md) › [dmarc](../README.md)

# sgcl::net::dmarc::report

```cpp
#include "sgcl/net/dmarc.h"   // or "sgcl/net/smtp.h"

namespace sgcl::net::dmarc {
    struct report {
        struct auth_result;
        struct row;
        string org_name;
        string email;
        string report_id;
        int64_t begin = 0;
        int64_t end = 0;
        vector<string> errors;
        string domain;
        dmarc::record policy;
        vector<row> rows;
    };
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::dmarc::report` is an aggregate report (RFC 7489 §7.2, Appendix C), what a receiver mails to the
addresses of a record's `rua=` once a day: who sent it and for which period, the policy it saw published, and a
[row](../report-row.md) per source address with how many messages came from it, what was done with them and what SPF
and DKIM found. Reports come zipped or gzipped in mail ([compress](../../../compress/README.md) opens both);
[parse](parse.md) reads the XML inside. Making and sending reports is a service of its own and not here.

## Member objects

| Member | Description |
|---|---|
| `org_name`, `email`, `report_id` | the receiver that reports, its address, the report's id |
| `begin`, `end` | the period, in seconds since the epoch |
| `errors` | what the receiver says went wrong while it made the report |
| `domain` | the domain reported on |
| `policy` | the [record](../record/README.md) the receiver saw published: the policies, the alignment modes, the percentage, the failure options |
| `rows` | a [row](../report-row.md) per source |

## Member functions

| Function | Description |
|---|---|
| [parse](parse.md) | a report of its XML |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"

using namespace sgcl;

int main() {
    string xml = "<feedback><report_metadata><org_name>receiver.example</org_name><report_id>42</report_id>"
                 "<date_range><begin>1791158400</begin><end>1791244799</end></date_range></report_metadata>"
                 "<policy_published><domain>example.com</domain><p>reject</p></policy_published>"
                 "<record><row><source_ip>192.0.2.1</source_ip><count>12</count><policy_evaluated>"
                 "<disposition>none</disposition><dkim>pass</dkim><spf>pass</spf></policy_evaluated></row>"
                 "<identifiers><header_from>example.com</header_from></identifiers></record></feedback>";
    net::dmarc::report r = net::dmarc::report::parse(xml).value();
    println("{} on {}: {} rows", r.org_name, r.domain, r.rows.size());
    for (auto& row : r.rows) {
        println("{} {} dkim={} spf={}", row.source, row.count, row.dkim, row.spf);
    }
}
```

Output:

```text
receiver.example on example.com: 1 rows
192.0.2.1 12 dkim=pass spf=pass
```

## See also

- [row](../report-row.md), [auth_result](../report-auth_result.md)
- [record](../record/README.md)
- [dmarc](../README.md)
