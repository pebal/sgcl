[sgcl](../../README.md) › [core](../README.md) › [unique_ptr](../unique_ptr.md)

# sgcl::operator==, operator\<=\> (sgcl::unique_ptr)

```cpp
/*(1)*/ template<class T1, class D1, class T2, class D2>
        bool operator==(const std::unique_ptr<T1, D1>& x, const std::unique_ptr<T2, D2>& y);
/*(2)*/ template<class T1, class D1, class T2, class D2>
        bool operator<(const std::unique_ptr<T1, D1>& x, const std::unique_ptr<T2, D2>& y);
/*(3)*/ template<class T1, class D1, class T2, class D2>
        bool operator<=(const std::unique_ptr<T1, D1>& x, const std::unique_ptr<T2, D2>& y);
/*(4)*/ template<class T1, class D1, class T2, class D2>
        bool operator>(const std::unique_ptr<T1, D1>& x, const std::unique_ptr<T2, D2>& y);
/*(5)*/ template<class T1, class D1, class T2, class D2>
        bool operator>=(const std::unique_ptr<T1, D1>& x, const std::unique_ptr<T2, D2>& y);
/*(6)*/ template<class T1, class D1, class T2, class D2>
            requires std::three_way_comparable_with<typename std::unique_ptr<T1, D1>::pointer,
                                                    typename std::unique_ptr<T2, D2>::pointer>
        std::compare_three_way_result_t<typename std::unique_ptr<T1, D1>::pointer,
                                        typename std::unique_ptr<T2, D2>::pointer>
            operator<=>(const std::unique_ptr<T1, D1>& x, const std::unique_ptr<T2, D2>& y);
/*(7)*/ template<class T, class D>
        bool operator==(const std::unique_ptr<T, D>& x, std::nullptr_t) noexcept;
/*(8)*/ template<class T, class D>
        bool operator<(const std::unique_ptr<T, D>& x, std::nullptr_t);
/*(9)*/ template<class T, class D>
        bool operator<(std::nullptr_t, const std::unique_ptr<T, D>& y);
/*(10)*/ template<class T, class D>
        bool operator<=(const std::unique_ptr<T, D>& x, std::nullptr_t);
/*(11)*/ template<class T, class D>
        bool operator<=(std::nullptr_t, const std::unique_ptr<T, D>& y);
/*(12)*/ template<class T, class D>
        bool operator>(const std::unique_ptr<T, D>& x, std::nullptr_t);
/*(13)*/ template<class T, class D>
        bool operator>(std::nullptr_t, const std::unique_ptr<T, D>& y);
/*(14)*/ template<class T, class D>
        bool operator>=(const std::unique_ptr<T, D>& x, std::nullptr_t);
/*(15)*/ template<class T, class D>
        bool operator>=(std::nullptr_t, const std::unique_ptr<T, D>& y);
/*(16)*/ template<class T, class D>
            requires std::three_way_comparable<typename std::unique_ptr<T, D>::pointer>
        std::compare_three_way_result_t<typename std::unique_ptr<T, D>::pointer>
            operator<=>(const std::unique_ptr<T, D>& x, std::nullptr_t);
```

The comparisons of `std::unique_ptr`, which take a `unique_ptr<T>` as its base: they compare the addresses, as with
raw pointers, and `!=` is rewritten from `==`.

- (1–6) Two owners.
- (7–16) An owner and `nullptr`.

## Parameters

| Parameter | Description |
|---|---|
| `x`, `y` | the owners to compare |

## Return value

The comparison of `x.get()` and `y.get()`, or of the one owner's `get()` and `nullptr`.

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
    unique_ptr a = make_tracked<int>(1);
    unique_ptr b = make_tracked<int>(1);
    unique_ptr<int> none;
    println("{} {} {} {}", a != b, a != nullptr, nullptr < a, none == nullptr);
}
```

Output:

```text
true true true true
```

## See also

- [get](get.md): the raw pointer
- [sgcl::unique_ptr\<T\>](../unique_ptr.md)
