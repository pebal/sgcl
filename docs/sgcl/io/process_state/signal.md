[sgcl](../../README.md) › [io](../README.md) › [process_state](README.md)

# sgcl::io::process_state::signal

```cpp
int signal() const noexcept;
```

Returns the signal that ended the process, `WTERMSIG`, or 0 when it exited.

## Parameters

None.

## Return value

The number of the signal, or 0.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include <csignal>

using namespace sgcl;

int main() {
    io::command terminated("sh", "-c", "kill -TERM $$");
    io::command done("true");
    (void)terminated.run();
    (void)done.run();
    println("{} {}", terminated.state->signal() == SIGTERM, done.state->signal());
}
```

Output:

```text
true 0
```

## See also

- [signaled](signaled.md): whether a signal ended the process
- [process::signal](../process/signal.md): sends a signal
- [sgcl::io::process_state](README.md)
