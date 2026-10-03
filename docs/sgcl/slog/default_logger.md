[sgcl](../README.md) › [slog](README.md)

# sgcl::slog::default_logger

```cpp
#include "sgcl/slog/logger.h"   // or "sgcl/slog.h"

namespace sgcl::slog {
    logger default_logger() noexcept;
}
```

Returns the default logger, slog's `slog.Default`: the logger [debug, info, warn and error](debug.md) write through,
and the access log of [net::http::server](../net/http/server/README.md) by default. It starts as `logger()`, text on
`io::stderr` from `info` up, until [set_default](set_default.md). The module keeps it in a
`rooted<atomic<logger>>` of its own (the atomic of a one-word handle, [atomic](../core/atomic.md)), so the call is
one atomic load. It is never destroyed: a record from the destructor of a static, at the end of the program, goes
through it too.

## Parameters

None.

## Return value

A copy of the default logger: it shares the default's output.

## Complexity

Constant: one atomic load.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    auto log = slog::default_logger();
    println("{} {}", log.enabled(slog::level::debug), log.enabled(slog::level::info));
    log.with("component", "db").info("connected");
}
```

Output:

```text
false true
time=2026-09-28T14:05:01.123+02:00 level=INFO msg=connected component=db
```

## See also

- [set_default](set_default.md)
- [debug, info, warn, error](debug.md)
- [sgcl::slog](README.md)
