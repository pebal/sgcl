[sgcl](../../README.md) › [core](../README.md) › [atomic_ref](../atomic_ref.md) › [tracked_ptr](../atomic_ref-tracked_ptr.md)

# sgcl::atomic_ref\<tracked_ptr\<T\>\>::is_lock_free

```cpp
bool is_lock_free() const noexcept;
```

Checks whether the operations of this view are lock-free: they are on every platform the library supports, since the
word of a `tracked_ptr` is a `std::atomic` of a pointer. `is_always_lock_free`, a `static constexpr bool`, says the
same at compile time.

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
    tracked_ptr<int> word;
    atomic_ref a(word);
    println("{} {}", a.is_lock_free(), atomic_ref<tracked_ptr<int>>::is_always_lock_free);
}
```

Output:

```text
true true
```

## See also

- [sgcl::atomic_ref\<tracked_ptr\<T\>\>](../atomic_ref-tracked_ptr.md)
