[sgcl](../README.md) › [slog](README.md) › [syslog](syslog/README.md)

# sgcl::slog::syslog::format

```cpp
#include "sgcl/slog/syslog.h"   // or "sgcl/slog.h"

namespace sgcl::slog {
    class syslog {
    public:
        enum class format : uint8_t {
            automatic,
            rfc5424,
            rfc3164
        };
    };
}
```

The shape of a syslog message: the protocol of RFC 5424 for a collector, or the older one of RFC 3164 that the
daemons of Unix read on their local socket. The default, `automatic`, takes the one the transport wants: RFC 3164 for
[local](syslog/local.md), RFC 5424 for a writer ([(constructor)](syslog/syslog.md)), so that options given for
something else (`{.app_name = "api"}`) leave the format as it fits.

| Value | Description |
|---|---|
| `automatic` | RFC 3164 on the local socket, RFC 5424 over a writer; the default |
| `rfc5424` | `<PRI>1 TIMESTAMP HOSTNAME APP-NAME PROCID MSGID SD MSG`: the time in RFC 3339 with microseconds and its offset, structured data possible |
| `rfc3164` | `<PRI>Mmm dd hh:mm:ss HOSTNAME TAG[PID]: MSG`, the local time to the second; the hostname left out on the local socket, where the daemon adds it |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    io::buffer out;
    slog::logger log(slog::syslog(out, {.app_name = "cron", .hostname = "box", .format = slog::syslog::format::rfc3164}));
    log.info("job done");
    string m = out.text();
    println("{}", m.substr(m.find(" box")));
}
```

Sample output:

```text
 box cron[52113]: job done
```

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    io::buffer out;  // a writer: automatic is RFC 5424
    slog::logger log(slog::syslog(out, {.app_name = "api", .hostname = "h"}));
    log.info("started");
    println("{}", out.text().substr(0, 5));
}
```

Output:

```text
<14>1
```

## See also

- [syslog::options](syslog-options.md)
- [sgcl::slog::syslog](syslog/README.md)
