[sgcl](../../README.md) › [net](../README.md) › [ntp](README.md)

# sgcl::net::ntp::query, async_query

```cpp
expected<response, io::error> query(const string& server = string("pool.ntp.org"),                // (1)
                                    const options& o = {});
async::task<expected<response, io::error>> async_query(string server = string("pool.ntp.org"),    // (2)
                                                       options o = {}) noexcept;
```

One query of the server: a client's packet of NTPv4 sent over UDP, the server's answer read and checked, and what
it says of this machine's clock — [response](response.md)'s offset, the server's clock less this one's, and delay,
the round trip less the server's own time. The answer is waited for at most the [options](options.md)' timeout.

`query` waits on the calling thread; a task awaits `async_query`.

## Parameters

| Parameter | Description |
|---|---|
| `server` | "host[:123]" |
| `o` | the timeout and the stop |

## Return value

The server's answer; `errc::kiss_of_death` (the code in the path), `errc::unsynchronized`, `errc::malformed`,
`ETIMEDOUT`, `ECANCELED`, the resolver's error for a name that is not there.

## Complexity

A round trip.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ntp.h"

using namespace sgcl;

int main() {
    net::ntp::response r = net::ntp::query("pool.ntp.org").value();
    println("stratum {}, off by less than a second: {}", r.stratum, r.offset.milliseconds() < 1000 && r.offset.milliseconds() > -1000);
}
```

Output:

```text
stratum 2, off by less than a second: true
```

## See also

- [response](response.md)
- [options](options.md)
- [ntp](README.md)
