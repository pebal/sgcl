[sgcl](../README.md) › [async](README.md)

# sgcl::async::signals

```cpp
#include "sgcl/async/signal.h"   // or "sgcl/async.h"

namespace sgcl::async {
    channel<int> signals(std::initializer_list<int> numbers, size_t capacity = 1);
}
```

Returns a channel that gets the number of every signal of `numbers` delivered to the process from now on, the way
Go's `os/signal` has them: a signal is waited for as anything else in the module is. A task `co_await`s a receive
on the channel and holds no thread meanwhile, a thread receives on it, a [select](select.md) takes it as a case,
so the shutdown of a server is one more case of the loop that serves it.

A signal handler may do almost nothing, so the delivery is in two steps. The handler the registration installs
writes the number as one byte to a pipe of the module (`write` is one of the calls a handler may make; the pipe's
write end is non-blocking, so the handler never blocks, and a full pipe drops the signal), and one thread of the
module, blocked in a `read` on the pipe's other end, sends the number on every channel registered for it, without
waiting: a channel that is full drops the delivery. A burst of signals is therefore coalesced to the capacity of
the channel, one by default, as Go recommends and as the kernel does anyway, a pending signal being one bit per
number, not a count.

A channel registered for several numbers gets each of them; several channels registered for one number each get
it. The channel is held by the registration, so it lives while the process listens, wherever the handle returned
went, until [reset_signals](reset_signals.md) or [ignore_signals](ignore_signals.md) forgets it.

## Parameters

| Parameter | Description |
|---|---|
| `numbers` | the signals to listen for (`SIGINT`, `SIGTERM`, `SIGHUP`, …) |
| `capacity` | the numbers the channel holds for a receiver that is not there yet; the rest are dropped |

## Return value

The channel of the numbers delivered: a [channel](channel.md) as any other, a handle copied into the tasks that
wait on it, and in a global a `rooted<async::channel<int>>` ([Handles](README.md#handles)).

## Complexity

Linear in `numbers`: a `sigaction` per number, the disposition before the first registration saved once per
number. The first call creates the pipe and starts the module's thread.

## Exceptions

`std::system_error` when the module's pipe or its thread cannot be made, on the first registration or the first
after the end of the module's thread (the code is `errno`'s, `too_many_files_open` for a process out of
descriptors); then no handler is installed, and a later call tries again. On Windows always, with
`std::errc::function_not_supported`: the signals are POSIX for now.

## Notes

- POSIX for now (`sigaction`, a pipe): macOS, Linux, the BSDs; Windows comes with the platform matrix, and until
  then the call compiles there and throws. `SIGKILL` and `SIGSTOP` cannot be caught, and a registration for them
  fails silently, as `sigaction` does.
- A registration replaces the disposition of the number for the whole process: a handler the program had
  installed is saved and no longer runs until [reset_signals](reset_signals.md). The handler installed sets
  `SA_RESTART`, so a system call the signal interrupts is restarted, as in Go.
- The delivery is asynchronous: a `raise` returns before the number is on the channel (the handler ran, the
  thread has not yet), so a program that raises and then looks waits on a receive rather than polls once.
- A delivery a channel has no room for is dropped, not queued: a receiver that needs every signal of a burst, if
  the kernel delivers them at all, gives the channel a capacity.
- The module's thread is a thread of the program to the collector, like any other; its send is a `try_send`, a
  push on the scheduler for a task that waits. At the end of the program the dispositions are given back and the
  thread joined.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <csignal>

using namespace sgcl;

// A server that serves its jobs until the process gets SIGINT or SIGTERM:
// the signal is a case of the select, like the jobs
async::task<int> serve(async::channel<int> jobs, async::channel<int> stop) {
    int done = 0;
    bool running = true;
    while (running) {
        co_await async::select(
            jobs.on_receive([&](int) { ++done; }),
            stop.on_receive([&](int) { running = false; })
        );
    }
    co_return done;
}

int main() {
    async::channel<int> stop = async::signals({SIGINT, SIGTERM});
    async::channel<int> jobs;  // a rendezvous: a send returns once the server took the job
    auto server = async::spawn(serve(jobs, stop));
    for (int i : range(5)) {
        jobs.send(i).wait();
    }
    std::raise(SIGINT);  // what Ctrl-C does
    println("{} jobs done", server.wait());
    async::reset_signals();
}
```

Output:

```text
5 jobs done
```

## See also

- [reset_signals](reset_signals.md): the disposition from before back
- [ignore_signals](ignore_signals.md): the signals ignored
- [run](run.md): the entry of a program, whose token the first SIGINT or SIGTERM stops, over these channels
- [stop_token](stop_token.md): the stop a signal usually requests, handed down as a token
- [channel](channel.md), [select](select.md): what `signals` returns, and where it is a case
