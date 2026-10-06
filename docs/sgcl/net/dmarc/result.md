[sgcl](../../README.md) › [net](../README.md) › [dmarc](README.md) › result

# sgcl::net::dmarc::result

```cpp
#include "sgcl/net/dmarc.h"   // or "sgcl/net/smtp.h"

namespace sgcl::net::dmarc {
    struct result {
        dmarc::status status = dmarc::status::none;
        string domain;
        string record_domain;
        optional<dmarc::record> record;
        bool dkim_aligned = false;
        bool spf_aligned = false;
        dmarc::policy policy = dmarc::policy::none;
        dmarc::policy disposition = dmarc::policy::none;
        string reason;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::dmarc::result` is a check's outcome, what [check](check.md) gives: whether the message passed, by which
record and why, and what the record asks to do with it. A receiver acts on `disposition`; `policy` is what a report
says was published.

## Member objects

| Member | Description |
|---|---|
| `status` | `pass`, `fail`, `none` (no record), `temperror`, `permerror` |
| `domain` | From's domain, in lower case |
| `record_domain` | where the record was found: the domain or its organizational domain |
| `record` | the [record](record/README.md); none when there is none |
| `dkim_aligned` | a DKIM signature passed for a domain aligned with From's |
| `spf_aligned` | SPF passed for a domain aligned with From's |
| `policy` | the record's policy for this name: `p=` for the domain itself, `sp=` (else `p=`) for a subdomain, `np=` for a name that does not exist |
| `disposition` | what to do with the message: `none` for a pass; the policy for a fail, one step milder (`reject` to `quarantine`, `quarantine` to `none`) when the message falls outside `pct=`'s sample |
| `reason` | why it did not pass, or why there is nothing to decide |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"

using namespace sgcl;

int main() {
    net::spf::result spf;
    net::dmarc::result r = net::dmarc::check("", spf, {});
    println("{}: {}", net::dmarc::to_string(r.status), r.reason);
}
```

Output:

```text
permerror: no valid From domain
```

## See also

- [check](check.md)
- [dmarc](README.md)
