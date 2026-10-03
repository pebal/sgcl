[sgcl](../../README.md) › [core](../README.md) › [variant](../variant.md)

# sgcl::operator==, operator!=, operator\<, operator\<=, operator\>, operator\>=, operator\<=\> (sgcl::variant)

```cpp
#include "sgcl/core/variant.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class... Ts>
    bool operator==(const variant<Ts...>& l, const variant<Ts...>& r) noexcept(              // (1)
        (noexcept(bool(std::declval<const Ts&>() == std::declval<const Ts&>())) && ...));
    template<class... Ts>
    bool operator!=(const variant<Ts...>& l, const variant<Ts...>& r) noexcept(              // (2)
        (noexcept(bool(std::declval<const Ts&>() == std::declval<const Ts&>())) && ...));
    template<class... Ts>
    bool operator<(const variant<Ts...>& l, const variant<Ts...>& r) noexcept(               // (3)
        (noexcept(bool(std::declval<const Ts&>() < std::declval<const Ts&>())) && ...));
    template<class... Ts>
    bool operator>(const variant<Ts...>& l, const variant<Ts...>& r) noexcept(               // (4)
        (noexcept(bool(std::declval<const Ts&>() < std::declval<const Ts&>())) && ...));
    template<class... Ts>
    bool operator<=(const variant<Ts...>& l, const variant<Ts...>& r) noexcept(              // (5)
        (noexcept(bool(std::declval<const Ts&>() < std::declval<const Ts&>())) && ...));
    template<class... Ts>
    bool operator>=(const variant<Ts...>& l, const variant<Ts...>& r) noexcept(              // (6)
        (noexcept(bool(std::declval<const Ts&>() < std::declval<const Ts&>())) && ...));
    template<class... Ts>
    requires (std::three_way_comparable<Ts> && ...)
    std::common_comparison_category_t<std::compare_three_way_result_t<Ts>...>
    operator<=>(const variant<Ts...>& l, const variant<Ts...>& r) noexcept(                  // (7)
        (noexcept(std::declval<const Ts&>() <=> std::declval<const Ts&>()) && ...));
}
```

Compares two variants of the same type: first by the index of the alternative held, then, for the same index, the
alternatives themselves.

1. `true` when both hold the same alternative and the two are equal, or both are valueless.
2. `!(l == r)`.
3. A valueless variant is less than any other; otherwise the lower index is less, and for the same index the
   alternatives compare with `<`.
4. `r < l`.
5. `!(r < l)`.
6. `!(l < r)`.
7. The three-way comparison in the same order: valueless first, then the index, then the alternatives with `<=>`.

- (1–2) Take part only when every alternative has an `==` whose result converts to `bool`.
- (3–6) Take part only when every alternative has a `<` whose result converts to `bool`; the standard asks each
  operator of the alternatives itself, the library builds the four from `<`.

## Parameters

| Parameter | Description |
|---|---|
| `l`, `r` | the variants to compare |

## Return value

- (1–6) The result of the comparison.
- (7) The order, of the common comparison category of the alternatives' `<=>`.

## Complexity

Constant, plus one comparison of the alternatives when the indices agree.

## Exceptions

What the comparison of the alternatives throws; none when every alternative's is noexcept.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    variant<int, string> a = 1, b = 2, c = "text";
    println("{} {} {}", a == b, a < b, a < c);  // an int before any string: the index decides
    println("{} {}", c == variant<int, string>("text"), (b <=> a) > 0);
}
```

Output:

```text
false true true
true true
```

## See also

- [index](index.md): the index the comparisons start with
- [sgcl::variant\<Ts...\>](../variant.md)
