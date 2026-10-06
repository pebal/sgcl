[sgcl](../../README.md) › [io](../README.md) › [file_lock](README.md)

# sgcl::io::file_lock::operator=

```cpp
file_lock& operator=(file_lock&& other) noexcept;    // (1)
file_lock& operator=(const file_lock&) = delete;     // (2)
```

1. Unlocks the lock this guard holds, if any, and takes over `other`'s, `other` left holding none. An assignment of
   a guard to itself does nothing.
2. Not copied: one guard unlocks.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the guard whose lock this one takes over |

## Return value

`*this`.

## Complexity

Constant: one system call when this guard held a lock.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::file_lock held = io::lock_file("assign.lock").value();
    held = io::file_lock();  // unlocks
    println("{}", io::lock_file("assign.lock", {.timeout = duration::zero()}).has_value());
}
```

Output:

```text
true
```

## See also

- [unlock](unlock.md): the same without a new guard
- [sgcl::io::file_lock](README.md)
