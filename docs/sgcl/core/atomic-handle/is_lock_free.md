[sgcl](../../README.md) › [core](../README.md) › [atomic](../atomic.md) › [handle](../atomic-handle.md)

# sgcl::atomic\<H\>::is_lock_free

```cpp
bool is_lock_free() const noexcept;
```

Checks whether the operations of this atomic are lock-free: they are on every platform the library supports, since
the word is a `std::atomic` of a pointer. `is_always_lock_free`, a `static constexpr bool`, says the same at compile
time.

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
    atomic<string> name;
    println("{} {}", name.is_lock_free(), atomic<string>::is_always_lock_free);
}
```

Output:

```text
true true
```

## See also

- [sgcl::atomic\<H\>](../atomic-handle.md)
