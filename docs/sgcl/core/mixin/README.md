# sgcl::mixin and sgcl::req: the mixins and the requirements

```cpp
#include "sgcl/core/mixin/mixin.h"   // the mixins, namespace mixin; or "sgcl/core/core.h", "sgcl/sgcl.h"
#include "sgcl/core/req.h"           // the requirements, namespace req

using namespace sgcl;
```

Two families of names with one purpose: to say what a type of the library is, in a way a class head declares and a function parameter can ask for. Each in a namespace of its own, so that the names are the plain words — `mixin::ordered` in a class head, `req::ordered` in a parameter — and never meet a type's.

A **mixin** (`namespace mixin`, this directory) is a base class a container names itself as the argument of (`class vector : public mixin::enumerable<vector<T>>, ...`): a static interface, no virtual method, no state, its constructor and destructor protected so that it exists only as such a base. It gives the class its methods — `v.contains(x)`, `v.sort()`, `m.get(key)` — and, by being there, declares a trait of the class: that it iterates, that it compares, that it is ordered, that its elements are written, that it is read by a key.

A **requirement** (`namespace req`, [req.h](../req.md)) is a requirement that asks a type for the traits it declared: `req::enumerable<R>` is "R carries `mixin::enumerable`", `req::ordered<R>` "R carries `mixin::ordered` and its elements are comparable". A requirement of a container is nominal — it does not ask whether the type happens to have `begin()` and `end()`, it asks whether the type said so — so that only what declared itself a range of the library passes a parameter that asks for one, and what did not is refused in one line at the call: `std::vector` is not `req::enumerable`, and [`range(v.begin(), v.end())`](../range.md) is how it enters. A requirement of a value (`req::equatable`, `req::comparable`) is structural too, because a value need not be the library's: an `int`, a `std::pair`, a class with a `<=>` of its own pass on what they can do.

```cpp
size_t count_odd(const req::enumerable auto& r) { return r.count_of([](int x) { return x % 2 != 0; }); }
const auto& largest(const req::ordered auto& r) { return r.max(); }
void sort_in_place(req::sequence auto& r) requires req::ordered<decltype(r)> { r.sort(); }
```

## The mixins

| mixin | what it gives | what it declares |
|---|---|---|
| [mixin::enumerable](enumerable.md) | the questions asked of the elements: `find_if`, `find_index`, `exists`, `all`, `count_of`, `for_each`; `contains`, `index_of`, `last_index_of` for elements with `==`; `min`, `max` for elements with `<` | a range of the library, `begin()` and `end()` |
| [mixin::equatable](equatable.md) | `==` between two containers, element by element | its values compare equal |
| [mixin::comparable](comparable.md) | `<=>` between two containers, lexicographic, by the elements' `<=>` or `<` | its values are ordered |
| [mixin::ordered](ordered.md) | the order of the whole range: `is_sorted`, `binary_search`, `sorted_index_of`, `lower_bound`, `upper_bound`; `sort`, `sort_by`, `stable_sort` where the elements are written | the range has an order |
| [mixin::sequence](sequence.md) | `fill`, `reverse` | the elements are written through the iterator |
| [mixin::lookup](lookup.md) | a map by its key: `get`, `try_get`, `value_or`, `contains_key`, `keys`, `values`, `values_of`; over `find` (an iterator), or the map's `_value_of` where it has one | a map |
| `mixin::bidirectional`, `mixin::random_access`, `mixin::contiguous` ([req](../req.md#the-requirements)) | nothing: a declaration of the iterator's category, where a concept can ask for it | walked backwards; reached by position; one block, `data()` |
| [mixin::immutable](immutable.md) | nothing: a declaration | a value that never changes: every change a new container, a copy one word |
| [mixin::text](text.md) | the read side of `std::string_view` over `data()` and `size()`: `find`, `starts_with`, `contains`, `compare`, `substr`… | text: `string`, `slice<const CharT>` |

Every condition is on a method, never on the class: `Derived` is not yet complete when the base is instantiated, so a method exists (`requires`) only for the elements and the categories that allow it, and a class that carries `mixin::ordered` over elements without `<` simply has no `sort()`, with one line of diagnostic when it is called. The mixins are independent — none inherits another; a container lists in its class head every one it carries — and a name lives in one mixin only, because a name found in two bases is ambiguous: every `sort` is `mixin::ordered`'s, `contains` of a value is `mixin::enumerable`'s.

A container whose own answer is better hides the mixin's with a method of the same name: a `sorted_set`'s `contains` by the key, its `min()` as `*begin()`. Hiding hides every overload of the name, so a container that keeps the mixin's other overloads re-exposes them with a using-declaration, or, as the sets do, defines its own.

## The requirements

| requirement | is | asks for |
|---|---|---|
| `req::equatable<T>` | a value with `==` | `mixin::equatable`, or `==` (as the standard containers ask of an element) |
| `req::comparable<T>` | a value with an order | `mixin::comparable`, or `<=>`, or `<` alone (synth-three-way, as the standard containers) |
| `req::enumerable<R>` | a range of the library | `mixin::enumerable` |
| `req::bidirectional<R>` | walked backwards | `req::enumerable` and `mixin::bidirectional` |
| `req::random_access<R>` | reached by position | `req::bidirectional` and `mixin::random_access` |
| `req::contiguous<R>` | one block | `req::random_access` and `mixin::contiguous` |
| `req::sequence<R>` | elements written | `req::enumerable` and `mixin::sequence` |
| `req::ordered<R>` | a range with an order | `req::enumerable`, `mixin::ordered` and `req::comparable` of the element |
| `req::lookup<R>` | a map | `req::enumerable` and `mixin::lookup` |
| `req::immutable<R>` | a value that never changes | `req::enumerable` and `mixin::immutable` |

The page: [req](../req.md).

## Who carries what

| | enumerable | category | equatable | comparable | ordered | sequence | lookup | immutable |
|---|---|---|---|---|---|---|---|---|
| [vector](../vector.md), [array](../array.md), [dynamic_array](../dynamic_array.md), [slice\<T\>](../slice.md) | ✓ | contiguous | ✓ | ✓ | ✓ | ✓ | | |
| [slice\<const T\>](../slice.md) | ✓ | contiguous | ✓ | ✓ | ✓ | | | |
| [deque](../deque.md) | ✓ | random access | ✓ | ✓ | ✓ | ✓ | | |
| [list](../list.md) | ✓ | bidirectional | ✓ | ✓ | ✓ (`sort` its own) | ✓ (`reverse` its own) | | |
| [forward_list](../forward_list.md) | ✓ | — | ✓ | ✓ | ✓ (`sort` its own) | ✓ (`reverse` its own) | | |
| [range](../range.md) | ✓ | the iterator's | ✓ | ✓ | ✓ | when the iterator writes | | |
| [immutable::vector](../../immutable/vector.md) | ✓ | random access | (its own `==`) | ✓ | ✓ (no `sort`) | | | ✓ |
| [immutable::list](../../immutable/list.md) | ✓ | — | (its own `==`) | ✓ | ✓ (no `sort`) | | | ✓ |
| [sorted_set](../sorted_set.md), [sorted_multiset](../sorted_multiset.md) | ✓ (`contains`, `min`, `max` their own) | bidirectional | ✓ | ✓ | | | | |
| [sorted_map](../sorted_map.md), [sorted_multimap](../sorted_multimap.md) | ✓ (the same) | bidirectional | ✓ | ✓ | | | ✓ | |
| [set](../set.md), [multiset](../multiset.md), [ordered_set](../ordered_set.md) | ✓ (`contains` its own) | — | (its own `==`) | | | | | |
| [immutable::set](../../immutable/set.md) | ✓ (the same) | — | (its own `==`) | | | | | ✓ |
| [map](../map.md), [multimap](../multimap.md), [ordered_map](../ordered_map.md) | ✓ (the same) | — | (its own `==`) | | | | ✓ | |
| [immutable::map](../../immutable/map.md) | ✓ (the same) | — | (its own `==`) | | | | ✓ | ✓ |
| [string](../string.md) | mixin::text only: a string enters as `as_slice()` | | | | | | | |

Not carried: the adaptors (`stack`, `queue`, `priority_queue`), `expiry_queue`, the weak containers and the concurrent ones — none iterates as a range of values.

## A class of your own

Derive from the mixins it can honour and give `begin()` and `end()`; the requirements see it as they see `vector`. The library writes the bases one per line, in alphabetical order of the mixin's name, so that a reader finds one in a fixed place and a list never has to say why it is ordered as it is:

```cpp
template<class T>
class ring
: public mixin::bidirectional<ring<T>>
, public mixin::comparable<ring<T>>
, public mixin::enumerable<ring<T>>
, public mixin::equatable<ring<T>>
, public mixin::ordered<ring<T>>
, public mixin::random_access<ring<T>>
, public mixin::sequence<ring<T>> {
    ...
};
static_assert(req::ordered<ring<int>>);
```

The pages: [mixin::enumerable](enumerable.md), [mixin::equatable](equatable.md), [mixin::comparable](comparable.md), [mixin::ordered](ordered.md), [mixin::sequence](sequence.md), [mixin::lookup](lookup.md), [mixin::immutable](immutable.md), [mixin::text](text.md), [req](../req.md); the tests in `tests/core/mixin.cpp`.
