# sgcl concepts: c_enumerable, c_ordered, c_comparable, ...

```cpp
#include "sgcl/core/mixin/concepts.h"   // or "sgcl/core/mixin/mixin.h", "sgcl/sgcl.h"

namespace sgcl {
    template<class T> concept c_equatable;        // a value with ==
    template<class T> concept c_comparable;       // a value with an order: <=>, or < alone
    template<class R> concept c_enumerable;       // a range of the library: carries m_enumerable
    template<class R> concept c_bidirectional;    // and m_bidirectional
    template<class R> concept c_random_access;    // and m_random_access
    template<class R> concept c_contiguous;       // and m_contiguous
    template<class R> concept c_sequence;         // and m_sequence: the elements are written
    template<class R> concept c_ordered;          // and m_ordered, with c_comparable elements
    template<class R> concept c_lookup;           // and m_lookup: a map

    template<class Derived> class m_bidirectional;    // the categories: declarations without methods
    template<class Derived> class m_random_access;
    template<class Derived> class m_contiguous;
}
```

The concepts (`c_`) are what a parameter of a library function asks of its argument and what the methods of a mixin ask of the class that carries them: one vocabulary for both, so that a function constrained by `c_ordered` can call `r.max()` and `r.binary_search(x)` knowing they are there ([the mixins](README.md)). Written in the abbreviated form, a concept reads in a parameter as an interface would in C#: `void shuffle(c_random_access auto& r)`.

## Rules

- **A concept of a container is nominal.** `c_enumerable<R>` is `std::derived_from<R, m_enumerable<R>>`: the type declared itself by carrying the mixin. It does not ask whether `R` has `begin()` and `end()`, so `std::vector<int>` is not `c_enumerable`, however much it iterates, and a function that takes `const c_enumerable auto&` refuses it in one line at the call. What lets a standard container in is the explicit adapter: [`range(v.begin(), v.end())`](../range.md), which declares by its iterator's category, or [`slice(v)`](../slice.md) for contiguous memory.
- **A concept of a value is structural too.** `c_equatable<T>` is "T carries `m_equatable`, or T has `==`"; `c_comparable<T>` "T carries `m_comparable`, or T has `<=>`, or `<` alone" — exactly what the standard containers ask of their elements (an `==` for `==`, a `<` for the synth-three-way `<=>`), no more: `std::equality_comparable` would also ask for a `!=` that a type with a converting `==` cannot always form. An `int`, a `std::pair`, a `std::string`, a class with `<=>` of its own all pass.
- **The categories are declared.** `m_bidirectional`, `m_random_access` and `m_contiguous` are mixins with no methods: what `iterator_category` says of the iterator, said of the container where a concept can ask for it; each implies the ones below it (`c_contiguous` is `c_random_access` is `c_bidirectional` is `c_enumerable`).
- **A function's constraint names everything its body uses.** C++ does not check that a constrained template's body uses only what its constraints promise (Rust does). A body that calls `r.max()` is constrained by `c_ordered`, not by `c_enumerable`, so that the error, if any, is at the call and not inside.

## The concepts

| concept | true for | example |
|---|---|---|
| `c_equatable<T>` | `int`, `std::string`, `sgcl::string`, `vector<int>`, a struct with `==`; not a struct without | `contains` needs it of the element |
| `c_comparable<T>` | `int`, `std::pair<int, int>`, a struct with `<=>` or with `<` alone; not a struct without | `min`, `sort`, `<=>` of a container need it |
| `c_enumerable<R>` | every container of the library that iterates, `slice`, `range`; not `std::vector`, not `string` | `count_of`, `for_each` |
| `c_bidirectional<R>` | `vector`, `deque`, `list`, `sorted_set`, `sorted_map`, `slice`; not `forward_list`, `set` | `last_index_of`, `reverse` |
| `c_random_access<R>` | `vector`, `array`, `deque`, `im::vector`, `slice`; not `list` | `binary_search`, `sort` |
| `c_contiguous<R>` | `vector`, `array`, `dynamic_array`, `slice` | `data()` |
| `c_sequence<R>` | the mutable sequences, `slice<T>`; not `im::vector`, not `slice<const T>`, not `sorted_set` | `fill`, `sort` |
| `c_ordered<R>` | a sequence, `slice`, `im::vector`, `im::list` of comparable elements; not `sorted_set`, not `vector<point>` | `max`, `is_sorted`, `lower_bound` |
| `c_lookup<R>` | `sorted_map`, `sorted_multimap`, `map`, `multimap`, `ordered_map`, `im::map` | `get`, `contains_key` |

```cpp
static_assert(sgcl::c_ordered<sgcl::vector<int>> && !sgcl::c_ordered<sgcl::sorted_set<int>> && !sgcl::c_enumerable<std::vector<int>>);
struct point { int x, y; };
static_assert(sgcl::c_enumerable<sgcl::vector<point>> && !sgcl::c_ordered<sgcl::vector<point>>);   // iterates; has no order
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>
#include <vector>

// Two functions, each asking for exactly what its body uses
size_t count_odd(const sgcl::c_enumerable auto& r) {
    return r.count_of([](int x) { return x % 2 != 0; });
}

void sort_and_print(sgcl::c_ordered auto& r) requires sgcl::c_sequence<decltype(r)> {
    r.sort();
    r.for_each([](int x) { std::cout << x << " "; });
    std::cout << "\n";
}

int main() {
    sgcl::vector v = {3, 1, 2};
    std::vector<int> sv = {9, 7, 8};
    sort_and_print(v);
    // sort_and_print(sv);                       // error: std::vector<int> does not satisfy c_ordered
    sgcl::range r(sv.begin(), sv.end());         // the adapter: a range of the library over std's iterators
    sort_and_print(r);                           // sorts sv
    std::cout << count_odd(v) << " " << count_odd(r) << " " << count_odd(sgcl::range(4)) << "\n";
    return 0;
}
```

The output:

```
1 2 3 
7 8 9 
2 2 2
```

## See also

- [the mixins and the concepts](README.md), [range](../range.md), [slice](../slice.md)
- `tests/core/mixin.cpp`
