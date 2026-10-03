[sgcl](../../README.md) › [io](../README.md) › [command](../command.md)

# sgcl::io::command::combined_output, async_combined_output

```cpp
expected<string, error> combined_output();                                // (1)
async::task<expected<string, error>> async_combined_output() noexcept;    // (2)
```

Runs the child with its standard output and its standard error captured together, in one pipe, in the order it
wrote them, Go's `Cmd.CombinedOutput`. `out` and `err` must be empty, and the command not started.

1. Waits on the calling thread.
2. The same as a task, the end of the child waited for on the [reactor](../../async/exited.md).

## Parameters

None.

## Return value

What the child wrote to both streams, or the [error](../error.md): `std::errc::invalid_argument` (operation
`combined_output`) when `out` or `err` is set or the command was started, else the error of [run](run.md).

## Complexity

The time of the child, plus linear in what it writes.

## Exceptions

- (1) `std::system_error` when the threads of the scheduler, which the start or the wait may start, cannot be made;
  `length_error` when the output passes 4 GiB, the most a string holds.
- (2) None: the task's own exceptions are the task's.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::command sh("sh", "-c", "echo out; echo err >&2; echo out again");
    if (auto both = sh.combined_output()) {
        print("{}", *both);
    }
}
```

Output:

```text
out
err
out again
```

## See also

- [output](output.md): the standard output alone
- [sgcl::io::command](../command.md)
