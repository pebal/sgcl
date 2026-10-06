[sgcl](../../README.md) › [slog](../README.md) › [syslog](README.md)

# sgcl::slog::syslog::syslog

```cpp
explicit syslog(const io::writer& out, const options& o = {});    // (1)
syslog(const syslog&) noexcept = default;                         // (2)
syslog(syslog&&) noexcept = default;                              // (3)
```

1. A handler that writes each message to `out`, a connection to a remote syslog server of the program's: a TCP or TLS
   connection of [net](../../net/README.md) with `octet_counting`, or a connected UDP socket, a datagram a write. The
   app name and the hostname are the options', or the program's name and the system's.
2. A handle of the same handler.
3. The same, taken from the other handle.

## Parameters

| Parameter | Description |
|---|---|
| `out` | where the messages go |
| `o` | the facility, the names, the format, the framing |

## Complexity

- (1) The system's hostname and the program's name read once.
- (2–3) Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    io::buffer stream;
    slog::syslog h(stream, {.app_name = "api", .hostname = "h", .octet_counting = true});
    slog::logger log(h);
    log.info("up");
    string m = stream.text();
    println("{}", m.substr(m.find(' ') + 1, 4));  // the length, a space, the message
}
```

Output:

```text
<14>
```

## See also

- [local](local.md): the machine's own syslog
- [options](../syslog-options.md)
- [sgcl::slog::syslog](README.md)
