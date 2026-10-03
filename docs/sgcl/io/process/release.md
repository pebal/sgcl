[sgcl](../../README.md) › [io](../README.md) › [process](README.md)

# sgcl::io::process::release

```cpp
expected<void, error> release() const noexcept;
```

Lets the process go without a wait, Go's `Process.Release`: its resources are the system's once it ends, and the
handle answers nothing more, a wait or a signal after it being `errc::process_done`.

## Parameters

None.

## Return value

Nothing, or the [error](../error/README.md) `errc::process_done` when the process was waited for or released already; the
operation is `release`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::command background("true");
    (void)background.start();
    println("{}", background.process.release().has_value());
    println("{}", background.process.wait().error().message());
}
```

Output:

```text
true
wait: process already finished
```

## See also

- [wait](wait.md): waits for the process
- [sgcl::io::process](README.md)
