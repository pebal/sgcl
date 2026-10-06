[sgcl](../../README.md) › [slog](../README.md) › [syslog](README.md)

# sgcl::slog::syslog::local

```cpp
static expected<syslog, io::error> local(const options& o = {}) noexcept;
```

A handler over the machine's syslog daemon: a datagram socket connected to the first of `/dev/log` (Linux),
`/var/run/syslog` (macOS) and `/var/run/log` (the BSDs). The format `automatic`, the options' default, is RFC 3164
here, the format those daemons read, without the hostname, which the daemon adds; Go's `log/syslog` writes the same
locally. Options given for something else keep it: `local({.app_name = "api"})` is RFC 3164 still, and only a format
named in them (`rfc5424`) takes its place.

## Parameters

| Parameter | Description |
|---|---|
| `o` | the facility, the app name, the format ([options](../syslog-options.md)) |

## Return value

The handler, or the `io::error` of the last socket tried (none there: `is_not_found()`).

## Complexity

A socket and a connect.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    auto local = slog::syslog::local({.facility = slog::syslog::facility::daemon, .app_name = "svc"});
    if (!local) {
        println("no syslog here: {}", local.error().message());
        return 0;
    }
    slog::logger log(*local);
    log.info("service started");
    println("{}", local->dropped());
}
```

## See also

- [(constructor)](syslog.md): a remote server
- [format](../syslog-format.md)
- [sgcl::slog::syslog](README.md)
