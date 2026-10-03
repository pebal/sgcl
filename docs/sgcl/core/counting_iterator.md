[sgcl](../README.md) › [core](README.md)

# sgcl::counting_iterator\<T\>

```cpp
#include "sgcl/core/range.h"   // or "sgcl/core.h"

namespace sgcl {
    template<std::integral T>
    class counting_iterator;
}
```

`sgcl::counting_iterator<T>` is the iterator of the counting forms of [range](range/README.md): an integer as an iterator,
whose `*` is the number and whose `++` is the next one. The deduction guides of `range(n)` and `range(first, last)`
give `range<counting_iterator<T>>`, so the type is named where a counting range is stored or passed by its type:
`range<counting_iterator<int>> r(2, 10)`. It is `std::ranges::iota_view<T>`'s iterator under a name of its own; Go
counts in the loop itself, `for i := range 10`, with no iterator to name.

## Rules

- An iterator is the number and nothing else: it lives anywhere, refers to nothing and is never invalidated.
- `*` returns the number by value: there is no element in memory to refer to, so a member that would point at an
  element gives the value instead ([find_if](mixin/enumerable/find_if.md) on a counting range returns an
  `optional`).
- To `std::ranges` it is a random-access iterator (`iterator_concept`), so the size of a counting range is a
  subtraction; to the pre-C++20 `iterator_category` it is an input iterator, as `iota_view`'s is, since that category
  asks a forward iterator's reference to be a true reference.
- The steps are the arithmetic of `T`; a range never steps past its `last`. The steps and the difference are taken
  modulo 2^64, without an overflow: the ends of a 64-bit `T` (`range(INT64_MIN, INT64_MAX)`) are 2^64 − 1 apart,
  past a `std::ptrdiff_t`, so their difference wraps, [size](range/size.md) takes it back as a `size_t`, and an
  iterator moved by it lands on the other end.

## Template parameters

| Parameter | Description |
|---|---|
| `T` | The type of the numbers: any integral type. |

## Member types

| Type | Definition |
|---|---|
| `iterator_category` | `std::input_iterator_tag` |
| `iterator_concept` | `std::random_access_iterator_tag` |
| `value_type` | `T` |
| `difference_type` | `std::ptrdiff_t` |
| `pointer` | `const T*` |
| `reference` | `T` |

## Member functions

| Function | Description |
|---|---|
| `(constructor)` | the iterator at zero (`T()`), or at a number: `explicit counting_iterator(T value)` |
| `operator*` | the number |
| `operator[]` | the number `n` steps further, `T(*it + n)` |
| `operator++`, `operator--` | the next, the previous number |
| `operator+=`, `operator-=` | `n` steps further, back |

## Non-member functions

| Function | Description |
|---|---|
| `operator+`, `operator-` | an iterator `n` steps further or back; the difference of two iterators, as a `std::ptrdiff_t` |
| `operator==`, `operator<=>` | compare the numbers |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <type_traits>

using namespace sgcl;

int main() {
    range<counting_iterator<int>> digits(0, 10);
    println("{}", std::is_same_v<decltype(range(10)), decltype(digits)>);

    counting_iterator<int> it = digits.begin();
    it += 3;
    println("{} {} {}", *it, it[2], digits.end() - it);
}
```

Output:

```text
true
3 5 7
```

## See also

- [range](range/README.md): the range of two counting iterators, `range(n)` and `range(first, last)`
