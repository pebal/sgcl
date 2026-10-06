[sgcl](../README.md) › [io](README.md)

# sgcl::io::watch

```cpp
#include "sgcl/io/watch.h"   // or "sgcl/io.h"

namespace sgcl::io {
    expected<async::channel<watch_event>, error> watch(const string& path, const watch_options& options = {},
                                                       async::stop_token stop = {});
}
```

The changes of a file, or of the entries of a directory (its whole tree with `recursive`), from now on, as a
[channel](../async/channel/README.md) of [watch_event](watch_event.md)s: a task `co_await`s it, a thread receives on
it, a [select](../async/select.md) takes it as a case, as [async::signals](../async/signals.md) and
[size_changes](size_changes.md) give theirs. Each event names the path under the path given (the system's real path,
`/private/var/...` for `/var/...` on macOS, given back in the program's form), what happened to it
([watch_op](watch_op.md), several or-ed), and whether it is a directory. The events of a path within the coalescing
interval of [watch_options](watch_options.md) are merged into one. Go has no watch in its standard library
(`fsnotify` is a module of its own).

On macOS the changes come from FSEvents, a stream of file events whose latency is the coalescing interval; a file is
watched through its directory, the events filtered to it. On Linux from inotify, a watch per directory, the new ones
of a recursive watch added as they come; on Windows from `ReadDirectoryChangesW` (both written to the pattern, run
with the platform matrix). The system's side touches nothing of the library's: it leaves plain records and wakes a
task of the library on the [reactor](../async/readable.md), which merges, maps and sends.

The channel holds 64 events: a receiver that falls behind holds the events back, and past 65536 records waiting they
are dropped and one event of `watch_op::overflow` sent, as when the system's own queue overflows: what matters is
read again. The channel ends, closed, when `stop` is requested, or at the next change after the program closed it.

## Parameters

| Parameter | Description |
|---|---|
| `path` | a file or a directory |
| `options` | recursive or not, the coalescing interval ([watch_options](watch_options.md)) |
| `stop` | ends the watch; none by default (it ends with its channel closed) |

## Return value

The channel of the events, or the [error](error/README.md), its operation `watch` and its path the one given:
`is_not_found()` for a path that is not there, `ENOTSUP` where the system has no watch of its own or refuses one, the
`errno` of the call otherwise.

## Complexity

The system's, per change; on Linux, a watch per directory of a recursive tree.

## Exceptions

`std::system_error` when the thread of the reactor, which its first use starts, cannot be made.

## Notes

A rename comes as `renamed` on both names: no system pairs them portably, and a stat tells the new name (there) from
the old (gone). macOS merges the operations of a path that come close together into one event of several ops
(`created | modified`): read the ops as what happened, and the file for what it is now.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::mkdir("inbox");
    async::stop_source stop;
    async::channel<io::watch_event> changes = io::watch("inbox", {}, stop.token()).value();
    io::write_file("inbox/order-1.json", "{}");
    for (io::watch_event e : changes) {
        if (e.path == "inbox/order-1.json" && (e.ops & io::watch_op::created)) {
            println("{} arrived", e.path);
            break;
        }
    }
    stop.request_stop();
}
```

Output:

```text
inbox/order-1.json arrived
```

## See also

- [watch_event](watch_event.md), [watch_op](watch_op.md), [watch_options](watch_options.md)
- [async::stop_token](../async/stop_token/README.md): what ends it
- [walk_dir](walk_dir.md): what is there now
