[sgcl](../../README.md) › [net](../README.md) › dmarc

# sgcl::net::dmarc

```cpp
#include "sgcl/net/dmarc.h"   // namespace sgcl::net::dmarc; or "sgcl/net/smtp.h"
```

Domain-based Message Authentication, Reporting and Conformance (RFC 7489): what the domain of a message's From asks
a receiver to do with mail that neither [SPF](../spf/README.md) nor [DKIM](../dkim/README.md) ties to it. A message
passes when SPF passed for a domain aligned with From's, or a DKIM signature verified for one; the domain's
[record](record/README.md), at `_dmarc.<domain>` or at its organizational domain's, says how strictly the domains
must agree and what to do with a message that fails — nothing, quarantine, reject. [check](check.md) makes that
decision from the results of the other two, [lookup](lookup.md) finds the record that applies to a domain, and an
aggregate [report](report/README.md) that receivers send back is read.

The organizational domain is the registrable domain of the Public Suffix List
([http::registrable_domain](../http/registrable_domain.md), both sections): `example.co.uk` for
`news.example.co.uk`. Besides RFC 7489's tags a record's `np=` is read (RFC 9091), the policy of names under the
domain that do not exist.

## The rules

1. A check never fails: what went wrong is its [status](status.md) — `temperror` for a DNS failure, `permerror` for
   a message without one From domain; `none` for a domain without a record, or with more than one.
2. The policy a check reports is the record's for the name (`p=` for the domain itself, `sp=` for its subdomains,
   `np=` for those that do not exist); the disposition is what to do with this message: the policy, one step milder
   for a failing message outside `pct=`'s sample, `none` for a pass.
3. The records are asked of DNS through the module's own resolver ([options](options.md); its servers empty:
   `/etc/resolv.conf`'s).

## Functions

| Function | Header | Description |
|---|---|---|
| [check, async_check](check.md) | `dmarc.h` | DMARC for a message of a From domain, given SPF's and DKIM's results |
| [lookup, async_lookup](lookup.md) | `dmarc.h` | the record that applies to mail from a domain |
| [to_string](to_string.md) | `dmarc.h` | a policy or a status as text |

## Classes

| Class | Header | Description |
|---|---|---|
| [options](options.md) | `dmarc.h` | the resolver of the records |
| [record](record/README.md) | `dmarc.h` | a DMARC record: the policies, the alignment modes, the sample, where reports go |
| [report](report/README.md) | `dmarc.h` | an aggregate report: who sent it, for which period, a row per source |
| [report::auth_result](report-auth_result.md) | `dmarc.h` | one result of SPF or DKIM in a report's row |
| [report::row](report-row.md) | `dmarc.h` | a source of a report and what was done with its messages |
| [result](result.md) | `dmarc.h` | a check's outcome: the status, the record, the alignment, the policy, the disposition |

## Enumerations

| Enumeration | Header | Description |
|---|---|---|
| [policy](policy.md) | `dmarc.h` | `none`, `quarantine`, `reject` |
| [status](status.md) | `dmarc.h` | a check's result as RFC 8601 names DMARC's |

## See also

- [spf](../spf/README.md), [dkim](../dkim/README.md)
- [smtp::check_sender](../smtp/check_sender.md): the three checks of a message at once
- RFC 7489, RFC 9091; `tests/net/mail/dmarc.cpp`
