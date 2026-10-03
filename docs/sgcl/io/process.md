[sgcl](../README.md) › [io](README.md)

# sgcl::io::process

```cpp
#include "sgcl/io/exec.h"   // or "sgcl/io.h"

namespace sgcl::io {
    class process;
}
```

`io::process` is a running child, Go's `os.Process`: its id, a signal, the one wait. It is made by
[command::start](command/start.md), which keeps it in the command's `process`; a default-constructed one holds no
child. It is a handle of one word, a `tracked_ptr` to the state the copies share: copied and passed by value, every
copy the same child, and a wait through any copy is the one wait.

Its destructor waits for nothing, since a wait in a destructor would block the sweep, and releases nothing: a child
never waited for stays a zombie until the program ends, as everywhere.

## Rules

- A handle is a tracked word: on a stack, in a task, in a managed object; in a global or a `std` container, a
  [rooted\<io::process\>](../core/rooted.md), never in a managed object or a task's frame, since a root is never
  part of a cycle.
- The child is waited for once, from a thread ([wait](process/wait.md)) or a task (`co_await async_wait()`), or let
  go of by [release](process/release.md). After either, [signal](process/signal.md) and [kill](process/kill.md) are
  `errc::process_done`: the id may be another process's by then.
- An operation on an empty handle is a contract violation, asserted in a debug build.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](process/process.md) | an empty handle |
| [pid](process/pid.md) | the process id |
| [signal](process/signal.md) | sends a signal |
| [kill](process/kill.md) | sends `SIGKILL` |
| [wait, async_wait](process/wait.md) | waits for the process to end |
| [release](process/release.md) | lets the process go without a wait |
| [operator bool](process/operator_bool.md) | checks whether the handle holds a process |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator!=](process/operator_cmp.md) | checks whether two handles are the same process |

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::command sleeper("sleep", "10");
    if (!sleeper.start()) {
        return 1;
    }
    io::process child = sleeper.process;  // the same child
    (void)child.kill();
    if (auto ended = child.wait()) {
        println("signaled: {}", ended->signaled());
    }
    println("{}", child.kill().error().message());
}
```

Output:

```text
signaled: true
signal: process already finished
```

## See also

- [command](command.md): the program that makes the process
- [process_state](process_state.md): how the process ended
- [pid](pid.md): the id of the program's own process
