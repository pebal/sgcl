[sgcl](../README.md) › [async](README.md)

# sgcl::async::readable

```cpp
#include "sgcl/async/reactor.h"   // or "sgcl/async.h"

namespace sgcl::async {
    event readable(int fd);
}
```

Returns an [event](event/README.md) set when `fd` can be read without blocking (data, or the end of the stream), or when the
wait is ended with nothing ([cancel_waits](cancel_waits.md), a stop of the reactor). A task writes
`co_await async::readable(fd)` and holds no thread until the data comes; a thread writes `async::readable(fd).wait()`;
a [select](select.md) bounds the wait (`async::readable(fd).on_set(f), async::timeout(1s, g)`) and a
[stop_token](stop_token/README.md) cancels it, since the wait is an event like any other. A wait woken by the event looks at
the descriptor again: the read says whether there is data, and a read that would block waits again.

Under it is the reactor: one thread on the kernel's queue (kqueue on macOS and FreeBSD, epoll on Linux, IOCP to
come), asleep in the kernel until something is ready, which then sets the event: a push on the scheduler for the task
that waits. The thread and the kernel's queue are made by the first wait, and stopped with the
[scheduler](scheduler/stop.md). The registration is one-shot (on kqueue an `EV_ONESHOT` entry), made under the
reactor's lock.
[writable](writable.md) is the same for a write, [exited](exited.md) for the end of a process.

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

- A wait is one-shot: the event is set once, for the first readiness after the call; the next wait is a new
  `readable(fd)`. Several waits on one descriptor in one direction share one registration and are set together by
  its first readiness.
- A wait left behind (a select that chose another case, a task that stopped) stays on the registration until the
  descriptor is ready, then fires and is dropped. A wait given up on a descriptor that may stay idle (a read with a
  short deadline in a loop) is ended by setting its event: the reactor drops the set ones at later waits on the
  descriptor, and holds no more than about twice the waits still open.
- The event is readiness, not data, and the end of a wait sets it too: after it `is_set()` is true either way, so the
  code that waited checks the result of its operation, never the event alone. A descriptor the kernel cannot watch
  (a regular file, on kqueue) is set at once, and the read says what is what.
- The event is a handle ([event](event/README.md)): one word, a tracked word, on a stack, in a task, in a managed object; in
  a global or a `std` container, a `rooted<async::event>`. Its state lives as long as something holds it, the
  reactor's registration included.
- The kernel's queue is made by the first wait; when it cannot be (`kqueue()` failing with the descriptors
  exhausted) that wait ends with nothing, and the next one tries again.
- [scheduler::stop](scheduler/stop.md) stops the reactor too: the waits still registered are ended with nothing
  (their events set), and so is a wait registered while the reactor is stopping; the next wait starts it again.
- A descriptor closed with a wait registered: the kernel drops the registration with the descriptor and nothing would
  signal the wait, and the reactor's record of it would take the waits of the next descriptor with the same number.
  So a program that closes a descriptor it waits on this way calls [cancel_waits](cancel_waits.md) first.
- The descriptors of the `io` and `net` modules (a file, a socket, a pipe) do not wait this way: each is registered
  with the kernel once, at its first wait in a direction, edge-triggered (kqueue's `EV_CLEAR`, epoll's `EPOLLET`),
  and stays registered until it is closed. A read that would block parks the task in the descriptor's slot, one word
  per direction that the task and the reactor's thread change with compare-exchanges: no system call and no lock per
  wait, and an edge that comes between the call's `EAGAIN` and the park is kept in the word (Go's `pollDesc`).
  Several waiters in one direction wait behind the first. A close, a deadline or a stop of the reactor wakes them
  without readiness (a stop fails the operation with `ECANCELED`), and a close releases the descriptor's slot on the
  reactor before the number goes back to the kernel. A descriptor the kernel refuses to register, or
  whose number is past the reactor's table (four million), fails the wait with the reason. `readable` and `writable`
  are for everything else; the two ways do not mix on one descriptor number, since the kernel keeps one entry per
  number and filter.
- The reactor's thread is a thread of the program to the collector, like any other.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include <unistd.h>

using namespace sgcl;

async::task<char> read_one(int fd) {
    co_await async::readable(fd);  // no thread held until the byte comes
    char c = 0;
    if (::read(fd, &c, 1) != 1) {
        co_return '?';
    }
    co_return c;
}

int main() {
    int fd[2];
    if (::pipe(fd) != 0) {
        return 1;
    }
    auto reader = async::spawn(read_one(fd[0]));
    [[maybe_unused]] auto n = ::write(fd[1], "x", 1);
    println("read {}", reader.wait());
    ::close(fd[0]);
    ::close(fd[1]);
}
```

Output:

```text
read x
```

## See also

- [writable](writable.md): the same for a write
- [cancel_waits](cancel_waits.md): the waits on a descriptor ended before its close
- [exited](exited.md): the end of a child process
- [event](event/README.md): what a wait is
- [select](select.md), [timeout](timeout.md), [stop_token](stop_token/README.md): bounding and cancelling a wait
- [spawn_blocking](spawn_blocking.md): a blocking call on a thread of its own, for what the reactor cannot watch
