[sgcl](../../README.md) › [net](../README.md) › [dmarc](README.md)

# sgcl::net::dmarc::check, async_check

```cpp
result check(const string& from_domain, const spf::result& spf, const vector<dkim::result>& dkim,    // (1)
             const options& o = {});
async::task<result> async_check(string from_domain, spf::result spf, vector<dkim::result> dkim,      // (2)
                                options o = {}) noexcept;
```

DMARC for a message whose From is of `from_domain` (RFC 7489 §6.6): the record discovered — `_dmarc.<domain>`,
else `_dmarc.<organizational domain>` — the identifiers aligned with From's domain (SPF's domain when SPF passed,
each DKIM signature's `d=` that passed; relaxed: the same organizational domain, strict: the same name), the result
`pass` when either is aligned, `fail` when neither is. The [result](result.md) says which record, which alignment,
the policy for the name and what to do with the message.

`check` waits on the calling thread; a task awaits `async_check`.

## Parameters

| Parameter | Description |
|---|---|
| `from_domain` | the domain of the message's From |
| `spf` | what [spf::check](../spf/check.md) found for the message's sender |
| `dkim` | what [dkim::verify](../dkim/verify.md) found for its signatures |
| `o` | the resolver of the records |

## Return value

The [result](result.md); its status `none`, `temperror` or `permerror` when there is nothing to decide.

## Complexity

One or two TXT lookups; three more to learn whether a failing name exists when the record has `np=`.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"

using namespace sgcl;

int main() {
    net::spf::result spf;
    spf.status = net::spf::status::pass;
    spf.domain = "bounces.example.net";
    net::dmarc::result r = net::dmarc::check("example.com", spf, {});
    println("{} {} {}", net::dmarc::to_string(r.status), r.spf_aligned, net::dmarc::to_string(r.disposition));
}
```

Sample output:

```text
fail false reject
```

## See also

- [result](result.md), [lookup](lookup.md)
- [smtp::check_sender](../smtp/check_sender.md): SPF, DKIM and DMARC of a message at once
- [dmarc](README.md)
