[sgcl](../README.md) › [io](README.md)

# sgcl::io::pipe

```cpp
#include "sgcl/io/file.h"   // or "sgcl/io.h"

namespace sgcl::io {
    expected<pipe_ends, error> pipe() noexcept;
}
```

An anonymous pipe: what is written to the write end is read from the read end, in order, through the kernel's
buffer. Both ends are non-blocking and served by the [reactor](../async/readable.md): a task waits for data or for
room without holding a worker, and a thread's read or write waits on the reactor in place. Both are `O_CLOEXEC`,
named `"pipe"` ([path](file/path.md)). Go's `os.Pipe`.

## Parameters

None.

## Return value

The two ends, [pipe_ends](pipe_ends.md): `ends.read` and `ends.write`, or `auto [r, w] = io::pipe().value();`. Or the
[error](error.md), its operation `pipe`, with the `errno` of `pipe(2)` (`EMFILE` when the process has no
descriptor left).

## Complexity

Constant: a few system calls.

## Exceptions

None.

## Notes

The reader sees the end, a read of 0, once the write end is closed: by [close](file/close.md), or by the collector
when nothing holds it any more, which may be long after. Close the write end when done.

A write to a pipe whose read end is closed fails with `EPIPE`; it raises no `SIGPIPE`, which would end the process,
as a write to a [net](../net/README.md) connection whose peer is gone does not. The pipes [command](command.md)
makes give the child's end the signal back, as a program at a shell's pipe expects.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<> produce(io::file out) {
    for (int i : range(3)) {
        co_await out.async_write("tick\n");
    }
    out.close();  // the reader sees the end
}

int main() {
    auto [in, out] = io::pipe().value();
    async::go(produce(out));
    print("{}", *in.read_all_text());
}
```

Output:

```text
tick
tick
tick
```

A write after the reader is gone is an error, and the program goes on:

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto [in, out] = io::pipe().value();
    in.close();
    auto written = out.write("nobody reads this");
    println("{}", written.error().message());
    println("still running");
}
```

Output:

```text
write pipe: Broken pipe
still running
```

## See also

- [pipe_ends](pipe_ends.md): the two ends
- [command](command.md): a child process's standard streams through pipes
- [sgcl::io::file](file.md)
