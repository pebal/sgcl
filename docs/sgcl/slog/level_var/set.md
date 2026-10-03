[sgcl](../../README.md) › [slog](../README.md) › [level_var](README.md)

# sgcl::slog::level_var::set

```cpp
void set(level l) const noexcept;
```

Changes the level to `l`, slog's `LevelVar.Set`. The next record of every logger made with this variable, or with a
copy of it, on any thread, is judged by `l`; a record in flight on another thread may still be judged by the level
before (a relaxed store). The method is `const`: the level is the shared state, not the handle.

## Parameters

| Parameter | Description |
|---|---|
| `l` | the new level |

## Return value

None.

## Complexity

Constant: one relaxed atomic store.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::level_var least(slog::level::error);
    slog::logger a(slog::options{.out = io::stdout, .level_var = least});
    slog::logger b(slog::options{.out = io::stdout, .level_var = least, .json = true});

    a.warn("not written");
    least.set(slog::level::warn);
    a.warn("text");
    b.warn("json");
}
```

Output:

```text
time=2026-09-28T14:05:01.123+02:00 level=WARN msg=text
{"time":"2026-09-28T14:05:01.123456+02:00","level":"WARN","msg":"json"}
```

## See also

- [get](get.md)
- [sgcl::slog::level_var](README.md)
