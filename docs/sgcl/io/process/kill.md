[sgcl](../../README.md) › [io](../README.md) › [process](../process.md)

# sgcl::io::process::kill

```cpp
expected<void, error> kill() const noexcept;
```

Ends the process now, Go's `Process.Kill`: [signal](signal.md) with `SIGKILL`, which the process cannot catch.

## Parameters

None.

## Return value

Nothing, or the [error](../error.md) of `signal`: `errc::process_done` after the wait or the release.

## Complexity

Constant: one call to the system.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::command sleeper("sleep", "10");
    (void)sleeper.start();
    (void)sleeper.process.kill();
    auto ran = sleeper.wait();
    println("{}, exit code {}", ran.error().is_exit_status(), sleeper.state->exit_code());
}
```

Output:

```text
true, exit code -1
```

## See also

- [signal](signal.md): any signal
- [command](../command.md): `stop`, the kill on a stop token
- [sgcl::io::process](../process.md)
