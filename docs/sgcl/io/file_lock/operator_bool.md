[sgcl](../../README.md) › [io](../README.md) › [file_lock](README.md)

# sgcl::io::file_lock::operator bool

```cpp
explicit operator bool() const noexcept;
```

Checks whether the guard holds a lock: `false` for one made by the default constructor, moved from, or unlocked.

## Parameters

None.

## Return value

`true` when a lock waits to be given back by it.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::file_lock held = io::lock_file("b.lock").value();
    println("{}", static_cast<bool>(held));
    held.unlock();
    println("{}", static_cast<bool>(held));
}
```

Output:

```text
true
false
```

## See also

- [unlock](unlock.md)
- [sgcl::io::file_lock](README.md)
