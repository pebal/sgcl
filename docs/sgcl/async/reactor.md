# sgcl::readable, sgcl::writable

```cpp
#include "sgcl/async/reactor.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    tracked_ptr<channel<void>> readable(int fd);   // one signal when fd can be read without blocking, then closed
    tracked_ptr<channel<void>> writable(int fd);   // the same for a write
    tracked_ptr<channel<void>> exited(int pid);    // one signal when the process has ended (its status still to collect)
    void cancel_waits(int fd);                     // the waits on fd ended with nothing: what a close of the descriptor calls
}
```

The reactor: the readiness of file descriptors, and the end of a child process, as channels. `readable(fd)` is a channel that gets one signal when `fd` can be read without blocking (data, or the end of the stream), and is closed then; `writable(fd)` the same for a write; `exited(pid)` gets its signal when the process has ended (kqueue's `EVFILT_PROC`, a pidfd on Linux: what [io::command](../io/exec.md) waits on, no thread per child, as Go 1.23), its status then collected without blocking. A task writes `co_await readable(fd)->async_receive()` and holds no thread until the data comes; a [select](select.md) bounds the wait (`readable(fd)->on_receive(f), timeout(1s, g)`) and a [stop_token](stop_token.md) cancels it, since the wait is a channel like any other. Under them one thread on the kernel's queue (kqueue on macOS and FreeBSD; epoll and IOCP on the other platforms, to come), asleep in the kernel until something is ready, which then signals the channel: a push on the scheduler for the task that waits. The thread starts with the first wait and stops with the scheduler.

This is the foundation the `io` and `net` modules stand on: a socket or a file in them is a descriptor with a buffer, and a read that would block is `co_await readable(fd)` followed by a read that will not. It is deliberately the smallest thing that does that: one registration per wait (the kernel's one-shot event), a managed channel per wait, no buffers, no descriptors of the library's own.

## Rules

- A wait is one-shot: the channel is signalled once, for the first readiness after the call, and closed. The next wait is a new `readable(fd)`. A wait left behind (a select that chose another case, a task that stopped) fires when the descriptor is ready and is dropped: nothing to receive it, the channel garbage. Several waits on one descriptor in one direction share one registration and are signalled together by its first readiness.
- The signal is readiness, not data: what a read then gives is the read's business (bytes, zero for the end of the stream, an error). A descriptor the kernel cannot watch (a regular file, on kqueue) is signalled at once, and the read says what is what.
- The channel is a managed object: it lives where a `tracked_ptr` may, and as long as something holds it, the reactor's registration included ([The rules](../core/README.md#the-rules), 1).
- `scheduler::stop()` stops the reactor too: the waits still registered are ended with nothing (their channels closed); the next wait starts it again. A wait registered while the reactor is stopping ends the same way.
- A descriptor closed with a wait registered: the kernel drops the registration with the descriptor and nothing would signal the wait, and the reactor's own record of it would take the waits of the next descriptor with the same number; so a close calls `cancel_waits(fd)` first, which ends the waits on the descriptor with nothing (their channels closed) — `file::close` does. A descriptor the program closes by hand while a task waits on it calls `cancel_waits(fd)` itself.
- The reactor thread is a thread of the program to the collector, like any other.

## Members

```cpp
tracked_ptr<channel<void>> readable(int fd);
tracked_ptr<channel<void>> writable(int fd);
tracked_ptr<channel<void>> exited(int pid);
void cancel_waits(int fd);
```

```cpp
int fd = /* a socket, a pipe */ 0;
task<int> read_one(int fd) {
    co_await readable(fd)->async_receive();          // no thread held
    char c;
    co_return ::read(fd, &c, 1) == 1 ? c : -1;
}
task<io::result<io::process_state>> ended(tracked_ptr<io::process> p) {   // what io::process::async_wait does
    co_await exited(p->pid())->async_receive();        // no thread held while the child runs
    co_return p->wait();                                // the status, without blocking: the child has ended
}
task<bool> read_one_within(int fd, duration d) {
    co_return co_await async_select(
        readable(fd)->on_receive([] {}),
        timeout(d, [] {})
    ) == 0;                                                 // the data came in time
}
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>
#include <string>
#include <unistd.h>

using namespace sgcl;

using namespace std::chrono_literals;

// An echo over a pipe: a reader task waits for each byte without holding
// a thread, a writer thread sends them slowly; the reader gives up on a
// gap longer than 50 ms. The waits are channels: bounded by a timeout,
// awaited by a task, reclaimed by the collector.
task<string> read_all(int fd) {
    std::string got;                                          // the scratch buffer; the string is made once, at the end
    for (;;) {
        size_t which = co_await async_select(
            readable(fd)->on_receive([] {}),
            timeout(50ms, [] {})
        );
        if (which == 1) {
            break;                                            // nothing for 50 ms: done
        }
        char c;
        if (::read(fd, &c, 1) != 1) {
            break;                                            // the end of the stream
        }
        got += c;
    }
    co_return string(got);
}

int main() {
    int fd[2];
    if (::pipe(fd) != 0) {
        return 1;
    }
    auto reader = spawn(read_all(fd[0]));
    for (char c : string("hello")) {
        this_thread::sleep_for(5ms);
        [[maybe_unused]] auto n = ::write(fd[1], &c, 1);
    }
    std::cout << reader.join() << "\n";                       // hello
    ::close(fd[0]);
    ::close(fd[1]);
    return reader.result() == "hello" ? 0 : 1;
}
```

The output:

```
hello
```

## See also

- [select](select.md), [timer](timer.md), [stop_token](stop_token.md): bounding and cancelling a wait; [channel](channel.md): what a wait is; [scheduler](scheduler.md): what runs the task after
- `tests/async/reactor.cpp`: every behaviour above, checked.
