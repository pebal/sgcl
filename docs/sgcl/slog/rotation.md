[sgcl](../README.md) › [slog](README.md)

# sgcl::slog::rotation

```cpp
#include "sgcl/slog/rotating_file.h"   // or "sgcl/slog.h"

namespace sgcl::slog {
    struct rotation {
        uint64_t max_size = 100 << 20;
        optional<time::cron> at;
        size_t keep = 7;
        bool compress = false;
        bool reopen_on_sighup = false;
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::slog::rotation` is when and how a [rotating_file](rotating_file/README.md) rotates: by size, at the times of
a [cron](../time/cron/README.md), or both; how many rotated files it keeps; whether it gzips them; whether a SIGHUP
opens the path again. A plain struct, filled by designated initializers:
`{.max_size = 10 << 20, .at = time::cron("@daily"), .keep = 14, .compress = true}`.

## Member objects

| Member | Description |
|---|---|
| `max_size` | the bytes the file may hold: a write that would take it past them rotates it first; 100 MB by default, 0 no limit. A single write longer than this goes alone into a file of its own |
| `at` | the times the file is rotated at too, those of the cron in its zone: the first write after each rotates it; none by default |
| `keep` | the rotated files kept, the oldest removed; 7 by default, 0 all |
| `compress` | a rotated file gzipped beside itself (`app-…log.gz`) and the plain one removed, on the blocking pool; `false` by default |
| `reopen_on_sighup` | the path opened again at SIGHUP, for a tool that moves the file away and signals (logrotate without `copytruncate`); `false` by default |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    slog::rotation daily{.max_size = 0, .at = time::cron("@daily"), .keep = 30, .compress = true};
    slog::rotating_file out = slog::rotating_file::open("app.log", daily);
    println("{}", out.path());
}
```

Output:

```text
app.log
```

## See also

- [rotating_file](rotating_file/README.md)
- [time::cron](../time/cron/README.md): the times
