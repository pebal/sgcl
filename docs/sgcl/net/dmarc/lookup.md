[sgcl](../../README.md) › [net](../README.md) › [dmarc](README.md)

# sgcl::net::dmarc::lookup, async_lookup

```cpp
expected<record, io::error> lookup(const string& domain, const options& o = {});
async::task<expected<record, io::error>> async_lookup(string domain, options o = {}) noexcept;
```

The [record](record/README.md) that applies to mail from the domain (RFC 7489 §6.6.3): the TXT record at
`_dmarc.<domain>` that starts with `v=DMARC1`, else the one at `_dmarc.<organizational domain>`. A name with more
than one such record has none, as the RFC says.

`lookup` waits on the calling thread; a task awaits `async_lookup`.

## Parameters

| Parameter | Description |
|---|---|
| `domain` | the domain of a From |
| `o` | the resolver of the records |

## Return value

The record; `net::errc::no_data` when there is none (or more than one), `net::errc::server_failure` when DNS failed,
`net::errc::malformed_dmarc` for one that does not [parse](record/parse.md), `net::errc::invalid_address` for no
domain.

## Complexity

One or two TXT lookups.

## Exceptions

- `lookup`: `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- `async_lookup`: none.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"

using namespace sgcl;

int main() {
    auto r = net::dmarc::lookup("www.example.com");
    if (r) {
        println("{}", r->to_string());
    } else {
        println("{}", r.error().message());
    }
}
```

Sample output:

```text
v=DMARC1; p=reject; sp=reject; adkim=s; aspf=s
```

## See also

- [record](record/README.md), [check](check.md)
- [dmarc](README.md)
