[sgcl](../../README.md) › [io](../README.md)

# sgcl::io::process

```cpp
#include "sgcl/io/exec.h"   // or "sgcl/io.h"

namespace sgcl::io {
    class process;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`io::process` is a running child, Go's `os.Process`: its id, a signal, the one wait. It is made by
[command::start](../command/start.md), which keeps it in the command's `process`; a default-constructed one holds no
child. It is a handle of one word, a `tracked_ptr` to the state the copies share: copied and passed by value, every
copy the same child, and a wait through any copy is the one wait.

Its destructor waits for nothing, since a wait in a destructor would block the sweep, and releases nothing: a child
never waited for stays a zombie until the program ends, as everywhere.

## Rules

- The child is waited for once, from a thread ([wait](wait.md)) or a task (`co_await async_wait()`), or let
  go of by [release](release.md). After either, [signal](signal.md) and [kill](kill.md) are
  `errc::process_done`: the id may be another process's by then.
- An operation on an empty handle is a contract violation, asserted in a debug build.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](process.md) | an empty handle |
| [pid](pid.md) | the process id |
| [signal](signal.md) | sends a signal |
| [kill](kill.md) | sends `SIGKILL` |
| [wait, async_wait](wait.md) | waits for the process to end |
| [release](release.md) | lets the process go without a wait |
| [operator bool](operator_bool.md) | checks whether the handle holds a process |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator!=](operator_cmp.md) | checks whether two handles are the same process |

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

- [command](../command/README.md): the program that makes the process
- [process_state](../process_state/README.md): how the process ended
- [pid](../pid.md): the id of the program's own process
