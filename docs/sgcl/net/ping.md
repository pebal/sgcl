[sgcl](../README.md) › [net](README.md)

# sgcl::net::ping, async_ping

```cpp
#include "sgcl/net/ping.h"

namespace sgcl::net {
    expected<ping_reply, io::error> ping(const string& host, const ping_options& o = {});                  // (1)
    async::task<expected<ping_reply, io::error>> async_ping(string host, ping_options o = {}) noexcept;    // (2)
}
```

An ICMP echo to the host (RFC 792, RFC 4443 for IPv6) and its [reply](ping_reply.md): who answered and after how
long. The request goes over an unprivileged datagram socket of ICMP where the system has one — macOS always; Linux
when the user's group is within `net.ipv4.ping_group_range` (`sysctl -w net.ipv4.ping_group_range="0 2147483647"`)
— and over a raw socket otherwise, which needs root or `CAP_NET_RAW` on Linux. Neither: `EACCES` with a path that
says so.

A reply is taken when it is ours: its sequence, and a random cookie of the payload's (another process's echo is
passed over). An ICMP error about the request ends the wait: `EHOSTUNREACH` for a destination unreachable,
`ETIMEDOUT` with "time exceeded" for a TTL that ran out on the way.

`ping` waits on the calling thread; a task awaits `async_ping`.

## Parameters

| Parameter | Description |
|---|---|
| `host` | a name or an address, IPv4 or IPv6 |
| `o` | the timeout, the payload's size, the TTL, the stop |

## Return value

The reply; `ETIMEDOUT` past the timeout, `EHOSTUNREACH`, `ECANCELED`, the resolver's error, `EACCES` with no ICMP
socket.

## Complexity

A round trip.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ping.h"

using namespace sgcl;

int main() {
    net::ping_reply r = net::ping("127.0.0.1").value();
    println("{} bytes from {}, ttl {}, under a second: {}", r.bytes, r.from, r.ttl, r.rtt.milliseconds() < 1000);
}
```

Output:

```text
64 bytes from 127.0.0.1, ttl 64, under a second: true
```

## See also

- [ping_options](ping_options.md)
- [ping_reply](ping_reply.md)
- [ntp](ntp/README.md)
