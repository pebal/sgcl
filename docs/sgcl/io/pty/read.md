[sgcl](../../README.md) › [io](../README.md) › [pty](README.md)

# sgcl::io::pty::read, async_read

```cpp
expected<size_t, error> read(const slice<byte>& buffer) const;                                // (1)
async::task<expected<size_t, error>> async_read(const slice<byte>& buffer) const noexcept;    // (2)
```

Reads what the program on the terminal wrote, at most the size of `buffer`: its output, and the echo of what was
written to the pseudo-terminal, with the terminal's line ends (`"\r\n"`). The master is non-blocking and served by
the [reactor](../../async/readable.md).

1. On the calling thread, which waits on the reactor until there is something to read.
2. The same for a task, which holds no worker while it waits.

## Parameters

| Parameter | Description |
|---|---|
| `buffer` | where the bytes go; its size is the most read |

## Return value

The number of bytes read; 0 at the end, once nobody holds the terminal end any more (the child started by
[start](start.md) and whatever it left on the terminal ended; Linux's `EIO` then is a read of 0 too), and for an
empty `buffer`. Or the [error](../error/README.md), its operation `read` and its path the terminal's
[name](name.md): `errc::closed` when the pseudo-terminal was closed before the call or while it waited
([close](close.md)), the `errno` of `read(2)` otherwise.

## Complexity

One system call, linear in the bytes read; one more per wait for readiness.

## Exceptions

- (1) `std::system_error` when the read has to wait and the thread of the reactor, which its first use starts,
  cannot be made.
- (2) None: the task's own exceptions are the task's.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<> screen(io::pty term) {
    vector<byte> chunk(256);
    for (;;) {
        size_t n = (co_await term.async_read(chunk)).value();
        if (n == 0) {
            break;
        }
        string text(chunk.as_slice().first(n));
        print("{}", text.replace("\r\n", "\n"));
    }
}

int main() {
    io::pty term = io::open_pty().value();
    io::command sh("/bin/sh", "-c", "echo one; echo two");
    term.start(sh).value();
    async::run(screen(term));
    sh.wait().value();
}
```

Output:

```text
one
two
```

## See also

- [write, async_write](write.md): the other direction
- [read_all_text](../mixin/reader/read_all_text.md): to the end in one call
- [sgcl::io::pty](README.md)
