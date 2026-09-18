# sgcl::signals

```cpp
#include "sgcl/async/signal.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    tracked_ptr<channel<int>> signals(std::initializer_list<int> numbers, size_t capacity = 1);   // the number of every signal delivered, from now on
    void reset_signals(std::initializer_list<int> numbers = {});   // the disposition from before back; every registered number, for an empty list
    void ignore_signals(std::initializer_list<int> numbers);       // the numbers ignored by the process
}
```

The same in the `Sgcl` interface: [Signals](../Sgcl/Async/Signal.md).

The signals of the process as a channel, the way Go's `os/signal` has them: `signals({SIGINT, SIGTERM})` returns a channel that gets the number of every signal of those delivered to the process from then on, so that a signal is waited for as anything else in the module is: a task `co_await`s the channel and holds no thread meanwhile, a thread receives on it, a select takes it as a case. The shutdown of a server is one more case of the loop that serves it:

```cpp
auto stop = sgcl::signals({SIGINT, SIGTERM});
while (running) {
    co_await sgcl::async_select(
        jobs.on_receive([&](Job j) { serve(j); }),
        stop->on_receive([&](int) { running = false; })
    );
}
```

A signal handler may do almost nothing, so the delivery is in two steps: the handler the registration installs writes the number as one byte to a pipe of the module (`write` is one of the calls a handler may make; the pipe's write end is non-blocking, so the handler never blocks), and one thread of the module, blocked in a `read` on the pipe's other end, sends the number on every channel registered for it, without waiting: a channel that is full drops the delivery. A burst of signals is therefore coalesced to the capacity of the channel, one by default, as Go recommends and as the kernel does anyway, a pending signal being one bit per number, not a count. A channel registered for several numbers gets each of them; several channels registered for one number each get it. The channel is held by the registration, so it lives while the process listens, wherever the `tracked_ptr` returned went.

`reset_signals({SIGINT})` gives the number the disposition it had before the first registration, the default action or a handler the program had installed (Go's `signal.Reset`); `ignore_signals({SIGPIPE})` sets it to be ignored (`signal.Ignore`), and a later reset undoes that too. Both forget the channels registered for the numbers, and neither closes them, since a channel may serve other numbers still. At the end of the program the dispositions are given back and the thread joined.

## Rules

- POSIX for now (`sigaction`, a pipe): macOS, Linux, the BSDs; Windows comes with the platform matrix. `SIGKILL` and `SIGSTOP` cannot be caught, and the registration for them fails silently, as `sigaction` does.
- A registration replaces the disposition of the number for the whole process: a handler the program had installed is saved and no longer runs until `reset_signals`. The handler installed sets `SA_RESTART`, so a system call the signal interrupts is restarted, as in Go.
- The delivery is asynchronous: a `raise` returns before the number is on the channel (the handler ran, the thread has not yet), so a test that raises and then polls waits a moment, or receives.
- A delivery a channel has no room for is dropped, not queued: a receiver that needs every signal of a burst, if the kernel delivers them at all, gives the channel a capacity.
- The thread of the module is a thread of the program to the collector, like any other; the send on a channel is a `try_send`, a push on the scheduler for a task that waits.

## Members

### signals

```cpp
tracked_ptr<channel<int>> signals(std::initializer_list<int> numbers, size_t capacity = 1);
```

The channel, registered for the numbers; the handler installed for each, the module's thread started on the first call.

### reset_signals, ignore_signals

```cpp
void reset_signals(std::initializer_list<int> numbers = {});   // the saved disposition back; every registered number for {}
void ignore_signals(std::initializer_list<int> numbers);       // SIG_IGN; reset_signals undoes it
```

```cpp
auto usr = sgcl::signals({SIGUSR1, SIGUSR2});
std::raise(SIGUSR2);                                             // what another process's kill would do
int n = *usr->receive();                                         // SIGUSR2
auto t = sgcl::spawn([](sgcl::tracked_ptr<sgcl::channel<int>> usr) -> sgcl::task<int> {
    co_return *co_await usr->async_receive();                    // no thread held while the process waits
}(usr));
std::raise(SIGUSR1);
int m = t.join();                                                // SIGUSR1
sgcl::ignore_signals({SIGUSR2});                                 // ignored from now on: nothing on the channel
sgcl::reset_signals();                                           // both as they were before the first signals()
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <csignal>
#include <iostream>

// A server that serves its jobs until the process gets SIGINT or
// SIGTERM: the signal is a case of the select, like the jobs. The
// program sends itself the SIGINT that Ctrl-C would.
sgcl::task<int> serve(sgcl::channel<int>& jobs, sgcl::tracked_ptr<sgcl::channel<int>> stop, int& by) {
    int done = 0;
    bool running = true;
    while (running) {
        co_await sgcl::async_select(
            jobs.on_receive([&](int) { ++done; }),
            stop->on_receive([&](int n) { by = n; running = false; })
        );
    }
    co_return done;
}

int main() {
    sgcl::tracked_ptr stop = sgcl::signals({SIGINT, SIGTERM});
    sgcl::channel<int> jobs;                  // a rendezvous: a send returns once the server took the job
    int by = 0;
    auto server = sgcl::spawn(serve(jobs, stop, by));
    for (int i : sgcl::range(5)) {
        jobs.send(i);
    }
    std::raise(SIGINT);                       // Ctrl-C
    int done = server.join();
    std::cout << done << " jobs done, stopped by signal " << by << (by == SIGINT ? " (SIGINT)" : "") << "\n";
    sgcl::reset_signals();                    // SIGINT and SIGTERM as they were
    sgcl::scheduler::stop();
}
```

The output:

```
5 jobs done, stopped by signal 2 (SIGINT)
```

## See also

- [channel](channel.md): what `signals` returns; [select](select.md): the signal as a case; [stop_token](stop_token.md): the stop the signal usually requests, handed down as a token
- `tests/async/signal.cpp`: every behaviour above, checked, under the thread sanitizer too.
