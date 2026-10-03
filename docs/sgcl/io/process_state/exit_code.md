[sgcl](../../README.md) › [io](../README.md) › [process_state](../process_state.md)

# sgcl::io::process_state::exit_code

```cpp
int exit_code() const noexcept;
```

Returns the code the process exited with, `WEXITSTATUS`, or -1 when it did not exit: when a signal ended it.

## Parameters

None.

## Return value

The exit code, 0 to 255, or -1.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::command done("sh", "-c", "exit 7");
    io::command killed("sh", "-c", "kill -9 $$");
    (void)done.run();
    (void)killed.run();
    println("{} {}", done.state->exit_code(), killed.state->exit_code());
}
```

Output:

```text
7 -1
```

## See also

- [success](success.md): exited with 0
- [exited](exited.md): ended by exiting
- [sgcl::io::process_state](../process_state.md)
