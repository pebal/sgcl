[sgcl](../README.md) › [core](README.md)

# sgcl::req

```cpp
#include "sgcl/core/req.h"   // or "sgcl/core.h"

namespace sgcl::req {
    template<class T> concept equatable;
    template<class T> concept comparable;
    template<class R> concept enumerable;
    template<class R> concept bidirectional;
    template<class R> concept random_access;
    template<class R> concept contiguous;
    template<class R> concept sequence;
    template<class R> concept ordered;
    template<class R> concept lookup;
    template<class R> concept immutable;
    template<class H> concept handle;  // sgcl/core/atomic.h
}
```

The requirements (`namespace sgcl::req`) are the concepts of the library: what a parameter of a library function asks of its argument and what the methods of a mixin ask of the class that carries them — one vocabulary for both, so that a function constrained by `req::ordered` can call `r.max()` and `r.binary_search(x)` knowing they are there ([the mixins](mixin/README.md)). Written in the abbreviated form, a requirement reads in a parameter as the word says: `void shuffle(req::random_access auto& r)`, `size_t count_odd(const req::enumerable auto& r)`.

## Rules

- **A requirement of a container is nominal.** `req::enumerable<R>` is `std::derived_from<R, mixin::enumerable<R>>`: the type declared itself by carrying the mixin. It does not ask whether `R` has `begin()` and `end()`, so `std::vector<int>` is not `req::enumerable`, however much it iterates, and a function that takes `const req::enumerable auto&` refuses it in one line at the call. What lets a standard container in is the explicit adapter: [range(v.begin(), v.end())](range.md), which declares by its iterator's category, or [slice(v)](slice.md) for contiguous memory.
- **A requirement of a value is structural.** `req::equatable<T>` is "T has `==`"; `req::comparable<T>` "T has `<=>`, or `<` alone" — exactly what the standard containers ask of their elements (an `==` for `==`, a `<` for the synth-three-way `<=>`), no more: `std::equality_comparable` would also ask for a `!=` that a type with a converting `==` cannot always form. An `int`, a `std::pair`, a `std::string`, a class with `<=>` of its own all pass, and so does a container of the library whose elements do: carrying `mixin::equatable` does not make `vector<T>` equatable when `T` has no `==`, since the mixin gives the operator only for elements that compare.
- **The categories are declared.** `mixin::bidirectional`, `mixin::random_access` and `mixin::contiguous` are mixins with no methods: what `iterator_category` says of the iterator, said of the container where a concept can ask for it; each implies the ones below it (`req::contiguous` is `req::random_access` is `req::bidirectional` is `req::enumerable`).
- **A function's constraint names everything its body uses.** C++ does not check that a constrained template's body uses only what its constraints promise (Rust does). A body that calls `r.max()` is constrained by `req::ordered`, not by `req::enumerable`, so that the error, if any, is at the call and not inside.

## Requirements

#### Values

| Requirement | Description |
|---|---|
| [equatable](req/equatable.md) | a value with `==` |
| [comparable](req/comparable.md) | a value with an order: `<=>`, or `<` alone |
| [handle](req/handle.md) | a public type of one tracked word to the object inside it |

#### Ranges

| Requirement | Description |
|---|---|
| [enumerable](req/enumerable.md) | a range of the library: carries `mixin::enumerable` |
| [bidirectional](req/bidirectional.md) | a range walked backwards as well |
| [random_access](req/random_access.md) | a range whose elements are reached by position |
| [contiguous](req/contiguous.md) | a range whose elements lie next to each other in memory |
| [sequence](req/sequence.md) | a range whose elements may be written in place |
| [ordered](req/ordered.md) | a range of comparable elements, with the questions of their order |
| [lookup](req/lookup.md) | a map read by its key |
| [immutable](req/immutable.md) | a range that never changes: every change a new one |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <vector>

using namespace sgcl;

// Two functions, each asking for exactly what its body uses
size_t count_odd(const req::enumerable auto& r) {
    return r.count_of([](int x) { return x % 2 != 0; });
}

void sort_and_print(req::ordered auto& r) requires req::sequence<decltype(r)> {
    r.sort();
    r.for_each([](int x) { print("{} ", x); });
    println();
}

int main() {
    vector v = {3, 1, 2};
    std::vector<int> sv = {9, 7, 8};
    sort_and_print(v);
    // sort_and_print(sv);                       // error: std::vector<int> does not satisfy req::ordered
    range r(sv.begin(), sv.end());         // the adapter: a range of the library over std's iterators
    sort_and_print(r);                           // sorts sv
    println("{} {} {}", count_odd(v), count_odd(r), count_odd(range(4)));
    return 0;
}
```

Output:

```text
1 2 3 
7 8 9 
2 2 2
```

## See also

- [the mixins and the requirements](mixin/README.md), [range](range.md), [slice](slice.md)
- `tests/core/mixin.cpp`
