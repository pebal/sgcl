[sgcl](../../../README.md) › [net](../../README.md) › [tls](../README.md)

# sgcl::net::tls::revocation_cache

```cpp
#include "sgcl/net/tls.h"

namespace sgcl::net::tls {
    class revocation_cache;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`net::tls::revocation_cache` is where the online checks of revocation ([revocation_mode](../revocation_mode.md)
`soft_fail` and `hard_fail`) keep what they fetched: the OCSP answer for a certificate, by its issuer and its serial
number, a CRL, by the URL of its distribution point, and a staple a server sent that verified, by its bytes, each
until its `nextUpdate` (an hour for one without), so that the next connection to the same server, or to another
server of the same CA, asks nothing again and verifies no signature twice. A client's
[config](../config.md) names one in `revocation_cache`; a config without one shares the process's own, of 1024
entries, made at the first check and kept for the process. Go has nothing of it, having no revocation checks;
the browsers and Java's `PKIXRevocationChecker` keep such a cache inside.

It is a handle of one word whose state is made in the constructor: copies, a config's among them, are the same
cache, and it is safe from many threads at once. A cache holds at most `capacity` entries, the oldest dropped
first; an entry past its time is dropped when it is next looked up.

## Rules

- A moved-from cache is the same cache, as a moved-from [tracked_ptr](../../../core/tracked_ptr/README.md) still
  points.
- Nothing of it waits: its calls take a mutex of its own for the time of a look over at most `capacity` entries.
- What it keeps is public: OCSP answers and CRLs are signed by their CA and given to anyone who asks.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](revocation_cache.md) | a cache of a capacity; a copy |
| [operator=](operator_assign.md) | makes this handle one of another's cache |

#### Capacity

| Function | Description |
|---|---|
| [capacity](capacity.md) | the entries it holds at most |
| [size](size.md) | the entries it holds now |

#### Modifiers

| Function | Description |
|---|---|
| [clear](clear.md) | drops every entry |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

int main() {
    net::tls::config cfg;
    cfg.revocation = net::tls::revocation_mode::soft_fail;
    cfg.revocation_cache = net::tls::revocation_cache(64);   // this client's own
    println("{} {}", cfg.revocation_cache->capacity(), cfg.revocation_cache->size());
}
```

Output:

```text
64 0
```

## See also

- [config](../config.md): `revocation_cache`, `revocation`
- [revocation_mode](../revocation_mode.md): the checks that fill it
- [net::tls](../README.md)
