[sgcl](../../README.md) › [io](../README.md) › [process_state](README.md)

# sgcl::io::process_state::success

```cpp
bool success() const noexcept;
```

Checks whether the process exited with 0: what the [wait](../command/wait.md) of a [command](../command/README.md) takes for
success, any other end being `errc::exit_status`.

## Parameters

None.

## Return value

`true` when the process exited with 0.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::command yes("true");
    io::command no("false");
    (void)yes.run();
    (void)no.run();
    println("{} {}", yes.state->success(), no.state->success());
}
```

Output:

```text
true false
```

## See also

- [exit_code](exit_code.md): the code it exited with
- [sgcl::io::process_state](README.md)
