[sgcl](../../README.md) › [slog](../README.md) › [rotating_file](README.md)

# sgcl::slog::rotating_file::open

```cpp
static expected<rotating_file, io::error> open(const string& path, const rotation& r = {}) noexcept;
```

Opens the log file at `path` for appending, made when absent (mode 0644), its size that of what is there; the cron's
next time is taken from now. With `reopen_on_sighup` the handler of SIGHUP is in place before it returns.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the path of the log file |
| `r` | when and how it rotates; the defaults: 100 MB, 7 kept |

## Return value

The file, or the `io::error` of the open (a directory that does not exist: `is_not_found()`).

## Complexity

An open and a stat.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    auto out = slog::rotating_file::open("app.log", {.max_size = 1 << 20});
    if (!out) {
        println("{}", out.error().message());
        return 1;
    }
    println("{} {}", out->path(), out->size());
}
```

Output:

```text
app.log 0
```

## See also

- [(constructor)](rotating_file.md): a handle of the same file
- [rotation](../rotation.md)
- [sgcl::slog::rotating_file](README.md)
