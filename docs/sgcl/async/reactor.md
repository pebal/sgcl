# sgcl::async::readable, sgcl::async::writable

```cpp
#include "sgcl/async/reactor.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    async::event async::readable(int fd);   // set when fd can be read without blocking, or when the wait is ended with nothing
    async::event async::writable(int fd);   // the same for a write
    async::event async::exited(int pid);    // set when the process has ended (its status still to collect)
    void async::cancel_waits(int fd);                     // the waits on fd ended with nothing: what a close of the descriptor calls
}
```

The reactor: the readiness of file descriptors, and the end of a child process, as [events](event.md). `async::readable(fd)` is an event set when `fd` can be read without blocking (data, or the end of the stream); `async::writable(fd)` the same for a write; `async::exited(pid)` is set when the process has ended (kqueue's `EVFILT_PROC`; on Linux for now a thread in `waitid`, a pidfd to come: what [io::command](../io/exec.md) waits on, no thread per child, as Go 1.23), its status then collected without blocking. A task writes `co_await async::readable(fd)` and holds no thread until the data comes, a thread `async::readable(fd).wait()`; a [select](select.md) bounds the wait (`async::readable(fd).on_set(f), async::timeout(1s, g)`) and a [stop_token](stop_token.md) cancels it, since the wait is an event like any other. An event is set by the readiness or by the end of the wait with nothing (a cancel, a stop of the reactor): a wait woken by it looks at the descriptor again — the read says whether there is data, and a read that would block waits again. Under them one thread on the kernel's queue (kqueue on macOS and FreeBSD, epoll on Linux, IOCP to come), asleep in the kernel until something is ready, which then sets the event: a push on the scheduler for the task that waits. The thread starts with the first wait and stops with the scheduler.

The descriptors of the `io` and `net` modules (a socket, a pipe) do not wait this way: each is registered with the kernel once, at its first wait in a direction, edge-triggered (kqueue's `EV_CLEAR`, epoll's `EPOLLET`), and stays registered until it is closed. A read that would block tries the call, then parks the task in the descriptor's slot on the reactor, one word per direction that the task and the reactor's thread change with compare-exchanges: no system call and no lock per wait, and an edge that comes between the call's `EAGAIN` and the park is kept in the word, so the park answers at once and the call is tried again (Go's `pollDesc`, in this library's terms). Several waiters in one direction (tasks accepting on one listener) wait behind the first and are woken, to try again, whenever it gives the direction up. A close, a deadline, or a stop of the reactor wakes the waiters without readiness, and each tells why. A descriptor the kernel refuses to register, or whose number is past the reactor's table (four million), fails the wait with the reason (the registration's `errno`, or `io::errc::unsupported`) rather than answering it at once, where the operation would try its call again forever. This page's events stay for everything else: a descriptor of the program's own, and the end of a child process. The two ways do not mix on one descriptor number, since the kernel keeps one entry per number and filter.

## Rules

- A wait is one-shot: the event is set once, for the first readiness after the call. The next wait is a new `async::readable(fd)`. A wait left behind (a select that chose another case, a task that stopped) stays on the registration until the descriptor is ready, then fires and is dropped: nothing waits on it, the event garbage. A wait given up on a descriptor that may stay idle (a read with a short deadline in a loop, as `net` makes) is ended by setting its event: the reactor drops the set ones at later waits on the descriptor, and holds no more than about twice the waits still open. Several waits on one descriptor in one direction share one registration and are set together by its first readiness.
- The event is readiness, not data, and it is set by the end of a wait too: what a read then gives is the read's business (bytes, zero for the end of the stream, an error, `EAGAIN` for a wait ended with nothing, after which it waits again). A descriptor the kernel cannot watch (a regular file, on kqueue) is set at once, and the read says what is what.
- The event is a handle ([event](event.md)): one word, a tracked word — on a stack, in a task, in a managed object; in a global or a std container, a `rooted<async::event>`. Its state lives as long as something holds it, the reactor's registration included ([The rules](../core/README.md#the-rules), 1).
- The kernel's queue is made by the first wait; when it cannot be (`kqueue()` failing with the descriptors exhausted) that wait ends with nothing, and the next one tries again.
- `async::scheduler::stop()` stops the reactor too: the waits still registered are ended with nothing (their events set), and so are the descriptors' parked waits (the operation fails with `ECANCELED`); the next wait starts it again and registers the descriptors again. A wait registered while the reactor is stopping ends the same way.
- A descriptor closed with a wait registered: the kernel drops the registration with the descriptor and nothing would signal the wait, and the reactor's own record of it would take the waits of the next descriptor with the same number; so a close calls `async::cancel_waits(fd)` first, which ends the waits on the descriptor with nothing (their events set) — `file::close` does. An event the kernel gives for such a registration meanwhile is dropped. A descriptor the program closes by hand while a task waits on it calls `async::cancel_waits(fd)` itself.
- The reactor thread is a thread of the program to the collector, like any other.

## Members

```cpp
async::event async::readable(int fd);
async::event async::writable(int fd);
async::event async::exited(int pid);
void async::cancel_waits(int fd);
```

```cpp
int fd = /* a socket, a pipe */ 0;
async::task<int> read_one(int fd) {
    co_await async::readable(fd);                     // no thread held
    char c;
    co_return ::read(fd, &c, 1) == 1 ? c : -1;
}
async::task<expected<io::process_state, io::error>> ended(io::process child) {   // what io::process::wait does
    co_await async::exited(child.pid());                // no thread held while the child runs
    co_return child.wait();                             // the status, without blocking: the child has ended
}
async::task<bool> read_one_within(int fd, duration d) {
    co_return co_await async::select(
        async::readable(fd).on_set([] {}),
        async::timeout(d, [] {})
    ) == 0;                                                 // the data came in time
}
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <string>
#include <unistd.h>

using namespace sgcl;

using namespace std::chrono_literals;

// An echo over a pipe: a reader task waits for each byte without holding
// a thread, a writer thread sends them slowly; the reader gives up on a
// gap longer than 50 ms. The waits are events: bounded by a timeout,
// awaited by a task, reclaimed by the collector.
async::task<string> read_all(int fd) {
    std::string got;                                          // the scratch buffer; the string is made once, at the end
    for (;;) {
        size_t which = co_await async::select(
            async::readable(fd).on_set([] {}),
            async::timeout(50ms, [] {})
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
    auto reader = async::spawn(read_all(fd[0]));
    for (char c : string("hello")) {
        this_thread::sleep_for(5ms);
        [[maybe_unused]] auto n = ::write(fd[1], &c, 1);
    }
    println("{}", reader.wait());                       // hello
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

- [select](select.md), [timer](timer.md), [stop_token](stop_token.md): bounding and cancelling a wait; [event](event.md): what a wait is; [scheduler](scheduler.md): what runs the task after
- `tests/async/reactor.cpp`: every behaviour above, checked.
