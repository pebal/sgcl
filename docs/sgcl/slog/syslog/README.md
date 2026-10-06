[sgcl](../../README.md) › [slog](../README.md)

# sgcl::slog::syslog

```cpp
#include "sgcl/slog/syslog.h"   // or "sgcl/slog.h"

namespace sgcl::slog {
    class syslog {
    public:
        friend bool operator==(const syslog& a, const syslog& b) noexcept;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::slog::syslog` is a handler of the module that sends each record as one syslog message: to the machine's own
daemon through its socket ([local](local.md)), or to a remote collector over a connection of the program's
([(constructor)](syslog.md)). The message is RFC 5424's, `<PRI>1 TIMESTAMP HOSTNAME APP-NAME PROCID MSGID SD MSG`, or
RFC 3164's for the local daemons, `<PRI>Mmm dd hh:mm:ss TAG[PID]: MSG`, as Go's `log/syslog` writes it there; its MSG
is the record's message and the module's own text of the attributes after it (`disk almost full free=1024
req.id=7`), or, with an [options](../syslog-options.md)`::structured_data_id`, the attributes as RFC 5424's
structured data. A level becomes a severity: error and up 3, warn 4, info 6, debug and below 7.

The remote transports are connections of [net](../../net/README.md) handed over as an `io::writer`: a TCP or TLS
connection with octet counting (RFC 6587, RFC 5425), or a connected UDP socket, a datagram a write. The module stands
below net, whose HTTP server logs through it, so it opens no network connection itself.

## Rules

- A syslog is a handle: one word, a tracked word to the socket or the writer, which copies share;
  [operator==](operator_cmp.md) says whether two are the same.
- One record, one write: a datagram to the local socket, from the thread that logs; a write to a remote stream under a
  short lock of the handler's, since a TLS connection does not take writes from many threads at once.
- A write that fails throws nothing: [dropped](dropped.md) counts the messages lost. A connection that breaks is the
  program's to make again.

## Member types

| Type | Definition |
|---|---|
| [facility](../syslog-facility.md) | the facility of a message, RFC 5424's numbers |
| [format](../syslog-format.md) | RFC 5424, RFC 3164, or the one the transport wants |
| [options](../syslog-options.md) | the facility, the names, the format and the framing |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](syslog.md) | constructs a handler over a connection to a remote server |
| [local](local.md) | constructs a handler over the machine's syslog socket |
| `(destructor)` | lets go of the handle; the socket is closed once no handle holds it |

#### Records

| Function | Description |
|---|---|
| [handle](handle.md) | sends a record as one message |
| [dropped](dropped.md) | the messages whose write failed |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator!=](operator_cmp.md) | checks whether two handles are the same |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    io::buffer collector;  // a connection to a collector, here a buffer
    slog::logger log(slog::syslog(collector, {.app_name = "api", .hostname = "web-1"}));
    log.warn("disk almost full", "free", 1024);
    string m = collector.text();
    println("{}", m.substr(0, 4));
    println("{}", m.substr(m.find(" web-1")));
}
```

Sample output:

```text
<12>
 web-1 api 41022 - - disk almost full free=1024
```

## See also

- [journald](../journald/README.md): systemd's journal
- [options](../syslog-options.md), [facility](../syslog-facility.md), [format](../syslog-format.md)
- [handler](../handler/README.md): what a logger takes
