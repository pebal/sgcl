[sgcl](../README.md) › [io](README.md)

# sgcl::io::size_changes

```cpp
#include "sgcl/io/terminal.h"   // or "sgcl/io.h"

namespace sgcl::io {
    async::channel<terminal_size> size_changes(int fd = 1, async::stop_token stop = {});
}
```

The size of the terminal on the descriptor `fd`, the standard output by default, after every change of it, which
the terminal tells the process by `SIGWINCH`: a [channel](../async/channel/README.md) that a task `co_await`s, a
thread receives on, a [select](../async/select.md) takes as a case — the redraw of a full-screen program. The
signal is taken through [async::signals](../async/signals.md), and a task of the library reads the size after
each one. The channel holds one size, and a newer one replaces a size not received yet, so that a burst of resizes
is one redraw. It ends, closed and the signal's registration forgotten, when `stop` is requested, or at the first
change after the program closed the channel. Go has no such function: `signal.Notify(ch, syscall.SIGWINCH)` and
`term.GetSize` by hand.

## Parameters

| Parameter | Description |
|---|---|
| `fd` | a descriptor of the terminal; 1, the standard output, by default |
| `stop` | ends it; none by default (it ends with its channel closed) |

## Return value

The channel of the sizes ([terminal_size](terminal_size.md)).

## Complexity

Constant; a task and a registration of `SIGWINCH` for as long as it runs.

## Exceptions

`std::system_error` when the thread of the signals, which the first registration starts, cannot be made
([async::signals](../async/signals.md)).

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include <csignal>
#include <unistd.h>

using namespace sgcl;

int main() {
    io::pty term = io::open_pty().value();
    async::stop_source stop;
    async::channel<io::terminal_size> sizes = io::size_changes(term.terminal().fd(), stop.token());
    term.resize({.rows = 40, .columns = 120});
    ::kill(::getpid(), SIGWINCH);  // what the terminal of this process sends when its window is resized
    io::terminal_size size = sizes.receive().wait().value();
    println("{} rows, {} columns", size.rows, size.columns);
    stop.request_stop();
}
```

Output:

```text
40 rows, 120 columns
```

## See also

- [get_terminal_size](get_terminal_size.md): the size now
- [async::signals](../async/signals.md): the signals as a channel
- [terminal_size](terminal_size.md)
