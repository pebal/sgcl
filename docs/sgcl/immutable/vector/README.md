[sgcl](../../README.md) › [immutable](../README.md)

# sgcl::immutable::vector\<T\>

```cpp
#include "sgcl/immutable/vector.h"   // or "sgcl/immutable.h"

namespace sgcl::immutable {
    template<class T>
    class vector;
}
```

`sgcl::immutable::vector<T>` is the immutable vector, the persistent vector of Clojure and Scala: a sequence
every operation of which returns a new vector and leaves the old one exactly as it was, the two sharing everything
but the path that changed. The elements live in a trie of 32-way branches indexed by five bits of the position
per level, the last up-to-32 elements in a tail held apart from it. Reading an element walks log32(*n*) branches
(three for a million elements, none for the last 32); `set` copies the branches on the path to the element and
the leaf, four objects for a million elements, and shares the rest; `push_back` copies the tail (32 elements at
most) and, once in 32 pushes, hangs the full tail on the trie along a copied path; `pop_back` does the reverse.
Two versions of a vector of a hundred thousand elements that differ in one element cost the one vector plus a
path
([Benchmarks: Memory of a version](../benchmarks.md#memory-of-a-version)).

The vector is four words: the size, the height of the trie and a `tracked_ptr` to the root and to the tail. A
copy of it is a copy of those words. The branches and the leaves are managed objects that no version owns: a leaf
reached by ten versions is one leaf, and the collector frees it once the last version that reaches it is dropped.
That is the question a persistent structure in a language without a collector answers with a reference count per
node, and the reason these structures belong in a library that has one: the sharing costs nothing to account
for.

What differs from `std::vector` and from a Go slice: nothing is ever modified. `push_back`, `pop_back` and `set`
are `const` and return the new vector, and a vector held by any number of threads is read by all of them without
a lock. The mutable [vector](../../core/vector/README.md) stays the default for everything else: its `push_back` writes in
place and its random access is one load, where the trie's is a walk of a few branches.

## Rules

- A vector holds its root and its tail by `tracked_ptr`, so it lives where one may: on a thread's stack or inside
  a managed object ([The rules](../../core/README.md#the-rules), 1).
- Every member but the assignment is `const`. `push_back`, `emplace_back`, `pop_back` and `set` return the new
  vector; the one they were called on is unchanged, and stays so for as long as it is held. The elements are
  reached as `const`.
- A change copies `T`s: `push_back` and `pop_back` copy the elements of the tail (up to 32), `set` the 32 elements
  of the leaf holding the position. The copy constructor of `T` is what a change costs, plus the branches on the
  path, 256 bytes each.
- A `T` holding tracked pointers is traced where it lives, in a leaf; a `T` with a destructor is destroyed when
  the collector frees its leaf, once no version reaches it. Nothing is destroyed by a `pop_back`: the old version
  still holds the element.
- An iterator holds nothing alive: it is valid while the vector object it was taken from exists and holds the
  same version, as an iterator of `std` is.
- Sharing between threads: any number of threads read any version. A vector variable that one thread replaces
  while others read it is the one thing that needs synchronization, and
  [concurrent::copy_on_write](../../concurrent/copy_on_write/README.md) is the shape for it: `load()` is one atomic load for
  a snapshot, `update(f)` copies the four words, applies `f` (a `push_back`, a `set`) and swings the pointer, so an
  update costs O(log *n*) where a `copy_on_write` of a mutable vector costs a copy of everything. An
  [atomic](../../core/atomic.md) `tracked_ptr` to a vector does the same with the version in a managed object of its
  own.
- `operator[]`, `front` and `back` are unchecked, `at` and `set` throw `out_of_range`; `pop_back` on an empty
  vector is undefined, as `std::vector`'s is.

## Template parameters

| Parameter | Description |
|---|---|
| `T` | The type of the elements: an object type that is not a reference or an array. A change copies elements, so `push_back`, `emplace_back`, `pop_back` and `set` require `T` to be copy constructible. |

## Member types

| Type | Definition |
|---|---|
| `value_type` | `T` |
| `reference` | `const T&` |
| `const_reference` | `const T&` |
| `pointer` | `const T*` |
| `const_pointer` | `const T*` |
| `size_type` | `size_t` |
| `difference_type` | `ptrdiff_t` |
| `const_iterator` | a random-access iterator over `const T`, `std::random_access_iterator` |
| `iterator` | `const_iterator` |
| `const_reverse_iterator` | `std::reverse_iterator<const_iterator>` |
| `reverse_iterator` | `const_reverse_iterator` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](vector.md) | constructs the vector |
| `(destructor)` | drops this version; the nodes no other version reaches are left to the collector |
| [operator=](operator_assign.md) | makes the variable hold another version |

#### Element access

| Function | Description |
|---|---|
| [at](at.md) | access the element at a position, with bounds checking |
| [operator[]](operator_at.md) | access the element at a position |
| [front](front.md) | access the first element |
| [back](back.md) | access the last element |

#### Iterators

| Function | Description |
|---|---|
| [begin, cbegin](begin.md) | an iterator to the beginning |
| [end, cend](end.md) | an iterator to the end |
| [rbegin, crbegin](rbegin.md) | a reverse iterator to the beginning |
| [rend, crend](rend.md) | a reverse iterator to the end |

#### Capacity

| Function | Description |
|---|---|
| [empty](empty.md) | checks whether the vector is empty |
| [size](size.md) | the number of elements |
| [depth](depth.md) | the number of levels of branches a random access walks |

#### New versions

| Function | Description |
|---|---|
| [push_back](push_back.md) | the vector with one more element at the end |
| [emplace_back](emplace_back.md) | the same, the element constructed from arguments |
| [pop_back](pop_back.md) | the vector without its last element |
| [set](set.md) | the vector with the element at a position replaced |
| [update](update.md) | the vector with a function of the element at a position in its place |

#### From mixin::enumerable

The questions asked of the elements, carried by every container of the library
([mixin::enumerable](../../core/mixin/enumerable/README.md)).

| Function | Description |
|---|---|
| [contains](../../core/mixin/enumerable/contains.md) | checks whether an element is equal to a value |
| `index_of` | the position of the first element equal to a value |
| `last_index_of` | the position of the last element equal to a value |
| `find_if` | a pointer to the first element the predicate accepts |
| `find_index` | the position of the first element the predicate accepts |
| `exists` | checks whether the predicate accepts some element |
| `all` | checks whether the predicate accepts every element |
| `count_of` | the number of elements the predicate accepts |
| `min`, `max` | the smallest, the largest element |
| `for_each` | calls a function with every element |

#### From mixin::ordered

The order of the elements ([mixin::ordered](../../core/mixin/ordered/README.md)). The sorts are not here: nothing is
written in place.

| Function | Description |
|---|---|
| `is_sorted` | checks whether the elements are sorted |
| `binary_search` | checks whether a sorted vector holds a value |
| `lower_bound`, `upper_bound` | the first element not less than, greater than a value, in a sorted vector |
| `sorted_index_of` | the position of a value in a sorted vector |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator!=](operator_cmp.md) | compare the elements |
| `operator<=>` | compares the elements lexicographically ([mixin::comparable](../../core/mixin/comparable/README.md)) |

## Deduction guides

```cpp
template<std::input_iterator InputIt>
vector(InputIt, InputIt) -> vector<typename std::iterator_traits<InputIt>::value_type>;
```

## Complexity

- Random access: logarithmic in the size, base 32: `depth()` branches and a leaf, none for the last 32 elements.
- `push_back`, `pop_back`: constant in practice; a copy of the tail, at most 32 elements, and once in 32 changes a
  path of `depth()` branches.
- `set`: logarithmic in the size, base 32: the path of branches and a leaf copied.
- Copy and assignment: constant, four words.

## Iterator invalidation

| Operations | Invalidated |
|---|---|
| every member but `operator=` | never: nothing changes the vector |
| `operator=` | always |

An iterator keeps the vector object it was taken from, not the version: once the variable holds another version,
the iterator is not valid.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

// An undo history in one line per step: every version of the text is kept,
// and the versions share all but what each step changed
int main() {
    vector<immutable::vector<char>> history;
    immutable::vector<char> text;
    string word = "persistent";
    for (char c : word) {
        text = text.push_back(c);
        history.push_back(text);  // a version: four words, no copy of the text
    }
    text = text.set(0, 'P');
    history.push_back(text);
    for (const auto& version : history) {
        println("{}", string(version.begin(), version.end()));
    }

    // two versions differing in one element share the rest
    immutable::vector<int> big;
    for (int i : range(100000)) {
        big = big.push_back(i);
    }
    auto changed = big.set(50000, -1);
    println("{} {} {} levels", big[50000], changed[50000], big.depth());
}
```

Output:

```text
p
pe
per
pers
persi
persis
persist
persiste
persisten
persistent
Persistent
50000 -1 3 levels
```

## See also

- [list](../list/README.md): the immutable sequence read from the front
- [map](../map/README.md), [set](../set/README.md): the immutable hash map and set
- [concurrent::copy_on_write](../../concurrent/copy_on_write/README.md): how a version is published to other threads;
  [atomic](../../core/atomic.md)
- [vector](../../core/vector/README.md): the mutable one
- [README: The rules](../README.md#the-rules), [Benchmarks](../benchmarks.md)
