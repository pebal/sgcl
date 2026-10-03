[sgcl](../../README.md) › [io](../README.md)

# sgcl::io::process_state

```cpp
#include "sgcl/io/exec.h"   // or "sgcl/io.h"

namespace sgcl::io {
    class process_state;
}
```

`io::process_state` is how a process ended, Go's `os.ProcessState`: its id, its exit code or the signal that ended
it, and the processor time it used. It is a plain value made by a wait ([process::wait](../process/wait.md), and
[command::wait](../command/wait.md), which keeps it in the command's `state`); a default-constructed one is the state of
no process, id 0, exited with 0.

It is copied field by field, not as bytes, so it is not trivially copyable: a trivially copyable state would make
`optional<process_state>` trivially copyable, and a command moved off a stack would carry the stack's leftover words
in the storage of its empty `state` into a managed object, where the debug check of the collector would take a stale
word for a pointer.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](process_state.md) | an empty state, or a copy |
| [operator=](operator_assign.md) | copies a state |
| [pid](pid.md) | the process id |
| [exited](exited.md) | checks whether the process ended by exiting |
| [exit_code](exit_code.md) | the exit code |
| [success](success.md) | checks whether the process exited with 0 |
| [signaled](signaled.md) | checks whether a signal ended the process |
| [signal](signal.md) | the signal that ended the process |
| [user_time](user_time.md) | the processor time in user mode |
| [system_time](system_time.md) | the processor time in the kernel |
| [to_string](to_string.md) | the state as Go writes it, `exit status 1` |

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::command cmd("sh", "-c", "exit 4");
    (void)cmd.run();
    if (cmd.state) {
        const io::process_state& st = *cmd.state;
        println("{} {} {} {}", st.exited(), st.exit_code(), st.success(), st.signaled());
        println("{}", st.to_string());
    }
}
```

Output:

```text
true 4 false false
exit status 4
```

## See also

- [process](../process/README.md): the running process
- [command](../command/README.md): `state`, kept by the wait
