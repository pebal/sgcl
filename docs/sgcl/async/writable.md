[sgcl](../README.md) › [async](README.md)

# sgcl::async::writable

```cpp
#include "sgcl/async/reactor.h"   // or "sgcl/async.h"

namespace sgcl::async {
    event writable(int fd);
}
```

Returns an [event](event.md) set when `fd` can be written without blocking, or when the wait is ended with nothing
([cancel_waits](cancel_waits.md), a stop of the reactor): what [readable](readable.md) is for a read, with its rules.
A task writes `co_await async::writable(fd)` and holds no thread while the descriptor's buffer is full; a write woken
by it tries again, and a write that would block waits again.

## Parameters

| Parameter | Description |
|---|---|
| `fd` | the descriptor to wait for: a pipe, a socket, a terminal of the program's own |

## Return value

An event, set by the first readiness after the call or by the end of the wait.

## Complexity

Constant: the event made, and the descriptor registered with the kernel, a system call under the reactor's lock;
several waits on one descriptor in one direction share one registration.

## Exceptions

`std::system_error` when the reactor's thread has to be started and cannot be.

## Notes

The wait is one-shot and readiness, not room for a whole write: a write of more than the descriptor takes at once
writes a part, and the rest waits on a new `writable(fd)`. The rules of the waits, of a close and of a stop are on
[readable](readable.md).

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include <fcntl.h>
#include <unistd.h>

using namespace sgcl;

async::task<long> write_all(int fd, long size) {
    char block[4096] = {};
    long written = 0;
    while (written < size) {
        auto n = ::write(fd, block, sizeof block);
        if (n > 0) {
            written += n;
        } else {
            co_await async::writable(fd);  // the pipe is full: no thread held until it drains
        }
    }
    co_return written;
}

int main() {
    int fd[2];
    if (::pipe(fd) != 0) {
        return 1;
    }
    ::fcntl(fd[1], F_SETFL, O_NONBLOCK);
    auto writer = async::spawn(write_all(fd[1], 1 << 20));
    char buf[4096];
    long got = 0;
    while (got < (1 << 20)) {
        auto n = ::read(fd[0], buf, sizeof buf);
        if (n <= 0) {
            break;
        }
        got += n;
    }
    println("written {}, read {}", writer.wait(), got);
    ::close(fd[0]);
    ::close(fd[1]);
}
```

Output:

```text
written 1048576, read 1048576
```

## See also

- [readable](readable.md): the same for a read, and the rules of the reactor
- [cancel_waits](cancel_waits.md): the waits on a descriptor ended before its close
- [event](event.md): what a wait is
