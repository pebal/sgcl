[sgcl](../../README.md) › [net](../README.md) › [spf](README.md)

# sgcl::net::spf::check, async_check

```cpp
result check(const ip_address& client, const string& sender, const string& helo, const options& o = {});
async::task<result> async_check(ip_address client, string sender, string helo, options o = {}) noexcept;
```

RFC 7208's `check_host()` for a message's sender: the `v=spf1` record of the sender's domain read, its terms
evaluated in order against the client's address, the first that matches deciding with its qualifier (`+` pass, `-`
fail, `~` softfail, `?` neutral), `redirect=` followed when none does, `neutral` when nothing decides. `include:`
evaluates another domain's record, its pass a match; a fail's explanation is read from `exp=`'s TXT record.

The null sender (`""`) is checked as `postmaster@` the HELO name (§2.4), a sender without a local part as
`postmaster@` its domain; HELO by itself (§2.3) by giving the HELO name as the sender. An IPv4 address mapped into
IPv6 is checked as IPv4. A domain that is no name, or of one label, is `none` with no lookup.

`check` waits on the calling thread; a task awaits `async_check`.

## Parameters

| Parameter | Description |
|---|---|
| `client` | the address of the host that sends: the SMTP client's |
| `sender` | MAIL FROM's address, `"user@example.com"`; `""` for the null sender |
| `helo` | the HELO or EHLO name, for the null sender and for `%{h}` |
| `o` | the resolver, the time, the limits, the receiver's name for `%{r}` |

## Return value

The [result](result.md); its status `temperror` or `permerror` for what went wrong.

## Complexity

A TXT lookup for each record read, and the lookups of its terms: at most ten terms that ask DNS (`include`, `a`,
`mx`, `ptr`, `exists`, `redirect`), each `mx` and `ptr` at most ten names more.

## Exceptions

- `check`: `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- `async_check`: none.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::ip_address client("192.0.2.25");
    net::spf::result r = net::spf::check(client, "alice@example.com", "mail.example.com");
    println("{} for {} ({})", net::spf::to_string(r.status), r.domain, r.mechanism);
}
```

Sample output:

```text
fail for example.com (-all)
```

## See also

- [result](result.md), [options](options.md)
- [smtp::check_sender](../smtp/check_sender.md)
- [spf](README.md)
