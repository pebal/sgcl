[sgcl](../../README.md) › [io](../README.md) › [file_lock](README.md)

# sgcl::io::file_lock::file_lock

```cpp
file_lock() noexcept = default;           // (1)
file_lock(file_lock&& other) noexcept;    // (2)
file_lock(const file_lock&) = delete;     // (3)
```

1. A guard that holds no lock: `!held`, its unlock does nothing.
2. The guard of `other`'s lock, `other` left holding none.
3. Not copied: one guard unlocks.

A guard of a lock is made by [lock_file](../lock_file.md).

## Parameters

| Parameter | Description |
|---|---|
| `other` | the guard whose lock this one takes over |

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::file_lock none;
    io::file_lock held = io::lock_file("ctor.lock").value();
    io::file_lock taken = std::move(held);
    println("{} {} {}", static_cast<bool>(none), static_cast<bool>(held), static_cast<bool>(taken));
}
```

Output:

```text
false false true
```

## See also

- [lock_file](../lock_file.md): what makes one
- [sgcl::io::file_lock](README.md)
