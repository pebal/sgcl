[sgcl](../../README.md) › [net](../README.md) › spf

# sgcl::net::spf

```cpp
#include "sgcl/net/spf.h"   // namespace sgcl::net::spf; or "sgcl/net/smtp.h"
```

The Sender Policy Framework (RFC 7208): whether a domain lets a host send its mail, read from the `v=spf1` TXT
record the domain publishes — the host's address against the record's mechanisms, the first that matches deciding
with its qualifier. Go and Python have no SPF in their standard libraries. [check](check.md) is RFC 7208's
`check_host()` for the sender of a message, given the client's address, MAIL FROM and HELO; its [result](result.md)
says what decided and why. An [smtp::server](../smtp/server/README.md) makes the check for each message when its
`sender_checks` is set.

Every mechanism (`all`, `include`, `a`, `mx`, `ptr`, `ip4`, `ip6`, `exists`) with its qualifiers and CIDR lengths,
both modifiers (`redirect`, `exp`), unknown modifiers passed over, the macros of §7 with their transformers and
URL escaping, and the limits of §4.6.4: ten terms that ask DNS, two lookups that find nothing, ten MX and PTR names,
twenty seconds for the whole check.

## The rules

1. A check never fails: what went wrong is its [status](status.md) — `temperror` for a DNS failure or a check past
   its time, `permerror` for a record that cannot be read, more than one record, or a limit passed.
2. The records are asked of DNS through the module's own resolver ([dns::options](../dns-options.md) in
   [options](options.md); its servers empty: `/etc/resolv.conf`'s), every name asked as a full name, with no cache.
3. The null sender (a bounce, MAIL FROM:<>) is checked as `postmaster@` its HELO name (§2.4); HELO alone is checked
   by giving the HELO name as the sender.

## Functions

| Function | Header | Description |
|---|---|---|
| [check, async_check](check.md) | `spf.h` | RFC 7208's `check_host()` for a sender |
| [to_string](to_string.md) | `spf.h` | a status as Received-SPF and Authentication-Results write it |

## Classes

| Class | Header | Description |
|---|---|---|
| [options](options.md) | `spf.h` | the resolver, the time, the limits, the receiver's name |
| [result](result.md) | `spf.h` | a check's outcome: the status, the domain, the term that decided, the explanation |

## Enumerations

| Enumeration | Header | Description |
|---|---|---|
| [status](status.md) | `spf.h` | a check's result as RFC 7208 §2.6 names it |

## See also

- [dkim](../dkim/README.md), [dmarc](../dmarc/README.md): the other checks of a message's origin
- [smtp::check_sender](../smtp/check_sender.md): the three checks of a message at once
- RFC 7208; `tests/net/mail/spf.cpp` (RFC 7208 §7.4's macro examples, every mechanism against a zone on the loopback)
