[sgcl](../../README.md) › [net](../README.md) › [ntp](README.md)

# sgcl::net::ntp::response

```cpp
#include "sgcl/net/ntp.h"

namespace sgcl::net::ntp {
    struct response {
        duration offset;
        duration delay;
        time::datetime time;
        int stratum = 0;
        int leap = 0;
        int version = 0;
        string reference_id;
        int poll = 0;
        double precision = 0;
        duration root_delay;
        duration root_dispersion;
        time::datetime reference_time;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::ntp::response` is what a server answered to a [query](query.md) (RFC 5905 §7.3), with what it says of
this machine's clock: θ = ((T2 − T1) + (T3 − T4)) / 2 and δ = (T4 − T1) − (T3 − T2), T1 the query's departure, T2 its
arrival at the server, T3 the answer's departure, T4 its arrival here.

## Member objects

| Member | Description |
|---|---|
| `offset` | θ: add it to this clock to have the server's |
| `delay` | δ: the round trip, the server's own time taken out |
| `time` | the server's clock when it answered (T3), in UTC |
| `stratum` | 1: a reference clock of its own (GPS, an atomic clock); 2 to 15: that many servers away |
| `leap` | 0 none, 1 a minute of 61 seconds, 2 one of 59 at the end of the day |
| `version` | the server's NTP version |
| `reference_id` | stratum 1: its clock's code ("GPS", "PPS"); above: the upstream server's IPv4 address |
| `poll` | log2 of its poll interval in seconds |
| `precision` | its clock's precision in seconds |
| `root_delay` | the round trip to its reference clock |
| `root_dispersion` | the error it allows itself to its reference clock |
| `reference_time` | when its clock was last set, in UTC |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ntp.h"

using namespace sgcl;

int main() {
    auto r = net::ntp::query("pool.ntp.org").value();
    println("stratum {} via {}, version {}", r.stratum, r.reference_id, r.version);
}
```

Output:

```text
stratum 2 via 127.0.0.1, version 4
```

## See also

- [query](query.md)
