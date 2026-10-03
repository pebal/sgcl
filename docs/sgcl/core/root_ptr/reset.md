[sgcl](../../README.md) › [core](../README.md) › [root_ptr](../root_ptr.md)

# sgcl::root_ptr\<T\>::reset

```cpp
/*(1)*/ void reset() noexcept;
/*(2)*/ template<class U, std::enable_if_t<std::is_convertible_v<typename tracked_ptr<U>::element_type*, element_type*>, int> = 0>
        void reset(const tracked_ptr<U>& p) noexcept;
```

Stores another pointer in the cell; the cell stays.

1. Null: `*this = nullptr`.
2. The pointer of `p`, `U*` convertible to `T*`: `*this = p`.

## Parameters

| Parameter | Description |
|---|---|
| `p` | the pointer to store |

## Return value

None.

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
    root_ptr<int> slot = make_tracked<int>(1);
    tracked_ptr next = make_tracked<int>(2);
    slot.reset(next);
    println("{}", *slot);
    slot.reset();
    println("{}", slot == nullptr);
}
```

Output:

```text
2
true
```

## See also

- [operator=](operator_assign.md): stores another pointer in the cell
- [sgcl::root_ptr\<T\>](../root_ptr.md)
