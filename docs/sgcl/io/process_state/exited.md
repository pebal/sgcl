[sgcl](../../README.md) › [io](../README.md) › [process_state](README.md)

# sgcl::io::process_state::exited

```cpp
bool exited() const noexcept;
```

Checks whether the process ended by exiting, by `exit` or a return from its `main`, rather than by a signal:
`WIFEXITED`.

## Parameters

None.

## Return value

`true` when the process exited.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::command done("sh", "-c", "exit 2");
    io::command killed("sh", "-c", "kill -9 $$");
    (void)done.run();
    (void)killed.run();
    println("{} {}", done.state->exited(), killed.state->exited());
}
```

Output:

```text
true false
```

## See also

- [exit_code](exit_code.md): the code it exited with
- [signaled](signaled.md): ended by a signal
- [sgcl::io::process_state](README.md)
