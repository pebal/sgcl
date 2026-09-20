# sgcl::req: enumerable, ordered, comparable, ...

```cpp
#include "sgcl/core/req.h"   // or "sgcl/core/core.h", "sgcl/sgcl.h"

namespace sgcl::req {
    template<class T> concept equatable;        // a value with ==
    template<class T> concept comparable;       // a value with an order: <=>, or < alone
    template<class R> concept enumerable;       // a range of the library: carries mixin::enumerable
    template<class R> concept bidirectional;    // and mixin::bidirectional
    template<class R> concept random_access;    // and mixin::random_access
    template<class R> concept contiguous;       // and mixin::contiguous
    template<class R> concept sequence;         // and mixin::sequence: the elements are written
    template<class R> concept ordered;          // and mixin::ordered, with req::comparable elements
    template<class R> concept lookup;           // and mixin::lookup: a map
}

namespace sgcl::mixin {
    template<class Derived> class bidirectional;    // the categories: declarations without methods
    template<class Derived> class random_access;
    template<class Derived> class contiguous;
}
```

The requirements (`namespace sgcl::req`) are the concepts of the library: what a parameter of a library function asks of its argument and what the methods of a mixin ask of the class that carries them — one vocabulary for both, so that a function constrained by `req::ordered` can call `r.max()` and `r.binary_search(x)` knowing they are there ([the mixins](mixin/README.md)). Written in the abbreviated form, a requirement reads in a parameter as the word says: `void shuffle(req::random_access auto& r)`, `size_t count_odd(const req::enumerable auto& r)`.

## Rules

- **A requirement of a container is nominal.** `req::enumerable<R>` is `std::derived_from<R, mixin::enumerable<R>>`: the type declared itself by carrying the mixin. It does not ask whether `R` has `begin()` and `end()`, so `std::vector<int>` is not `req::enumerable`, however much it iterates, and a function that takes `const req::enumerable auto&` refuses it in one line at the call. What lets a standard container in is the explicit adapter: [`range(v.begin(), v.end())`](range.md), which declares by its iterator's category, or [`slice(v)`](slice.md) for contiguous memory.
- **A requirement of a value is structural too.** `req::equatable<T>` is "T carries `mixin::equatable`, or T has `==`"; `req::comparable<T>` "T carries `mixin::comparable`, or T has `<=>`, or `<` alone" — exactly what the standard containers ask of their elements (an `==` for `==`, a `<` for the synth-three-way `<=>`), no more: `std::equality_comparable` would also ask for a `!=` that a type with a converting `==` cannot always form. An `int`, a `std::pair`, a `std::string`, a class with `<=>` of its own all pass.
- **The categories are declared.** `mixin::bidirectional`, `mixin::random_access` and `mixin::contiguous` are mixins with no methods: what `iterator_category` says of the iterator, said of the container where a concept can ask for it; each implies the ones below it (`req::contiguous` is `req::random_access` is `req::bidirectional` is `req::enumerable`).
- **A function's constraint names everything its body uses.** C++ does not check that a constrained template's body uses only what its constraints promise (Rust does). A body that calls `r.max()` is constrained by `req::ordered`, not by `req::enumerable`, so that the error, if any, is at the call and not inside.

## The requirements

| requirement | true for | example |
|---|---|---|
| `req::equatable<T>` | `int`, `std::string`, `sgcl::string`, `vector<int>`, a struct with `==`; not a struct without | `contains` needs it of the element |
| `req::comparable<T>` | `int`, `std::pair<int, int>`, a struct with `<=>` or with `<` alone; not a struct without | `min`, `sort`, `<=>` of a container need it |
| `req::enumerable<R>` | every container of the library that iterates, `slice`, `range`; not `std::vector`, not `string` | `count_of`, `for_each` |
| `req::bidirectional<R>` | `vector`, `deque`, `list`, `sorted_set`, `sorted_map`, `slice`; not `forward_list`, `set` | `last_index_of`, `reverse` |
| `req::random_access<R>` | `vector`, `array`, `deque`, `im::vector`, `slice`; not `list` | `binary_search`, `sort` |
| `req::contiguous<R>` | `vector`, `array`, `dynamic_array`, `slice` | `data()` |
| `req::sequence<R>` | the mutable sequences, `slice<T>`; not `im::vector`, not `slice<const T>`, not `sorted_set` | `fill`, `sort` |
| `req::ordered<R>` | a sequence, `slice`, `im::vector`, `im::list` of comparable elements; not `sorted_set`, not `vector<point>` | `max`, `is_sorted`, `lower_bound` |
| `req::lookup<R>` | `sorted_map`, `sorted_multimap`, `map`, `multimap`, `ordered_map`, `im::map` | `get`, `contains_key` |

```cpp
static_assert(sgcl::req::ordered<sgcl::vector<int>> && !sgcl::req::ordered<sgcl::sorted_set<int>> && !sgcl::req::enumerable<std::vector<int>>);
struct point { int x, y; };
static_assert(sgcl::req::enumerable<sgcl::vector<point>> && !sgcl::req::ordered<sgcl::vector<point>>);   // iterates; has no order
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>
#include <vector>

// Two functions, each asking for exactly what its body uses
size_t count_odd(const sgcl::req::enumerable auto& r) {
    return r.count_of([](int x) { return x % 2 != 0; });
}

void sort_and_print(sgcl::req::ordered auto& r) requires sgcl::req::sequence<decltype(r)> {
    r.sort();
    r.for_each([](int x) { std::cout << x << " "; });
    std::cout << "\n";
}

int main() {
    sgcl::vector v = {3, 1, 2};
    std::vector<int> sv = {9, 7, 8};
    sort_and_print(v);
    // sort_and_print(sv);                       // error: std::vector<int> does not satisfy req::ordered
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

- [the mixins and the requirements](mixin/README.md), [range](range.md), [slice](slice.md)
- `tests/core/mixin.cpp`
