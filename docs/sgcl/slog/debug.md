[sgcl](../README.md) › [slog](README.md)

# sgcl::slog::debug, info, warn, error

```cpp
#include "sgcl/slog/logger.h"   // or "sgcl/slog.h"

namespace sgcl::slog {
    template<class... A>
    void debug(message m, const A&... kv);    // (1)
    template<class... A>
    void info(message m, const A&... kv);     // (2)
    template<class... A>
    void warn(message m, const A&... kv);     // (3)
    template<class... A>
    void error(message m, const A&... kv);    // (4)
}
```

Writes a record through the [default logger](default_logger.md), slog's `slog.Debug`, `slog.Info`, `slog.Warn` and
`slog.Error`: `default_logger().info(m, kv...)` and the like ([logger::log](logger/log.md)). Until
[set_default](set_default.md) the default logger writes text on `io::stderr` from `info` up, in the local time.

1. A record of `level::debug`.
2. A record of `level::info`.
3. A record of `level::warn`.
4. A record of `level::error`.

- (1–4) The attributes are pairs, a key then a value, and [groups](group.md), checked by the compiler as a
  logger's verbs check them.

## Parameters

| Parameter | Description |
|---|---|
| `m` | the text of the record and the place of the call |
| `kv` | the attributes: pairs and groups |

## Return value

None. A write that fails is counted by the default logger's [dropped](logger/dropped.md).

## Complexity

One atomic load of the default logger, then what [logger::log](logger/log.md) costs.

## Exceptions

What the text of a value of the program throws (its `to_text`, `to_string` or `format_value`), what a handler or a
writer of the program set as the default throws.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::info("server started", "port", 8080, "tls", true);
    auto config = io::read_file("/etc/app.conf");
    if (!config) {
        slog::error("read failed", "path", "/etc/app.conf", "error", config.error());
    }
    slog::debug("not written: the default logger starts at info");
}
```

Output:

```text
time=2026-09-28T21:21:25.303+02:00 level=INFO msg="server started" port=8080 tls=true
time=2026-09-28T21:21:25.304+02:00 level=ERROR msg="read failed" path=/etc/app.conf error="open /etc/app.conf: No such file or directory"
```

## See also

- [default_logger](default_logger.md), [set_default](set_default.md)
- [logger::log](logger/log.md): the verbs of a logger of one's own
- [sgcl::slog](README.md)
