[sgcl](../../README.md) › [core](../README.md)

# sgcl::vector\<T\>

```cpp
#include "sgcl/core/vector.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class T>
    class vector;

    template<class T>
    class vector<unique_ptr<T>>;
}
```

`sgcl::vector<T>` is a sequence of elements stored contiguously in one buffer on the managed heap, with the
interface and the behaviour of `std::vector`: elements are constructed and destroyed one at a time, an explicit
removal destroys them at once, a reallocation moves them and destroys the moved-from ones, `clear()` keeps the
capacity. A pointer to an element may be passed to any function that expects a pointer to an element of an
array.

What differs from `std::vector` is where the memory lives. The vector object is three words: a `tracked_ptr` to
the first element of the buffer, the size and the capacity. The buffer is a managed array; the collector never
destroys a buffer, it only frees one nothing refers to any more (the buffer a reallocation abandoned, the buffer
of a vector that is gone). A `vector<tracked_ptr<T>>` is therefore the managed form of a vector of pointers: its
elements are traced, and it may hold cycles like any other managed object. There is no allocator parameter, and
`vector<bool>` is a plain vector of `bool`.

What differs from a Go slice: the vector is the owner of its elements, not a view of an array, and `push_back`
changes the vector in place instead of returning a new one. The view is [as_slice](as_slice.md): a
[slice](../slice/README.md) that keeps the buffer alive, as a Go slice keeps its array.

## Rules

- A vector holds a `tracked_ptr`, so it lives where one may: on a stack or inside a managed object, never in
  `new`/`malloc` memory, a `std` container, a global or a plain coroutine frame
  ([The rules](../README.md#the-rules), 1).
- The elements are destroyed by the vector itself, exactly when `std::vector` destroys them: on removal, on a
  reallocation (the moved-from elements) and in the destructor, wherever that runs, on a stack or in a sweep
  inside a dying managed object ([Containers](../README.md#containers)).
- A buffer is referred to only through the pointer to its first element, the one the vector holds. A
  `tracked_ptr` may not address an element of the buffer: an alias into it would keep nothing, and debug builds
  assert on the attempt ([The rules](../README.md#the-rules), 4). A raw pointer, a reference or an iterator to an
  element is valid exactly as long as with `std::vector` ([Iterator invalidation](#iterator-invalidation)).
- Thread safety is that of `std::vector`: concurrent readers, or one writer, with the program's own
  synchronization. The collector never waits for a mutator and never touches a buffer a vector still holds.

## Template parameters

| Parameter | Description |
|---|---|
| `T` | The type of the elements. Any object type that is not a reference or an array, aligned to at most 16 bytes; an operation that copies or moves elements requires `T` to be copyable or movable. `vector<unique_ptr<T>>` is a [specialization](#specializations). |

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
| [(constructor)](vector.md) | constructs the vector |
| `(destructor)` | destroys the elements; the buffer is left to the collector |
| [operator=](operator_assign.md) | assigns values to the container |
| [assign](assign.md) | assigns values to the container |

#### Element access

| Function | Description |
|---|---|
| [at](at.md) | access the element at a position, with bounds checking |
| [operator[]](operator_at.md) | access the element at a position |
| [front](front.md) | access the first element |
| [back](back.md) | access the last element |
| [data](data.md) | the buffer as a plain pointer |
| [as_slice, operator slice](as_slice.md) | the elements, or a part of them, as a slice that holds the buffer |

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
| [max_size](max_size.md) | the largest number of elements a vector may hold |
| [reserve](reserve.md) | reserves storage |
| [capacity](capacity.md) | the number of elements the current buffer holds |
| [shrink_to_fit](shrink_to_fit.md) | replaces the buffer by one sized for the elements |

#### Modifiers

| Function | Description |
|---|---|
| [clear](clear.md) | destroys every element, keeps the buffer |
| [insert](insert.md) | inserts elements |
| [emplace](emplace.md) | constructs an element in place |
| [erase](erase.md) | erases elements |
| [push_back](push_back.md) | appends an element |
| [emplace_back](emplace_back.md) | constructs an element in place at the end |
| [pop_back](pop_back.md) | removes the last element |
| [resize](resize.md) | changes the number of elements |
| [swap](swap.md) | swaps the contents |

#### From mixin::enumerable

The questions asked of the elements, carried by every container of the library
([mixin::enumerable](../mixin/enumerable/README.md)).

| Function | Description |
|---|---|
| [contains](../mixin/enumerable/contains.md) | checks whether an element is equal to a value |
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

The order of the elements ([mixin::ordered](../mixin/ordered/README.md)).

| Function | Description |
|---|---|
| `sort` | sorts the elements |
| `sort_by` | sorts the elements by a projection |
| `stable_sort` | sorts the elements, keeping the order of equal ones |
| `is_sorted` | checks whether the elements are sorted |
| `binary_search` | checks whether a sorted vector holds a value |
| `lower_bound`, `upper_bound` | the first element not less than, greater than a value, in a sorted vector |
| `sorted_index_of` | the position of a value in a sorted vector |

#### From mixin::sequence

The writes over every element ([mixin::sequence](../mixin/sequence/README.md)).

| Function | Description |
|---|---|
| `fill` | assigns a value to every element |
| `reverse` | reverses the order of the elements |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator\<=\>](operator_cmp.md) | compare the elements lexicographically ([mixin::equatable](../mixin/equatable/README.md), [mixin::comparable](../mixin/comparable/README.md)) |
| [swap](swap2.md) | swaps the contents of two vectors |
| [erase, erase_if](erase_if.md) | erase every element equal to a value, or satisfying a predicate |

## Deduction guides

```cpp
template<std::input_iterator InputIt>
vector(InputIt, InputIt) -> vector<std::iter_value_t<InputIt>>;
```

## Specializations

`vector<unique_ptr<T>>` is a `std::vector<unique_ptr<T>>` with its constructors and assignments. A `unique_ptr`
owns its object and needs no tracing, so a vector of them may live anywhere a `std::vector` may, and the objects
die when their `unique_ptr` does.

## Complexity

- Random access: constant.
- Insertion or removal at the end: amortized constant. A growth doubles the capacity; the old buffer is collected,
  not freed at once ([Benchmarks: Containers](../benchmarks.md#containers): 2.7 ns per `push_back` against 1.6 ns
  for `std::vector`).
- Insertion or removal of elements elsewhere: linear in the distance to the end of the vector.

## Iterator invalidation

| Operations | Invalidated |
|---|---|
| all read-only operations, `swap`, `as_slice` | never |
| `clear`, `operator=`, `assign` | always |
| `reserve`, `shrink_to_fit` | if the vector reallocated, all; otherwise none |
| `erase` | the erased elements and all after them, `end()` included |
| `push_back`, `emplace_back` | if the vector reallocated, all; otherwise only `end()` |
| `insert`, `emplace` | if the vector reallocated, all; otherwise those at or after the insertion point, `end()` included |
| `resize` | if the vector reallocated, all; otherwise only `end()` and the erased elements |
| `pop_back` | the erased element and `end()` |

A slice from `as_slice` is never invalidated: it holds the buffer it was taken from, and after a reallocation it
still reads the elements as they were.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int value;
    vector<tracked_ptr<Node>> edges;  // a managed buffer of traced pointers
};

int main() {
    vector numbers = {5, 3, 9, 1};
    numbers.push_back(7);
    numbers.sort();
    erase_if(numbers, [](int x) { return x > 5; });
    println("{}", numbers);

    // a cycle through the vectors of two nodes: collected like anything else
    tracked_ptr a = make_tracked<Node>(1);
    tracked_ptr b = make_tracked<Node>(2);
    a->edges.push_back(b);
    b->edges.push_back(a);

    // a vector of pointers on the stack keeps every node it holds
    vector<tracked_ptr<Node>> nodes;
    for (int i : range(100)) {
        tracked_ptr n = make_tracked<Node>(i);
        n->edges.push_back(a);
        nodes.push_back(n);
    }
    nodes.erase(nodes.begin(), nodes.begin() + 90);
    a = b = nullptr;  // the cycle stays reachable through nodes[0]->edges

    println("{} nodes, the first one {}", nodes.size(), nodes.front()->value);
    println("{} through the cycle", nodes.front()->edges.front()->edges.front()->value);
}
```

Output:

```text
[1, 3, 5]
10 nodes, the first one 90
2 through the cycle
```

## See also

- [array](../array/README.md): a buffer whose size is fixed at creation
- [deque](../deque/README.md): cheap insertion at both ends
- [slice](../slice/README.md): a view of elements that holds their buffer
- [tracked_ptr](../tracked_ptr/README.md), [unique_ptr](../unique_ptr/README.md), [make_tracked](../make_tracked.md)
- [README: Containers](../README.md#containers), [README: The rules](../README.md#the-rules)
