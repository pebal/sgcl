[sgcl](../../README.md) › [core](../README.md) › [atomic_ref](../atomic_ref.md) › [handle](README.md)

# sgcl::atomic_ref\<H\>::is_lock_free

```cpp
bool is_lock_free() const noexcept;
```

Checks whether the operations of this view are lock-free: they are on every platform the library supports, since
the word of a handle is a `std::atomic` of a pointer. `is_always_lock_free`, a `static constexpr bool`, says the same
at compile time.

## Parameters

None.

## Return value

`true` when the operations are lock-free.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string name;
    atomic_ref a(name);
    println("{} {}", a.is_lock_free(), atomic_ref<string>::is_always_lock_free);
}
```

Output:

```text
true true
```

## See also

- [sgcl::atomic_ref\<H\>](README.md)
