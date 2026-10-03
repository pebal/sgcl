[sgcl](../../README.md) › [core](../README.md) › [tracked_ptr](../tracked_ptr.md)

# sgcl::operator==, operator\<=\> (sgcl::tracked_ptr)

```cpp
/*(1)*/ template<class T, class U>
        bool operator==(const tracked_ptr<T>& l, const tracked_ptr<U>& r) noexcept;
/*(2)*/ template<class T, class U>
        std::strong_ordering operator<=>(const tracked_ptr<T>& l, const tracked_ptr<U>& r) noexcept;
/*(3)*/ template<class T>
        bool operator==(const tracked_ptr<T>& l, std::nullptr_t) noexcept;
/*(4)*/ template<class T>
        std::strong_ordering operator<=>(const tracked_ptr<T>& l, std::nullptr_t) noexcept;
/*(5)*/ template<class T>
        bool operator==(std::nullptr_t, const tracked_ptr<T>& r) noexcept;
/*(6)*/ template<class T>
        std::strong_ordering operator<=>(std::nullptr_t, const tracked_ptr<T>& r) noexcept;
```

Compare the addresses, as with raw pointers: all six relational operators, `!=`, `<`, `<=`, `>` and `>=` rewritten
from these.

- (1–2) Two pointers, compared in the common type of the two raw pointers, as raw pointers compare: a pointer to a
  base subobject at an offset of the object equals the pointer to the object. `==` compares two types without a
  common type (unrelated classes) as `const void*`; `<=>` needs one (a class and its base, a type and `void`).
- (3–6) A pointer and `nullptr`.

## Parameters

| Parameter | Description |
|---|---|
| `l`, `r` | the pointers to compare |

## Return value

- (1), (3), (5) `true` when the addresses are equal, `false` otherwise.
- (2), (4), (6) The order of the addresses.

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
    tracked_ptr a = make_tracked<int>(1);
    tracked_ptr b = a;
    tracked_ptr c = make_tracked<int>(1);
    println("{} {} {} {}", a == b, a != c, a != nullptr, nullptr < a);
    println("{}", a <= b);
}
```

Output:

```text
true true true true
true
```

## See also

- [get](get.md): the raw pointer
- [sgcl::tracked_ptr\<T\>](../tracked_ptr.md)
