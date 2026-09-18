# Readable, Writable

```cpp
#include "sgcl/Sgcl/Async/Reactor.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    Ptr<Channel<void>> Readable(int fd);   // one signal when fd can be read without blocking, then closed
    Ptr<Channel<void>> Writable(int fd);   // the same for a write
}
```

The same in the `sgcl` interface: [readable, writable](../../async/reactor.md).

The reactor: the readiness of file descriptors, as channels. `Readable(fd)` is a channel that gets one signal when `fd` can be read without blocking (data, or the end of the stream), and is closed then; `Writable(fd)` the same for a write. A task writes `co_await Readable(fd)->AsyncReceive()` and holds no thread until the data comes; a [Select](Select.md) bounds the wait (`Readable(fd)->OnReceive(f), Timeout(1s, g)`) and a [StopToken](StopToken.md) cancels it, since the wait is a channel like any other. Under them one thread on the kernel's queue (kqueue on macOS and FreeBSD; epoll and IOCP on the other platforms, to come), asleep in the kernel until something is ready, which then signals the channel: a push on the scheduler for the task that waits. The thread starts with the first wait and stops with the scheduler.

This is the foundation the `io` and `net` modules stand on: a socket or a file in them is a descriptor with a buffer, and a read that would block is `co_await Readable(fd)` followed by a read that will not. It is deliberately the smallest thing that does that: one registration per wait (the kernel's one-shot event), a managed channel per wait, no buffers, no descriptors of the library's own.

## Rules

- A wait is one-shot: the channel is signalled once, for the first readiness after the call, and closed. The next wait is a new `Readable(fd)`. A wait left behind (a select that chose another case, a task that stopped) fires when the descriptor is ready and is dropped: nothing to receive it, the channel garbage. Several waits on one descriptor in one direction share one registration and are signalled together by its first readiness.
- The signal is readiness, not data: what a read then gives is the read's business (bytes, zero for the end of the stream, an error). A descriptor the kernel cannot watch (a regular file, on kqueue) is signalled at once, and the read says what is what.
- The channel is a managed object: it lives where a `Ptr` may, and as long as something holds it, the reactor's registration included ([The rules](../../core/README.md#the-rules), 1).
- `Scheduler::Stop()` stops the reactor too: the waits still registered are ended with nothing (their channels closed); the next wait starts it again. A wait registered while the reactor is stopping ends the same way.
- Close a descriptor only when no wait is registered on it: the kernel drops the registration with the descriptor and nothing signals the wait, which stays registered until `Scheduler::Stop()`; a wait on a descriptor the other side may close is bounded by a timeout or a token.
- The reactor thread is a thread of the program to the collector, like any other.

## Members

```cpp
Ptr<Channel<void>> Readable(int fd);
Ptr<Channel<void>> Writable(int fd);
```

```cpp
int fd = /* a socket, a pipe */ 0;
Task<int> ReadOne(int fd) {
    co_await Readable(fd)->AsyncReceive();                 // no thread held
    char c;
    co_return ::read(fd, &c, 1) == 1 ? c : -1;
}
Task<bool> ReadOneWithin(int fd, Duration d) {
    co_return co_await AsyncSelect(
        Readable(fd)->OnReceive([] {}),
        Timeout(d, [] {})
    ) == 0;                                                 // the data came in time
}
```

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>
#include <unistd.h>

using namespace std::chrono_literals;

// An echo over a pipe: a reader task waits for each byte without holding
// a thread, a writer thread sends them slowly; the reader gives up on a
// gap longer than 50 ms. The waits are channels: bounded by a timeout,
// awaited by a task, reclaimed by the collector.
Task<String> ReadAll(int fd) {
    StringBuilder got;                                        // the scratch buffer; the String is made once, at the end
    for (;;) {
        size_t which = co_await AsyncSelect(
            Readable(fd)->OnReceive([] {}),
            Timeout(50ms, [] {})
        );
        if (which == 1) {
            break;                                            // nothing for 50 ms: done
        }
        char c;
        if (::read(fd, &c, 1) != 1) {
            break;                                            // the end of the stream
        }
        got.Append(c);
    }
    co_return got.ToString();
}

int main() {
    int fd[2];
    if (::pipe(fd) != 0) {
        return 1;
    }
    Task reader = Spawn(ReadAll(fd[0]));
    for (char c : String("hello")) {
        ThisThread::SleepFor(5ms);
        [[maybe_unused]] auto n = ::write(fd[1], &c, 1);
    }
    std::cout << reader.Join() << "\n";                       // hello
    ::close(fd[0]);
    ::close(fd[1]);
    return reader.Result() == "hello" ? 0 : 1;
}
```

The output:

```
hello
```

## See also

- [Select](Select.md), [Time](Time.md), [StopToken](StopToken.md): bounding and cancelling a wait; [Channel](Channel.md): what a wait is; [Scheduler](Scheduler.md): what runs the task after
- `tests/Sgcl/sgcl.cpp`: the behaviour above, checked.
