[sgcl](../../README.md) › [io](../README.md) › [process_state](../process_state.md)

# sgcl::io::process_state::signaled

```cpp
bool signaled() const noexcept;
```

Checks whether a signal ended the process, `WIFSIGNALED`.

## Parameters

None.

## Return value

`true` when a signal ended the process.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::command killed("sh", "-c", "kill -9 $$");
    (void)killed.run();
    println("{} {}", killed.state->signaled(), killed.state->signal());
}
```

Output:

```text
true 9
```

## See also

- [signal](signal.md): which signal
- [exited](exited.md): ended by exiting
- [sgcl::io::process_state](../process_state.md)
