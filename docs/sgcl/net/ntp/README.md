[sgcl](../../README.md) › [net](../README.md) › ntp

# sgcl::net::ntp

```cpp
#include "sgcl/net/ntp.h"   // namespace sgcl::net::ntp
```

SNTP (RFC 4330, the client's part of NTPv4, RFC 5905): one [query](query.md) of a time server and what its answer
says of this machine's clock — the offset to add to it, the round trip's delay, the server's stratum and reference.
Go has no NTP client in its standard library; the module asks a server as `sntp` does and leaves the clock alone.

## The rules

1. A server is `"host[:123]"`; `pool.ntp.org` when none is named.
2. The query's transmit timestamp carries random low bits, and an answer is taken only when its originate timestamp
   is that one (RFC 5905 §9.1): a stray or a forged datagram is passed over.
3. A server that says to go away or to ask less often (stratum 0, a Kiss-o'-Death) is `errc::kiss_of_death`, its
   code ("RATE", "DENY", "RSTR") in the error's path; one whose clock is not set is `errc::unsynchronized`.
4. The timestamps are read in the era nearest this clock: the rollover of 2036 reads right.

## Functions

| Function | Header | Description |
|---|---|---|
| [category](category.md) | `ntp.h` | the error category of `errc`, named `"ntp"` |
| [make_error_code](make_error_code.md) | `ntp.h` | an `errc` as an `error_code` |
| [query, async_query](query.md) | `ntp.h` | a server's answer and this clock's offset |

## Classes

| Class | Header | Description |
|---|---|---|
| [options](options.md) | `ntp.h` | the timeout and the stop of a query |
| [response](response.md) | `ntp.h` | what a server answered: offset, delay, stratum, reference |

## Enumerations

| Enumeration | Header | Description |
|---|---|---|
| [errc](errc.md) | `ntp.h` | the refusals of a server's answer |

## See also

- [ping](../ping.md)
- [time::datetime](../../time/datetime/README.md)
- [Benchmarks](../benchmarks.md)
- RFC 4330, RFC 5905; `tests/net/ntp/` (an SNTP server of Python's)
