[sgcl](../../README.md) › [core](../README.md) › [root_ptr](../root_ptr.md)

# sgcl::operator==, operator\<=\> (sgcl::root_ptr)

```cpp
/*(1)*/ template<class T, class U>
        bool operator==(const root_ptr<T>& l, const root_ptr<U>& r) noexcept;
/*(2)*/ template<class T, class U>
        bool operator==(const root_ptr<T>& l, const tracked_ptr<U>& r) noexcept;
/*(3)*/ template<class T, class U>
        bool operator==(const tracked_ptr<T>& l, const root_ptr<U>& r) noexcept;
/*(4)*/ template<class T>
        bool operator==(const root_ptr<T>& l, std::nullptr_t) noexcept;
/*(5)*/ template<class T, class U>
        std::strong_ordering operator<=>(const root_ptr<T>& l, const root_ptr<U>& r) noexcept;
```

Compare the objects pointed at, by their addresses; `!=` and the comparisons with the operands swapped are rewritten
from these.

- (1–3) `l.get() == r.get()`: with another root, or with a `tracked_ptr` on either side.
- (4) Whether the root is null.
- (5) The order of the addresses between two roots, in the common type of the two pointers, as (1) compares them;
  as `const void*` for two types without one.

## Parameters

| Parameter | Description |
|---|---|
| `l`, `r` | the pointers to compare |

## Return value

- (1–4) `true` when the addresses are equal, `false` otherwise.
- (5) The order of the addresses.

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
    tracked_ptr object = make_tracked<int>(1);
    root_ptr a = object;
    root_ptr b = object;  // another cell, the same object
    root_ptr<int> none;
    println("{} {} {} {}", a == b, object == a, none == nullptr, nullptr != a);
    println("{}", (a <=> b) == 0);
}
```

Output:

```text
true true true true
true
```

## See also

- [get](get.md): the raw pointer
- [sgcl::root_ptr\<T\>](../root_ptr.md)
