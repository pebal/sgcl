[sgcl](../../README.md) › [core](../README.md) › mixin

# sgcl::mixin

```cpp
#include "sgcl/core/mixin/mixin.h"   // namespace sgcl::mixin; or "sgcl/core.h"
#include "sgcl/core/req.h"           // namespace sgcl::req; or "sgcl/core.h"
```

Two families of names with one purpose: to say what a type of the library is, in a way a class head declares and
a function parameter can ask for. Each lives in a namespace of its own, so that the names are the plain words —
`mixin::ordered` in a class head, `req::ordered` in a parameter — and never meet a type's.

A **mixin** (`namespace sgcl::mixin`, this directory) is a base class a container names itself as the argument of
(`class vector : public mixin::enumerable<vector<T>>, ...`): a static interface, no virtual method, no state, its
constructor and destructor protected so that it exists only as such a base. It gives the class its methods —
`v.contains(x)`, `v.sort()`, `m.get(key)` — and, by being there, declares a trait of the class: that it iterates,
that it compares, that it is ordered, that its elements are written, that it is read by a key.

A **requirement** (`namespace sgcl::req`, [req](../req/README.md)) asks a type for the traits it declared:
`req::enumerable<R>` is "R carries `mixin::enumerable`", `req::ordered<R>` "R carries `mixin::ordered` and its
elements are comparable". A requirement of a container is nominal — it does not ask whether the type happens to
have `begin()` and `end()`, it asks whether the type said so — so that only what declared itself a range of the
library passes a parameter that asks for one, and what did not is refused in one line at the call: `std::vector`
is not `req::enumerable`, and `range(v.begin(), v.end())` ([range](../range/README.md)) is how it enters. A requirement
of a value (`req::equatable`, `req::comparable`) is structural, because a value need not be the library's: an
`int`, a `std::pair`, a class with a `<=>` of its own pass on what they can do.

## The rules

- Every condition is on a method, never on the class: `Derived` is not yet complete when the base is
  instantiated, so a method exists (`requires`) only for the elements and the categories that allow it, and a
  class that carries `mixin::ordered` over elements without `<` simply has no `sort()`, with one line of
  diagnostic when it is called.
- The mixins are independent — none inherits another; a container lists in its class head every one it carries —
  and a name lives in one mixin only, because a name found in two bases is ambiguous: every `sort` is
  `mixin::ordered`'s, `contains` of a value is `mixin::enumerable`'s.
- A container whose own answer is better hides the mixin's with a method of the same name: a `sorted_set`'s
  `contains` by the key, its `min()` as `*begin()`. Hiding hides every overload of the name, so a container that
  keeps the mixin's other overloads re-exposes them with a using-declaration, or, as the sets do, defines its own.
- A class of your own derives from the mixins it can honour and gives `begin()` and `end()`; the requirements see
  it as they see `vector` ([mixin::comparable: Example](comparable/README.md#example),
  [mixin::sequence: Example](sequence/README.md#example)). The library writes the bases one per line, in alphabetical
  order of the mixin's name, so that a reader finds one in a fixed place and a list never has to say why it is
  ordered as it is.

## Mixins

| Mixin | Header | Description |
|---|---|---|
| bidirectional | `sgcl/core/mixin/bidirectional.h` | gives nothing: declares that the range is walked backwards, where a concept can ask for it ([req::bidirectional](../req/bidirectional.md)) |
| [comparable](comparable/README.md) | `sgcl/core/mixin/comparable.h` | `<=>` between two containers, lexicographic, by the elements' `<=>` or `<`; declares that its values are ordered |
| contiguous | `sgcl/core/mixin/contiguous.h` | gives nothing: declares that the elements are one block, `data()` ([req::contiguous](../req/contiguous.md)) |
| [enumerable](enumerable/README.md) | `sgcl/core/mixin/enumerable.h` | the questions asked of the elements: `find_if`, `find_index`, `exists`, `all`, `count_of`, `for_each`; `contains`, `index_of`, `last_index_of` for elements with `==`; `min`, `max` for elements with `<`; declares a range of the library, `begin()` and `end()` |
| [equatable](equatable/README.md) | `sgcl/core/mixin/equatable.h` | `==` between two containers, element by element; declares that its values compare equal |
| [immutable](immutable.md) | `sgcl/core/mixin/immutable.h` | gives nothing: declares a value that never changes, every change a new container, a copy one word |
| [lookup](lookup/README.md) | `sgcl/core/mixin/lookup.h` | a map read by its key: `get`, `try_get`, `value_or`, `contains_key`, `keys`, `values`, `values_of`; declares a map |
| [ordered](ordered/README.md) | `sgcl/core/mixin/ordered.h` | the order of the whole range: `is_sorted`, `binary_search`, `sorted_index_of`, `lower_bound`, `upper_bound`; `sort`, `sort_by`, `stable_sort` where the elements are written; declares that the range has an order |
| random_access | `sgcl/core/mixin/random_access.h` | gives nothing: declares that the elements are reached by position ([req::random_access](../req/random_access.md)) |
| [sequence](sequence/README.md) | `sgcl/core/mixin/sequence.h` | `fill`, `reverse`; declares that the elements are written through the iterator |
| [text](text/README.md) | `sgcl/core/mixin/text.h` | the read side of `std::string_view` over `data()` and `size()`: `find`, `starts_with`, `contains`, `compare`…; declares text: `string`, `slice<const CharT>` |

## Requirements

The page of the requirements: [req](../req/README.md).

| Requirement | Header | Description |
|---|---|---|
| [bidirectional](../req/bidirectional.md) | `sgcl/core/req.h` | walked backwards: `req::enumerable` and `mixin::bidirectional` |
| [comparable](../req/comparable.md) | `sgcl/core/req.h` | a value with an order: `mixin::comparable`, or `<=>`, or `<` alone (synth-three-way, as the standard containers) |
| [contiguous](../req/contiguous.md) | `sgcl/core/req.h` | one block: `req::random_access` and `mixin::contiguous` |
| [enumerable](../req/enumerable.md) | `sgcl/core/req.h` | a range of the library: `mixin::enumerable` |
| [equatable](../req/equatable.md) | `sgcl/core/req.h` | a value with `==`: `mixin::equatable`, or `==` (as the standard containers ask of an element) |
| [immutable](../req/immutable.md) | `sgcl/core/req.h` | a value that never changes: `req::enumerable` and `mixin::immutable` |
| [lookup](../req/lookup.md) | `sgcl/core/req.h` | a map: `req::enumerable` and `mixin::lookup` |
| [ordered](../req/ordered.md) | `sgcl/core/req.h` | a range with an order: `req::enumerable`, `mixin::ordered` and `req::comparable` of the element |
| [random_access](../req/random_access.md) | `sgcl/core/req.h` | reached by position: `req::bidirectional` and `mixin::random_access` |
| [sequence](../req/sequence.md) | `sgcl/core/req.h` | elements written: `req::enumerable` and `mixin::sequence` |

## Who carries what

| Container | Enumerable | Category | Equatable | Comparable | Ordered | Sequence | Lookup | Immutable |
|---|---|---|---|---|---|---|---|---|
| [array](../array/README.md) | ✓ | contiguous | ✓ | ✓ | ✓ | ✓ | | |
| [deque](../deque/README.md) | ✓ | random access | ✓ | ✓ | ✓ | ✓ | | |
| [dynamic_array](../dynamic_array/README.md) | ✓ | contiguous | ✓ | ✓ | ✓ | ✓ | | |
| [forward_list](../forward_list/README.md) | ✓ | — | ✓ | ✓ | ✓ (`sort` its own) | ✓ (`reverse` its own) | | |
| [immutable::list](../../immutable/list/README.md) | ✓ | — | (its own `==`) | ✓ | ✓ (no `sort`) | | | ✓ |
| [immutable::map](../../immutable/map/README.md) | ✓ (`contains` its own) | — | (its own `==`) | | | | ✓ | ✓ |
| [immutable::set](../../immutable/set/README.md) | ✓ (`contains` its own) | — | (its own `==`) | | | | | ✓ |
| [immutable::vector](../../immutable/vector/README.md) | ✓ | random access | (its own `==`) | ✓ | ✓ (no `sort`) | | | ✓ |
| [list](../list/README.md) | ✓ | bidirectional | ✓ | ✓ | ✓ (`sort` its own) | ✓ (`reverse` its own) | | |
| [map](../map/README.md) | ✓ (`contains` its own) | — | (its own `==`) | | | | ✓ | |
| [multimap](../multimap/README.md) | ✓ (`contains` its own) | — | (its own `==`) | | | | ✓ | |
| [multiset](../multiset/README.md) | ✓ (`contains` its own) | — | (its own `==`) | | | | | |
| [ordered_map](../ordered_map/README.md) | ✓ (`contains` its own) | — | (its own `==`) | | | | ✓ | |
| [ordered_set](../ordered_set/README.md) | ✓ (`contains` its own) | — | (its own `==`) | | | | | |
| [range](../range/README.md) | ✓ | the iterator's | ✓ | ✓ | ✓ | when the iterator writes | | |
| [set](../set/README.md) | ✓ (`contains` its own) | — | (its own `==`) | | | | | |
| [slice\<const T\>](../slice/README.md) | ✓ | contiguous | ✓ | ✓ | ✓ | | | |
| [slice\<T\>](../slice/README.md) | ✓ | contiguous | ✓ | ✓ | ✓ | ✓ | | |
| [sorted_map](../sorted_map/README.md) | ✓ (`contains`, `min`, `max` its own) | bidirectional | ✓ | ✓ | | | ✓ | |
| [sorted_multimap](../sorted_multimap/README.md) | ✓ (`contains`, `min`, `max` its own) | bidirectional | ✓ | ✓ | | | ✓ | |
| [sorted_multiset](../sorted_multiset/README.md) | ✓ (`contains`, `min`, `max` its own) | bidirectional | ✓ | ✓ | | | | |
| [sorted_set](../sorted_set/README.md) | ✓ (`contains`, `min`, `max` its own) | bidirectional | ✓ | ✓ | | | | |
| [string](../string/README.md) | `mixin::text` only: a string enters as `as_slice()` | | | | | | | |
| [vector](../vector/README.md) | ✓ | contiguous | ✓ | ✓ | ✓ | ✓ | | |

Not carried: the adaptors (`stack`, `queue`, `priority_queue`), `expiry_queue`, the weak containers and the
concurrent ones — none iterates as a range of values.

## See also

- [req](../req/README.md): the requirements, their rules and an example of functions that ask for them
- [core](../README.md): the containers that carry the mixins
- `tests/core/mixin.cpp`
