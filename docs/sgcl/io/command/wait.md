[sgcl](../../README.md) › [io](../README.md) › [command](../command.md)

# sgcl::io::command::wait, async_wait

```cpp
/*(1)*/ expected<void, error> wait();
/*(2)*/ async::task<expected<void, error>> async_wait() noexcept;
```

Waits for the child to end, Go's `Cmd.Wait`. How it ended is kept in `state`; the tasks that copy to and from its
pipes are joined, within `wait_delay` when it is set; the program's ends of the pipes are closed (an input's, so
that the child, which has ended, holds nothing; an output's, drained by its task), and the watcher of `stop` ends.

1. Waits on the calling thread, `waitpid`.
2. The same as a task: the end of the child is waited for on the [reactor](../../async/exited.md),
   `async::exited(pid)`, and no thread is held meanwhile.

The command is waited for once, after its start.

## Parameters

None.

## Return value

Nothing when the child exited with 0, or the [error](../error.md):

- `errc::exit_status` when it exited with another status or was ended by a signal (Go's `ExitError`): the operation
  `wait`, the path the program, the code and the signal in `state`;
- `errc::wait_delay` when the copying tasks outlasted `wait_delay` after the child ended;
- the first error of a copying task, when the child exited with 0, as Go's `Wait` gives it: a writer in `out` or
  `err` that failed (a closed file, a full disk), a reader in `in` whose read failed. A write to the input of a child
  that ended without reading all of it (`EPIPE`) is no error;
- `errc::process_done` for a second wait;
- the `errno` of `waitpid` in the system category.

## Complexity

The time of the child, plus the copying of what its pipes still hold.

## Exceptions

- (1) `std::system_error` when the threads of the scheduler, which the join of the copying tasks may start, cannot
  be made.
- (2) None: the task's own exceptions are the task's.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<void> both() {
    io::command ok("true");
    io::command failing("sh", "-c", "exit 3");
    (void)ok.start();
    (void)failing.start();
    auto a = co_await ok.async_wait();
    auto b = co_await failing.async_wait();
    println("{} {}", bool(a), ok.state->to_string());
    println("{}, {}", b.error().is_exit_status(), failing.state->to_string());
}

int main() {
    io::command cmd("echo", "waited for");
    cmd.out = io::stdout;  // the program's own descriptor, inherited
    (void)cmd.start();
    if (auto r = cmd.wait(); r) {
        println("{}", cmd.state->exit_code());
    }
    async::run(both());
}
```

Output:

```text
waited for
0
true exit status 0
true, exit status 3
```

## See also

- [start](start.md): starts the child
- [process_state](../process_state.md): how the child ended
- [process::wait](../process/wait.md): the wait of a process alone
- [sgcl::io::command](../command.md)
