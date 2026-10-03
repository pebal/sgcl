[sgcl](../../README.md) › [io](../README.md) › [process_state](../process_state.md)

# sgcl::io::process_state::to_string

```cpp
string to_string() const noexcept;
```

Returns the state as text, in the form of Go's `ProcessState.String`: `exit status 1` for a process that exited, and
for one a signal ended `signal: ` followed by the name of the signal as Go writes it: `signal: killed`,
`signal: terminated`, `signal: broken pipe`. The name is the system's description of the signal (`strsignal`) with
its first letter small unless the second is a capital too (`I/O possible`), without the number macOS adds to it; a
signal the system does not name is `signal 34`. A process that left a core file has ` (core dumped)` after it.

## Parameters

None.

## Return value

The text; empty for a process that neither exited nor was ended by a signal.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::command ok("true");
    io::command failed("sh", "-c", "exit 2");
    io::command killed("sh", "-c", "kill -KILL $$");
    (void)ok.run();
    (void)failed.run();
    (void)killed.run();
    println("{}; {}; {}", ok.state->to_string(), failed.state->to_string(), killed.state->to_string());
}
```

Output:

```text
exit status 0; exit status 2; signal: killed
```

## See also

- [exit_code](exit_code.md), [signal](signal.md): the parts of the text
- [sgcl::io::process_state](../process_state.md)
