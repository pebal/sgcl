[sgcl](../README.md) › [core](README.md)

# sgcl::dynamic_array\<T\>

```cpp
#include "sgcl/core/dynamic_array.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class T>
    class dynamic_array;
}
```

`sgcl::dynamic_array<T>` is an array whose size is a value, fixed when the array is created and never changed
after: Java's `new T[n]`, C#'s `T[]`, what `std::dynarray` was to be. It is a handle of two words — a
`tracked_ptr` to the first element and the count — that lives on a stack or inside a managed object; the elements
live in a buffer on the managed heap, which the collector reclaims once nothing refers to it. Copying copies the
elements into a buffer of its own; moving passes the buffer on. It has the members of every range of the library
([the mixins](mixin/README.md)), `fill` among them, and `swap`: no capacity, no growth.

What it has that [vector](vector.md) has not: **the buffer never moves**. A pointer, an iterator or a
[slice](slice.md) to an element is valid for as long as the array is, not until the next `push_back`; threads may
keep pointers to its slots; and a field of this type says in its type that it does not grow — the rings of
[channel](../async/channel.md) and [broadcast](../async/broadcast.md) and the buckets of the concurrent containers
are `dynamic_array`s. For `N` known at compile time, inline: [array](array.md).

## Rules

- A `dynamic_array` holds a `tracked_ptr`, so it lives where one may: on a stack or inside a managed object, never
  in `new`/`malloc` memory, a `std` container, a global or a plain coroutine frame
  ([The rules](README.md#the-rules), 1).
- It destroys its elements itself, in its destructor and on assignment, wherever the handle dies, on a stack or in
  a sweep inside a dying managed object. The collector never destroys a buffer, it only frees one nothing refers
  to any more ([Containers](README.md#containers)).
- The buffer is referred to only through the pointer to its first element, the one the handle holds. A
  `tracked_ptr` may not address an element of it: an alias into it would keep nothing, and debug builds assert on
  the attempt ([The rules](README.md#the-rules), 4). A raw pointer, a reference, an iterator or a slice of an
  element is valid until the handle is assigned over, moved from or destroyed
  ([Iterator invalidation](#iterator-invalidation)).
- Iterators are raw pointers in a thin class (`std::contiguous_iterator`), cheap to copy, at home in any
  container; the array carries the contiguous category ([req::contiguous](req/contiguous.md)).
- Thread safety is that of a `std::vector`: concurrent readers, or one writer, with the program's own
  synchronization.

## Template parameters

| Parameter | Description |
|---|---|
| `T` | The type of the elements: any object type that is not a reference or an array. An operation that copies or moves elements requires `T` to be copyable or movable. |

## Member types

| Type | Definition |
|---|---|
| `value_type` | `T` |
| `size_type` | `size_t` |
| `difference_type` | `ptrdiff_t` |
| `reference` | `T&` |
| `const_reference` | `const T&` |
| `pointer` | `T*` |
| `const_pointer` | `const T*` |
| `iterator` | a pointer to `T` in a class of the library, `std::contiguous_iterator` |
| `const_iterator` | the same over `const T` |
| `reverse_iterator` | `std::reverse_iterator<iterator>` |
| `const_reverse_iterator` | `std::reverse_iterator<const_iterator>` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](dynamic_array/dynamic_array.md) | constructs the array |
| `(destructor)` | destroys the elements, wherever the handle dies; the buffer is left to the collector |
| [operator=](dynamic_array/operator_assign.md) | assigns values to the array |

#### Element access

| Function | Description |
|---|---|
| [at](dynamic_array/at.md) | access the element at a position, with bounds checking |
| [operator[]](dynamic_array/operator_at.md) | access the element at a position |
| [front](dynamic_array/front.md) | access the first element |
| [back](dynamic_array/back.md) | access the last element |
| [data](dynamic_array/data.md) | the buffer as a plain pointer |
| [as_slice, operator slice](dynamic_array/as_slice.md) | the elements, or a part of them, as a slice that holds the buffer |

#### Iterators

| Function | Description |
|---|---|
| [begin, cbegin](dynamic_array/begin.md) | an iterator to the beginning |
| [end, cend](dynamic_array/end.md) | an iterator to the end |
| [rbegin, crbegin](dynamic_array/rbegin.md) | a reverse iterator to the beginning |
| [rend, crend](dynamic_array/rend.md) | a reverse iterator to the end |

#### Capacity

| Function | Description |
|---|---|
| [empty](dynamic_array/empty.md) | checks whether the array is empty |
| [size](dynamic_array/size.md) | the number of elements |
| [max_size](dynamic_array/max_size.md) | the largest number of elements an array may hold |

#### Modifiers

| Function | Description |
|---|---|
| [swap](dynamic_array/swap.md) | swaps the contents |

#### From mixin::enumerable

The questions asked of the elements, carried by every container of the library
([mixin::enumerable](mixin/enumerable.md)).

| Function | Description |
|---|---|
| [contains](mixin/enumerable/contains.md) | checks whether an element is equal to a value |
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

The order of the elements ([mixin::ordered](mixin/ordered.md)).

| Function | Description |
|---|---|
| `sort` | sorts the elements |
| `sort_by` | sorts the elements by a projection |
| `stable_sort` | sorts the elements, keeping the order of equal ones |
| `is_sorted` | checks whether the elements are sorted |
| `binary_search` | checks whether a sorted array holds a value |
| `lower_bound`, `upper_bound` | the first element not less than, greater than a value, in a sorted array |
| `sorted_index_of` | the position of a value in a sorted array |

#### From mixin::sequence

The writes over every element ([mixin::sequence](mixin/sequence.md)).

| Function | Description |
|---|---|
| `fill` | assigns a value to every element |
| `reverse` | reverses the order of the elements |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator\<=\>](dynamic_array/operator_cmp.md) | compare the elements lexicographically |
| [swap](dynamic_array/swap2.md) | swaps the contents of two arrays |

## Deduction guides

```cpp
template<std::input_iterator InputIt>
dynamic_array(InputIt, InputIt) -> dynamic_array<std::iter_value_t<InputIt>>;
```

## Complexity

- Random access: constant.
- Construction and copy: linear in the number of elements; a move and `swap`: constant.

## Iterator invalidation

| Operations | Invalidated |
|---|---|
| all read-only operations, the writes of the mixins, `swap` | never |
| `operator=` of a copy of the same size | never: the elements are assigned in place |
| `operator=` of another size, from a list, by a move | always |
| the destructor | always |

After `swap` an iterator still points to its element, which now belongs to the other array. A slice from
`as_slice` holds its buffer and is never left on freed memory.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <numeric>

using namespace sgcl;

struct Holder {
    dynamic_array<int> values;  // a managed buffer, one handle inside the object
};

int main() {
    // A buffer sized at creation: the elements are on the managed heap
    dynamic_array<int> squares(8);
    for (size_t i : range(squares.size())) {
        squares[i] = int(i * i);
    }
    int* third = &squares[3];  // stays valid: the buffer never moves
    dynamic_array copy = squares;  // a deep copy, a buffer of its own
    copy.reverse();

    // Inside a managed object: the buffer goes with the object
    tracked_ptr h = make_tracked<Holder>();
    h->values = std::move(squares);  // the buffer is handed over, squares is empty
    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    collector::force_collect(true);

    int sum = std::accumulate(h->values.begin(), h->values.end(), 0);
    println("sum of squares {}, reversed copy starts with {}", sum, copy.front());
    println("third {}, squares empty: {}", *third, squares.empty());
}
```

Output:

```text
sum of squares 140, reversed copy starts with 49
third 9, squares empty: true
```

## See also

- [array](array.md): `N` in the type, the elements inline
- [vector](vector.md): a buffer that grows
- [slice](slice.md): a piece of the buffer that holds it
- [the mixins and the requirements](mixin/README.md), [tracked_ptr](tracked_ptr.md), [make_tracked](make_tracked.md)
- [README: Containers](README.md#containers), [README: The rules](README.md#the-rules)
