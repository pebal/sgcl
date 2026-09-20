# sgcl/core/mixin: the mixins and the concepts

```cpp
#include "sgcl/core/mixin/mixin.h"   // or "sgcl/core/core.h", "sgcl/sgcl.h"
```

Two families of names with one purpose: to say what a type of the library is, in a way a class head declares and a function parameter can ask for.

A **mixin** (`m_`) is a base class a container names itself as the argument of (`class vector : public m_enumerable<vector<T>>, ...`): a static interface, no virtual method, no state, its constructor and destructor protected so that it exists only as such a base. It gives the class its methods — `v.contains(x)`, `v.sort()`, `m.get(key)` — and, by being there, declares a trait of the class: that it iterates, that it compares, that it is ordered, that its elements are written, that it is read by a key.

A **concept** (`c_`) asks a type for the traits it declared: `c_enumerable<R>` is "R carries `m_enumerable`", `c_ordered<R>` "R carries `m_ordered` and its elements are comparable". A concept of a container is nominal — it does not ask whether the type happens to have `begin()` and `end()`, it asks whether the type said so — so that only what declared itself a range of the library passes a parameter that asks for one, and what did not is refused in one line at the call: `std::vector` is not `c_enumerable`, and [`range(v.begin(), v.end())`](../range.md) is how it enters. A concept of a value (`c_equatable`, `c_comparable`) is structural too, because a value need not be the library's: an `int`, a `std::pair`, a class with a `<=>` of its own pass on what they can do.

```cpp
size_t count_odd(const c_enumerable auto& r) { return r.count_of([](int x) { return x % 2 != 0; }); }
const auto& largest(const c_ordered auto& r) { return r.max(); }
void sort_in_place(c_sequence auto& r) requires c_ordered<decltype(r)> { r.sort(); }
```

## The mixins

| mixin | what it gives | what it declares |
|---|---|---|
| [m_enumerable](m_enumerable.md) | the questions asked of the elements: `find_if`, `find_index`, `exists`, `all`, `count_of`, `for_each`; `contains`, `index_of`, `last_index_of` for elements with `==`; `min`, `max` for elements with `<` | a range of the library, `begin()` and `end()` |
| [m_equatable](m_equatable.md) | `==` between two containers, element by element | its values compare equal |
| [m_comparable](m_comparable.md) | `<=>` between two containers, lexicographic, by the elements' `<=>` or `<` | its values are ordered |
| [m_ordered](m_ordered.md) | the order of the whole range: `is_sorted`, `binary_search`, `sorted_index_of`, `lower_bound`, `upper_bound`; `sort`, `sort_by`, `stable_sort` where the elements are written | the range has an order |
| [m_sequence](m_sequence.md) | `fill`, `reverse` | the elements are written through the iterator |
| [m_lookup](m_lookup.md) | a map by its key: `get`, `try_get`, `value_or`, `contains_key`, `keys`, `values`, `values_of`; over `find` as an iterator or as a pointer | a map |
| `m_bidirectional`, `m_random_access`, `m_contiguous` ([concepts](concepts.md#the-categories)) | nothing: a declaration of the iterator's category, where a concept can ask for it | walked backwards; reached by position; one block, `data()` |
| [m_text](m_text.md) | the read side of `std::string_view` over `data()` and `size()`: `find`, `starts_with`, `contains`, `compare`, `substr`… | text: `string`, `slice<const CharT>` |

Every requirement is on a method, never on the class: `Derived` is not yet complete when the base is instantiated, so a method exists (`requires`) only for the elements and the categories that allow it, and a class that carries `m_ordered` over elements without `<` simply has no `sort()`, with one line of diagnostic when it is called. The mixins are independent — none inherits another; a container lists in its class head every one it carries — and a name lives in one mixin only, because a name found in two bases is ambiguous: every `sort` is `m_ordered`'s, `contains` of a value is `m_enumerable`'s.

A container whose own answer is better hides the mixin's with a method of the same name: a `sorted_set`'s `contains` by the key, its `min()` as `*begin()`. Hiding hides every overload of the name, so a container that keeps the mixin's other overloads re-exposes them with a using-declaration, or, as the sets do, defines its own.

## The concepts

| concept | is | asks for |
|---|---|---|
| `c_equatable<T>` | a value with `==` | `m_equatable`, or `==` (as the standard containers ask of an element) |
| `c_comparable<T>` | a value with an order | `m_comparable`, or `<=>`, or `<` alone (synth-three-way, as the standard containers) |
| `c_enumerable<R>` | a range of the library | `m_enumerable` |
| `c_bidirectional<R>` | walked backwards | `c_enumerable` and `m_bidirectional` |
| `c_random_access<R>` | reached by position | `c_bidirectional` and `m_random_access` |
| `c_contiguous<R>` | one block | `c_random_access` and `m_contiguous` |
| `c_sequence<R>` | elements written | `c_enumerable` and `m_sequence` |
| `c_ordered<R>` | a range with an order | `c_enumerable`, `m_ordered` and `c_comparable` of the element |
| `c_lookup<R>` | a map | `c_enumerable` and `m_lookup` |

The page: [concepts](concepts.md).

## Who carries what

| | enumerable | category | equatable | comparable | ordered | sequence | lookup |
|---|---|---|---|---|---|---|---|
| [vector](../../containers/vector.md), [array](../../containers/array.md), [dynamic_array](../../containers/dynamic_array.md), [slice\<T\>](../slice.md) | ✓ | contiguous | ✓ | ✓ | ✓ | ✓ | |
| [slice\<const T\>](../slice.md) | ✓ | contiguous | ✓ | ✓ | ✓ | | |
| [deque](../../containers/deque.md) | ✓ | random access | ✓ | ✓ | ✓ | ✓ | |
| [list](../../containers/list.md) | ✓ | bidirectional | ✓ | ✓ | ✓ (`sort` its own) | ✓ (`reverse` its own) | |
| [forward_list](../../containers/forward_list.md) | ✓ | — | ✓ | ✓ | ✓ (`sort` its own) | ✓ (`reverse` its own) | |
| [range](../range.md) | ✓ | the iterator's | ✓ | ✓ | ✓ | when the iterator writes | |
| [im::vector](../../containers/im/vector.md) | ✓ | random access | (its own `==`) | ✓ | ✓ (no `sort`) | | |
| [im::list](../../containers/im/list.md) | ✓ | — | (its own `==`) | ✓ | ✓ (no `sort`) | | |
| [sorted_set](../../containers/sorted_set.md), [sorted_multiset](../../containers/sorted_multiset.md) | ✓ (`contains`, `min`, `max` their own) | bidirectional | ✓ | ✓ | | | |
| [sorted_map](../../containers/sorted_map.md), [sorted_multimap](../../containers/sorted_multimap.md) | ✓ (the same) | bidirectional | ✓ | ✓ | | | ✓ |
| [set](../../containers/set.md), [multiset](../../containers/multiset.md), [ordered_set](../../containers/ordered_set.md), [im::set](../../containers/im/set.md) | ✓ (`contains` its own) | — | (its own `==`) | | | | |
| [map](../../containers/map.md), [multimap](../../containers/multimap.md), [ordered_map](../../containers/ordered_map.md) | ✓ (the same) | — | (its own `==`) | | | | ✓ |
| [im::map](../../containers/im/map.md) | ✓ (the same) | — | (its own `==`) | | | | ✓ |
| [string](../string.md) | m_text only: a string enters as `as_slice()` | | | | | | |

Not carried: the adaptors (`stack`, `queue`, `priority_queue`), `expiry_queue`, the weak containers and the concurrent ones — none iterates as a range of values.

## A class of your own

Derive from the mixins it can honour and give `begin()` and `end()`; the concepts see it as they see `vector`:

```cpp
template<class T>
class ring : public sgcl::m_enumerable<ring<T>>, public sgcl::m_random_access<ring<T>>, public sgcl::m_bidirectional<ring<T>>,
             public sgcl::m_equatable<ring<T>>, public sgcl::m_comparable<ring<T>>, public sgcl::m_ordered<ring<T>>, public sgcl::m_sequence<ring<T>> {
    ...
};
static_assert(sgcl::c_ordered<ring<int>>);
```

The pages: [m_enumerable](m_enumerable.md), [m_equatable](m_equatable.md), [m_comparable](m_comparable.md), [m_ordered](m_ordered.md), [m_sequence](m_sequence.md), [m_lookup](m_lookup.md), [m_text](m_text.md), [concepts](concepts.md); the tests in `tests/core/mixin.cpp`.
