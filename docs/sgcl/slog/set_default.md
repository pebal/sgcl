[sgcl](../README.md) › [slog](README.md)

# sgcl::slog::set_default

```cpp
#include "sgcl/slog/logger.h"   // or "sgcl/slog.h"

namespace sgcl::slog {
    void set_default(const logger& l) noexcept;
}
```

Makes `l` the default logger from now on, for every thread, slog's `slog.SetDefault`: one atomic store. The free
[debug, info, warn and error](debug.md) and every later [default_logger](default_logger.md) write through it. A
logger taken by `default_logger()` before the call keeps writing where it did.

## Parameters

| Parameter | Description |
|---|---|
| `l` | the new default logger, a copy of which is kept |

## Return value

None.

## Complexity

Constant: one atomic store.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::logger json(slog::options{.out = io::stdout, .level = slog::level::debug, .json = true});
    slog::set_default(json);
    slog::debug("now JSON on stdout", "from", "debug");
}
```

Output:

```text
{"time":"2026-09-28T14:05:01.123456+02:00","level":"DEBUG","msg":"now JSON on stdout","from":"debug"}
```

## See also

- [default_logger](default_logger.md)
- [debug, info, warn, error](debug.md)
- [sgcl::slog](README.md)
