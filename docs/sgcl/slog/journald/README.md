[sgcl](../../README.md) › [slog](../README.md)

# sgcl::slog::journald

```cpp
#include "sgcl/slog/syslog.h"   // or "sgcl/slog.h"

namespace sgcl::slog {
    class journald {
    public:
        friend bool operator==(const journald& a, const journald& b) noexcept;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::slog::journald` is a handler that writes each record to systemd's journal in its native protocol: one datagram
of fields to `/run/systemd/journal/socket`. The fields are `MESSAGE`, `PRIORITY` (the severity of
[syslog](../syslog/README.md)), `SYSLOG_IDENTIFIER`, `SYSLOG_PID`, `CODE_FILE`, `CODE_LINE` and `CODE_FUNC` with
[options](../options.md)`::source`, and the attributes, each a field named by its key upper-cased, its other
characters `_` and a group's name and `_` before it (`req.method` is `REQ_METHOD`); `journalctl -o verbose` shows them
and filters by them. A value with a newline goes in the protocol's binary form.

Linux only: elsewhere there is no journal, and [open](open.md) says so. A record larger than a datagram may be (which
journald takes through a memfd) is dropped and counted.

## Rules

- A journald is a handle: one word, a tracked word to the socket, which copies share.
- One record, one datagram, from the thread that logs.

## Member functions

| Function | Description |
|---|---|
| [open](open.md) | connects to the journal |
| `(destructor)` | lets go of the handle; the socket is closed once no handle holds it |

#### Records

| Function | Description |
|---|---|
| [handle](handle.md) | sends a record as one entry |
| [dropped](dropped.md) | the entries whose send failed |

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
    auto journal = slog::journald::open("myapp");
    if (!journal) {
        println("no journal here");
        return 0;
    }
    slog::logger log(slog::options{.handler = *journal, .source = true});
    log.warn("disk almost full", "free", 1024);
}
```

Sample output:

```text
no journal here
```

## See also

- [syslog](../syslog/README.md): RFC 5424 and the local syslog
- [handler](../handler/README.md)
