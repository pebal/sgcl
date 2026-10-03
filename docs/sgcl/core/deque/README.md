[sgcl](../../README.md) › [core](../README.md)

# sgcl::deque\<T\>

```cpp
#include "sgcl/core/deque.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class T>
    class deque;

    template<class T>
    class deque<unique_ptr<T>>;
}
```

`sgcl::deque<T>` is `std::deque` over managed memory. The interface is the one of `std::deque` (constructors,
`assign`, element access, random-access iterators, `shrink_to_fit`, modifiers at both ends and in the middle,
three-way comparison, `std::erase` and `std::erase_if`), and so is the behaviour: elements are constructed at
insertion and destroyed at removal, references stay valid across a push or pop at either end, iterators do not.

What differs is where the memory lives. The elements live in blocks on the managed heap (a block holds as many
elements as fit in 4 KB, rounded down to a power of two, at least one), addressed through a managed array of
block pointers, the map; the deque object is four words (the map, its size, the index of the first element, the
count). A map that is outgrown is replaced by a fresh one, never shifted in place. A block emptied by pops stays
in the map as the spare block of its end, so a window of elements travelling through the deque allocates no
blocks; every block goes when the deque becomes empty. Nothing is ever freed by hand: the blocks and the maps the
deque lets go of are reclaimed by the collector once nothing refers to them. A `sgcl::deque<tracked_ptr<T>>` is
the managed form of a deque of pointers: its elements are traced, and it may hold cycles like any other managed
object.

## Rules

- A deque holds a `tracked_ptr` (the map), so it lives where one may: on a stack or inside a managed object, never
  in `new`/`malloc` memory, a `std` container, a global or a plain coroutine frame
  ([The rules](../README.md#the-rules), 1).
- The deque destroys its elements itself, exactly when `std::deque` does: on removal (`erase`, `pop_back`,
  `pop_front`, `clear`, `resize`, `assign`) and in the destructor. The one exception is a deque dying in a sweep,
  inside a managed object nobody refers to any more: its blocks are garbage of the same sweep, and each destroys
  the elements it still holds when the sweep reaches it ([Containers](../README.md#containers)).
- A `tracked_ptr` may not address an element of a deque ([The rules](../README.md#the-rules), 4): elements are
  reached through the deque, its iterators, references and raw pointers, valid exactly as long as with
  `std::deque` ([Iterator invalidation](#iterator-invalidation)). An invalid iterator must not be used, as in
  `std`.
- Iterators are raw (a pointer to the map, an index, a pointer to the element), cheap to copy, and may live
  anywhere, in a `std::vector` too. An iterator dying in a frame nulls its words, so a temporary left behind does
  not root the blocks under the conservative stack scan
  ([Stack roots](../../../garbage_collector/overview.md#stack-roots)).
- Thread safety is that of `std::deque`: concurrent readers, or one writer, with the program's own
  synchronization. The collector never waits for a mutator and never touches a block the deque still holds.

## Template parameters

| Parameter | Description |
|---|---|
| `T` | The type of the elements. An operation that copies or moves elements requires `T` to be copyable or movable. `deque<unique_ptr<T>>` is a [specialization](#specializations). |

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
| `iterator` | an iterator over `T` in a class of the library, `std::random_access_iterator`; converts to `const_iterator` |
| `const_iterator` | the same over `const T` |
| `reverse_iterator` | `std::reverse_iterator<iterator>` |
| `const_reverse_iterator` | `std::reverse_iterator<const_iterator>` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](deque.md) | constructs the deque |
| `(destructor)` | destroys the elements, unless the deque dies in a sweep; the blocks and the map are left to the collector |
| [operator=](operator_assign.md) | assigns values to the container |
| [assign](assign.md) | assigns values to the container |

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
| [empty](empty.md) | checks whether the deque is empty |
| [size](size.md) | the number of elements |
| [max_size](max_size.md) | the largest number of elements a deque may hold |
| [shrink_to_fit](shrink_to_fit.md) | replaces the map by one holding only the blocks in use |

#### Modifiers

| Function | Description |
|---|---|
| [clear](clear.md) | destroys every element, drops the blocks and the map |
| [insert](insert.md) | inserts elements |
| [emplace](emplace.md) | constructs an element in place |
| [erase](erase.md) | erases elements |
| [push_back](push_back.md) | appends an element at the end |
| [emplace_back](emplace_back.md) | constructs an element in place at the end |
| [pop_back](pop_back.md) | removes the last element |
| [push_front](push_front.md) | inserts an element at the beginning |
| [emplace_front](emplace_front.md) | constructs an element in place at the beginning |
| [pop_front](pop_front.md) | removes the first element |
| [resize](resize.md) | changes the number of elements |
| [swap](swap.md) | swaps the contents |

#### From mixin::enumerable

The questions asked of the elements, carried by every container of the library
([mixin::enumerable](../mixin/enumerable/README.md)). A question that compares elements exists only for elements that
compare.

| Function | Description |
|---|---|
| [contains](../mixin/enumerable/contains.md) | checks whether an element is equal to a value |
| `index_of` | the position of the first element equal to a value, or `npos` |
| `last_index_of` | the position of the last element equal to a value, or `npos` |
| `find_if` | a pointer to the first element the predicate accepts, or null |
| `find_index` | the position of the first element the predicate accepts, or `npos` |
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
| `binary_search` | checks whether a sorted deque holds a value |
| `lower_bound`, `upper_bound` | the first element not less than, greater than a value, in a sorted deque |
| `sorted_index_of` | the position of a value in a sorted deque |

#### From mixin::sequence

The writes over every element ([mixin::sequence](../mixin/sequence/README.md)).

| Function | Description |
|---|---|
| `fill` | assigns a value to every element |
| `reverse` | reverses the order of the elements |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator\<=\>](operator_cmp.md) | compare the elements lexicographically |
| [swap](swap2.md) | swaps the contents of two deques |
| [erase, erase_if](erase_if.md) | erase every element equal to a value, or satisfying a predicate |

## Deduction guides

```cpp
template<std::input_iterator InputIt>
deque(InputIt, InputIt) -> deque<std::iter_value_t<InputIt>>;
```

## Specializations

`deque<unique_ptr<T>>` is a `std::deque<unique_ptr<T>>` with the constructors of the base. A `unique_ptr` owns
its object and needs no tracing, so a deque of them may live anywhere a `std::deque` may, and the objects die when
their `unique_ptr` does: a `pop_front` destroys the object at once.

## Complexity

- Random access: constant, a division by the block size and two loads.
- Insertion or removal at either end: constant. A push is a load of the map, a load of the block, the
  construction and two stores (the block's range and the count); the map at its end or a missing block goes the
  slow way ([Benchmarks: Containers](../benchmarks.md#containers): 2.0 ns per push against 1.5 ns for
  `std::deque`).
- Insertion or removal of elements elsewhere: linear in the distance to the nearer end of the deque.

## Iterator invalidation

| Operations | Invalidated |
|---|---|
| all read-only operations, `swap` | never |
| `clear`, `operator=`, `assign` | always |
| `shrink_to_fit` | the iterators, when the map is replaced; the references stay valid |
| `push_front`, `emplace_front`, `push_back`, `emplace_back` | the iterators; the references stay valid |
| `insert`, `emplace` | at either end, the iterators, the references staying valid; elsewhere, all |
| `pop_front` | the erased element; `end()` too when the deque becomes empty |
| `pop_back` | the erased element and `end()` |
| `erase` | at the beginning, the erased elements; at the end, the erased elements and `end()`; elsewhere, all |
| `resize` | when the deque grows, the iterators, the references staying valid; when it shrinks, the erased elements and `end()` |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Job {
    int id;
    vector<tracked_ptr<Job>> depends_on;  // a job may wait for others
};

struct Scheduler {
    deque<tracked_ptr<Job>> pending;  // inside a managed object: traced with it
};

int main() {
    // a work queue on the stack: pushes at the back, pops at the front,
    // and a window of elements travelling through it allocates no blocks
    deque<int> window;
    for (int i : range(100000)) {
        window.push_back(i);
        if (window.size() > 16) {
            window.pop_front();  // the emptied block stays as the spare
        }
    }

    // a scheduler in a managed object; urgent jobs go to the front
    tracked_ptr s = make_tracked<Scheduler>();
    for (int i : range(1000)) {
        tracked_ptr job = make_tracked<Job>(i);
        if (i % 100 == 0) {
            s->pending.push_front(job);
        } else {
            s->pending.push_back(job);
            s->pending.front()->depends_on.push_back(job);  // the urgent job waits for it
        }
    }

    // drain the front half: a popped job nothing else refers to is garbage,
    // a popped urgent job lives on while a pending job it waits for is held
    int drained = 0;
    while (s->pending.size() > 500) {
        s->pending.pop_front();
        ++drained;
    }
    // optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    collector::force_collect(true);
    println("{} jobs drained, {} pending, first is job {}; {} live objects", drained,
            s->pending.size(), s->pending.front()->id, collector::get_live_object_count());
    println("window of {} from {}", window.size(), window.front());
}
```

Output:

```text
500 jobs drained, 500 pending, first is job 495; 508 live objects
window of 16 from 99984
```

The deque `window` dies at the end of `main` and destroys its sixteen elements then; its blocks and its map are
freed by the collector once nothing refers to them.

## See also

- [vector](../vector/README.md): a contiguous buffer
- [list](../list/README.md): stable references
- [stack](../stack/README.md), [queue](../queue/README.md): the adapters over a deque
- [tracked_ptr](../tracked_ptr/README.md), [unique_ptr](../unique_ptr/README.md), [make_tracked](../make_tracked.md)
- [README: Containers](../README.md#containers), [README: The rules](../README.md#the-rules),
  [Stack roots](../../../garbage_collector/overview.md#stack-roots)
