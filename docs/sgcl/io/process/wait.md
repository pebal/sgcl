[sgcl](../../README.md) › [io](../README.md) › [process](README.md)

# sgcl::io::process::wait, async_wait

```cpp
expected<process_state, error> wait() const noexcept;                       // (1)
async::task<expected<process_state, error>> async_wait() const noexcept;    // (2)
```

Waits for the process to end and returns how it ended, Go's `Process.Wait`. The process is waited for once: a second
wait, or one after [release](release.md), is refused. This is the wait of the process alone; a
[command](../command/README.md)'s [wait](../command/wait.md) waits for it through this one and also joins the tasks that
serve its pipes.

1. Waits on the calling thread, `wait4`, which gives the status and the times of the process.
2. The same as a task: the end is waited for on the [reactor](../../async/exited.md), `async::exited(pid)`, no
   thread held meanwhile, and the status collected once it came.

## Parameters

None.

## Return value

How the process ended, a [process_state](../process_state/README.md), or the [error](../error/README.md): `errc::process_done`
for a second wait or one after the release, else the `errno` of `wait4`; the operation is `wait`. A failure status
is no error here: it is in the state.

## Complexity

The time of the process.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<void> watch(io::process p) {
    auto ended = co_await p.async_wait();
    println("{}", ended->to_string());
}

int main() {
    io::command three("sh", "-c", "exit 3");
    (void)three.start();
    async::run(watch(three.process));
    println("{}", three.process.wait().error().message());
}
```

Output:

```text
exit status 3
wait: process already finished
```

## See also

- [command::wait](../command/wait.md): the wait of a command, its pipes joined
- [process_state](../process_state/README.md): how the process ended
- [sgcl::io::process](README.md)
