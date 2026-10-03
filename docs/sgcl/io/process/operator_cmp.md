[sgcl](../../README.md) › [io](../README.md) › [process](README.md)

# sgcl::io::operator==, operator!= (sgcl::io::process)

```cpp
friend bool operator==(const process& a, const process& b) noexcept;
```

Checks whether two handles are the same process: whether they hold the same state, which the copies of one handle
share. Two empty handles are equal. `!=` is the negation, which C++20 writes from `==`.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the handles to compare |

## Return value

`true` when both hold the same process, or neither holds one.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::command one("true");
    io::command two("true");
    (void)one.run();
    (void)two.run();
    io::process copy = one.process;
    println("{} {}", copy == one.process, copy != two.process);
}
```

Output:

```text
true true
```

## See also

- [operator bool](operator_bool.md): whether a handle holds a process
- [sgcl::io::process](README.md)
