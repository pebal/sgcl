[sgcl](../../README.md) › [io](../README.md) › [error](README.md)

# sgcl::io::error::is_exit_status

```cpp
bool is_exit_status() const noexcept;
```

Checks whether a child process ended with a failure status: `errc::exit_status`, which the wait of a
[command](../command/README.md) reports, as Go's `*exec.ExitError`. The status itself is in the command's state.

## Parameters

None.

## Return value

`true` when the code is `errc::exit_status`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::command test("sh", "-c", "exit 3");
    auto r = test.run();
    println("{}", r.error().is_exit_status());
}
```

Output:

```text
true
```

## See also

- [command](../command/README.md), [process_state](../process_state/README.md): the child and its status
- [sgcl::io::error](README.md)
