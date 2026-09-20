# sgcl::m_enumerable

```cpp
#include "sgcl/core/mixin/m_enumerable.h"   // or "sgcl/core/mixin/mixin.h", "sgcl/sgcl.h"

namespace sgcl {
    inline constexpr size_t npos;             // the position that is no position
    template<class Derived>
    class m_enumerable;
}
```

`m_enumerable<Derived>` gives a class the questions asked of the elements of a range — is there one like this, where, how many, the smallest — as members, over the `begin()` and `end()` of `Derived`, and declares the class a range of the library: `c_enumerable<R>` is "R carries `m_enumerable`" ([the mixins](README.md)). Every container that iterates carries it, from `vector` to `sorted_map`, `im::list` and `slice`; a class of your own does by deriving from it and giving `begin()` and `end()`.

## Rules

- A question that compares elements exists only for elements that compare: `contains`, `index_of`, `last_index_of` for `c_equatable` elements (`==`), `min()` and `max()` for `c_comparable` ones (`<`); the forms with a predicate or a comparator ask nothing of the element. On a `vector<T>` whose `T` has neither, `v.exists(pred)` is there and `v.contains(x)` is not.
- `min` and `max` on an empty range are undefined, as `front()` is; nothing is checked. They return a reference into the range, or a value where the iterator gives values (`range(n)`).
- A container with a better answer hides the mixin's: `set::contains` by the key, `set::min()` as `*begin()`.
- Thread safety is the container's: the members read the elements as the algorithms do.

## Members

```cpp
template<class Pred> size_t find_index(Pred pred) const;   // the position of the first element the predicate accepts, npos when none
template<class Pred> auto find_if(Pred pred) noexcept;     // a pointer to it, null when none; and const
template<class Pred> bool exists(Pred pred) const;         // some element satisfies pred
template<class Pred> bool all(Pred pred) const;            // every element does
template<class Pred> size_t count_of(Pred pred) const;     // how many do
template<class F> void for_each(F f);                      // and const

bool contains(const auto& value) const;                    // c_equatable elements: anything an element compares with
size_t index_of(const auto& value) const;                  // the first equal element's position, npos when none
size_t last_index_of(const auto& value) const;             // the last one's (a walk of the whole range)

decltype(auto) min() const;  template<class Compare> decltype(auto) min(Compare cmp) const;   // c_comparable elements, or by the comparator
decltype(auto) max() const;  template<class Compare> decltype(auto) max(Compare cmp) const;
```

```cpp
sgcl::vector v = {5, 3, 9, 3};
assert(v.contains(9) && v.index_of(3) == 1 && v.last_index_of(3) == 3 && v.index_of(7) == sgcl::npos);
assert(v.find_index([](int x) { return x > 4; }) == 0 && v.exists([](int x) { return x == 9; }) && !v.all([](int x) { return x > 3; }));
if (int* big = v.find_if([](int x) { return x > 8; })) {
    *big = 8;
}
assert(v.count_of([](int x) { return x == 3; }) == 2 && v.min() == 3 && v.max() == 8);
struct point { int x, y; };                      // no ==, no <
sgcl::vector<point> pts = {{1, 2}, {3, 0}};
assert(pts.min([](point a, point b) { return a.y < b.y; }).x == 3);   // a comparator asks nothing of point
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

// A function over any range of the library: a set, a slice, a list, a
// vector; what it asks for is what the parameter says
size_t count_odd(const sgcl::c_enumerable auto& r) {
    return r.count_of([](int x) { return x % 2 != 0; });
}

int main() {
    sgcl::vector v = {1, 2, 3, 4, 5};
    sgcl::sorted_set<int> s = {7, 8, 9};
    std::cout << count_odd(v) << " " << count_odd(s) << " " << count_odd(v.as_slice(1, 3)) << " " << count_odd(sgcl::range(10)) << "\n";
    return 0;
}
```

The output:

```
3 2 1 5
```

## See also

- [the mixins and the concepts](README.md); [m_ordered](m_ordered.md), the questions about the order of the whole range
- `tests/core/mixin.cpp`
