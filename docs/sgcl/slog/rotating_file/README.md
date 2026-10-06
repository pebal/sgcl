[sgcl](../../README.md) › [slog](../README.md)

# sgcl::slog::rotating_file

```cpp
#include "sgcl/slog/rotating_file.h"   // or "sgcl/slog.h"

namespace sgcl::slog {
    class rotating_file {
    public:
        friend bool operator==(const rotating_file& a, const rotating_file& b) noexcept;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::slog::rotating_file` is a log file that rotates itself: a writer of io for a logger's
[options](../options.md)`::out`, or for any `io::writer`. A write is one `write` to the file opened for appending,
from the calling thread, with no lock: the module's rule of one record, one write. A write that would take the file
past its [rotation](../rotation.md)'s size, or that comes after the next time of its cron, rotates it first: the file
is renamed to `name-YYYY-MM-DDTHH-MM-SS.mmm.ext` (the local time; `-1`, `-2` after it for a name taken in the same
millisecond), the path is opened anew, and the writes go on into it. Writes of other threads in that moment land in
the renamed file, through the descriptor they hold: none is lost or split, and the old descriptor is closed once no
write holds it. The gzip of a rotated file and the removal of those past `keep` run on the blocking pool, one job at a
time, so the thread that logs pays a rename and an open.

Go's `log/slog` writes to an `io.Writer` and leaves rotation to packages such as lumberjack, whose names this
follows (a timestamp in the name, never a cascade of renames); the times of a [cron](../../time/cron/README.md) in
place of an interval, so `@daily` rotates at midnight of its zone, across a change of the clock too.

## Rules

- A rotating_file is a handle: one word, a tracked word to the file's state, which copies share;
  [operator==](operator_cmp.md) says whether two are the same.
- The file is rotated by the write that crosses, never in between: a file quiet past its time is rotated by its next
  write. A size is never passed but by the writes in flight while one thread rotates, and by a single write longer
  than the size.
- A [close](close.md) ends it: a write after it is `errc::closed`. A file never closed is closed by the collector, and
  a SIGHUP watch lets go of a file nobody holds.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](rotating_file.md) | a handle of the same file |
| [open](open.md) | opens a log file, or says why it cannot |
| `(destructor)` | lets go of the handle |

#### Writing

| Function | Description |
|---|---|
| [write](write.md) | appends bytes, rotating first when the size or the time says so |
| [rotate](rotate.md) | rotates now |
| [reopen](reopen.md) | opens the path again, after an outside tool moved the file |
| [close](close.md) | closes the file |

#### Observers

| Function | Description |
|---|---|
| [path](path.md) | the path of the current file |
| [size](size.md) | the bytes in the current file |

#### From mixin::writer

The writes of text, a byte and a whole buffer of io's [mixin::writer](../../io/mixin/writer/README.md).

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator!=](operator_cmp.md) | checks whether two handles are the same file |

## Complexity

A write is one system call; a rotation a rename and an open more.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::rotating_file out = slog::rotating_file::open("app.log", {.max_size = 200, .keep = 2});
    slog::logger log(slog::options{.out = out, .json = true});
    for (int i : range(20)) {
        log.info("tick", "i", i);
    }
    println("{} bytes in the current file", out.size() <= 200 ? "at most 200" : "too many");
}
```

Output:

```text
at most 200 bytes in the current file
```

## See also

- [rotation](../rotation.md): when and how
- [logger](../logger/README.md), [options](../options.md): what writes to it
- [io::file](../../io/file/README.md): a file without rotation
