[sgcl](../README.md) › [core](README.md)

# sgcl::forward_list\<T\>

```cpp
#include "sgcl/core/forward_list.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class T>
    class forward_list;

    template<class T>
    class forward_list<unique_ptr<T>>;
}
```

`sgcl::forward_list<T>` is `std::forward_list` over managed nodes: singly linked nodes behind a sentinel, the one
`before_begin()` addresses. The interface is the one of `std::forward_list` (constructors, `assign`, `front`,
forward iterators, `insert_after`, `emplace_after`, `erase_after`, `push_front`, `merge`, `splice_after`, `remove`,
`remove_if`, `reverse`, `unique`, `sort`, three-way comparison, `std::erase` and `std::erase_if`; no `size()`), and
so is the behaviour: an element is constructed at insertion and destroyed at removal, references and iterators to
the other elements stay valid through every insertion, erasure and relink.

What differs is who frees the nodes. The links are `tracked_ptr`s, so the sentinel keeps every node alive and the
list walks them through raw pointers; an `erase_after` unlinks a node and destroys its element, and the collector
reclaims the node later, once nothing refers to it. Nothing is ever freed by hand, so a cycle through a list is
collected like any other cycle. The list object is one word, the sentinel: a bare link with no element inside the
list itself, not a node of its own, so an empty list allocates nothing and `insert_after` and `erase_after` work the
same at any position. A node is as big as its `std` counterpart and pays no malloc rounding
([Benchmarks: Containers](benchmarks.md#containers): 12.8 ns per `push_front` against 21.3 ns for
`std::forward_list`).

There is no allocator parameter. Beside the members of `std::forward_list`, a list carries the questions, the
searches and the writes of the [mixins](mixin/README.md) every sequence of the library has, so that `l.contains(x)`
and `l.min()` read as `l.push_front(x)` does.

## Rules

- A forward_list holds a `tracked_ptr` (the sentinel), so it lives where one may: on a stack or inside a managed
  object, never in `new`/`malloc` memory, a `std` container, a global or a plain coroutine frame
  ([The rules](README.md#the-rules), 1).
- The list destroys an element the moment it is erased, popped, cleared, assigned over or the list is destroyed,
  exactly like `std::forward_list`. The one exception is a list dying in a sweep, inside a managed object nobody
  refers to any more: its nodes are garbage of the same sweep, and each destroys its element when the sweep reaches
  it ([Containers](README.md#containers)).
- An iterator to an erased element is invalid as in `std`: it keeps the node's memory mapped but not the element.
  A `tracked_ptr` may not address an element of a list ([The rules](README.md#the-rules), 4): elements are reached
  through the list, its iterators, references and raw pointers.
- Iterators are raw node pointers, trivially copyable and at home in any container, a `std::vector` too: the list
  roots every linked node. Stepping and dereferencing are plain loads, with no write barrier.
- Relinking operations (`splice_after`, `merge`, `sort`, `reverse`) move nodes, never elements, and every node
  stays reachable through a `tracked_ptr` while it is being moved: every reference stays valid.
- Thread safety is that of `std::forward_list`: concurrent readers, or one writer, with the program's own
  synchronization. The collector never waits for a mutator and never touches a node the list still links.

## Template parameters

| Parameter | Description |
|---|---|
| `T` | The type of the elements: any object type that is not a reference or an array; an operation that copies or moves elements requires `T` to be copyable or movable, and the constructor and the `resize` of a number alone a `T` that is default-initializable. `forward_list<unique_ptr<T>>` is a [specialization](#specializations). |

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
| `iterator` | a pointer to a node in a class of the library, `std::forward_iterator` over `T` |
| `const_iterator` | the same over `const T`; an `iterator` converts to it |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](forward_list/forward_list.md) | constructs the list |
| `(destructor)` | destroys the elements, unless the list dies in a sweep; the nodes are left to the collector |
| [operator=](forward_list/operator_assign.md) | assigns values to the container |
| [assign](forward_list/assign.md) | assigns values to the container, in the nodes it has |

#### Element access

| Function | Description |
|---|---|
| [front](forward_list/front.md) | access the first element |

#### Iterators

| Function | Description |
|---|---|
| [before_begin, cbefore_begin](forward_list/before_begin.md) | an iterator to the sentinel, before the first element |
| [begin, cbegin](forward_list/begin.md) | an iterator to the beginning |
| [end, cend](forward_list/end.md) | an iterator to the end |

#### Capacity

| Function | Description |
|---|---|
| [empty](forward_list/empty.md) | checks whether the list is empty |
| [max_size](forward_list/max_size.md) | the largest number of elements a list may hold |

#### Modifiers

| Function | Description |
|---|---|
| [clear](forward_list/clear.md) | destroys every element |
| [insert_after](forward_list/insert_after.md) | inserts elements after an element |
| [emplace_after](forward_list/emplace_after.md) | constructs an element in place after an element |
| [erase_after](forward_list/erase_after.md) | erases an element after an element, or a range |
| [push_front](forward_list/push_front.md) | inserts an element at the beginning |
| [emplace_front](forward_list/emplace_front.md) | constructs an element in place at the beginning |
| [pop_front](forward_list/pop_front.md) | removes the first element |
| [resize](forward_list/resize.md) | changes the number of elements |
| [swap](forward_list/swap.md) | swaps the contents |

#### Operations

| Function | Description |
|---|---|
| [merge](forward_list/merge.md) | merges two sorted lists |
| [splice_after](forward_list/splice_after.md) | moves nodes from another list, or within this one |
| [remove, remove_if](forward_list/remove.md) | erases the elements equal to a value, or satisfying a predicate |
| [reverse](forward_list/reverse.md) | reverses the order of the nodes |
| [unique](forward_list/unique.md) | erases consecutive equal elements |
| [sort](forward_list/sort.md) | sorts the nodes, stable |

#### From mixin::enumerable

The questions asked of the elements, carried by every container of the library
([mixin::enumerable](mixin/enumerable.md)). A question that compares elements exists only for elements that
compare.

| Function | Description |
|---|---|
| [contains](mixin/enumerable/contains.md) | checks whether an element is equal to a value |
| `index_of` | the position of the first element equal to a value, `npos` when none |
| `last_index_of` | the position of the last element equal to a value, `npos` when none |
| `find_if` | a pointer to the first element the predicate accepts, null when none |
| `find_index` | the position of the first element the predicate accepts, `npos` when none |
| `exists` | checks whether the predicate accepts some element |
| `all` | checks whether the predicate accepts every element |
| `count_of` | the number of elements the predicate accepts |
| `min`, `max` | the smallest, the largest element |
| `for_each` | calls a function with every element |

#### From mixin::ordered

The order of the elements ([mixin::ordered](mixin/ordered.md)). The mixin's sorts are not a list's: `sort` is the
list's own, on the nodes, and `sort_by` and `stable_sort` need random access.

| Function | Description |
|---|---|
| `is_sorted` | checks whether the elements are sorted |
| `binary_search` | checks whether a sorted list holds a value |
| `lower_bound`, `upper_bound` | the first element not less than, greater than a value, in a sorted list |
| `sorted_index_of` | the position of a value in a sorted list |

#### From mixin::sequence

The writes over every element ([mixin::sequence](mixin/sequence.md)); `reverse` is the list's own, on the nodes.

| Function | Description |
|---|---|
| `fill` | assigns a value to every element |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator\<=\>](forward_list/operator_cmp.md) | compare the elements lexicographically |
| [swap](forward_list/swap2.md) | swaps the contents of two lists |
| [erase, erase_if](forward_list/erase_if.md) | erase every element equal to a value, or satisfying a predicate |

## Specializations

`forward_list<unique_ptr<T>>` is a `std::forward_list<unique_ptr<T>>` with the constructors of the base. A
`unique_ptr` owns its object and needs no tracing, so a list of them may live anywhere a `std::forward_list` may,
and the objects die when their `unique_ptr` does, at the `pop_front` or the `erase_after` that removes it.

## Complexity

- Insertion or removal of an element after a given one: constant. A node is allocated per element
  ([Benchmarks: Containers](benchmarks.md#containers)).
- Access to the first element: constant. There is no access by position and no `size()`:
  `std::ranges::distance(l)` counts the nodes.
- `splice_after` of one element: constant; of a whole list, linear in its size, walked to its last node; of a range,
  linear in the range.
- `merge`, `remove`, `remove_if`, `unique`, `reverse`: linear. `sort`: `N log N` comparisons.

## Iterator invalidation

| Operations | Invalidated |
|---|---|
| all read-only operations, `swap`, `splice_after`, `merge`, `sort`, `reverse` | never; an iterator to a moved node names its element in the list it is in now |
| `insert_after`, `emplace_after`, `push_front`, `emplace_front` | never |
| `erase_after`, `pop_front`, `remove`, `remove_if`, `unique` | the erased elements |
| `resize`, `assign`, copy `operator=` | the erased elements, when the list shrinks |
| `clear`, move `operator=` | all the elements of this list |

`before_begin()` addresses the sentinel inside the list object: it stays valid as long as the list object, and
`end()` is a null iterator, valid always.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <ranges>

using namespace sgcl;

struct Vertex {
    int id;
    forward_list<tracked_ptr<Vertex>> edges;  // an adjacency list, inside the vertex
};

int main() {
    println("values and a graph in forward lists");
    // counted after the first line: io's own objects are not the example's
    auto base = collector::get_live_object_count();

    // a list of values on the stack: the nodes are on the managed heap
    forward_list numbers = {5, 3, 9, 1};
    numbers.push_front(7);
    numbers.sort();  // the nodes relinked in place
    numbers.remove_if([](int x) { return x > 5; });

    // a graph: every vertex points at every other, so every vertex is in a cycle
    forward_list<tracked_ptr<Vertex>> vertices;
    for (int i : range(100)) {
        vertices.push_front(make_tracked<Vertex>(i));
    }
    for (const auto& v : vertices) {
        for (const auto& w : vertices) {
            if (v != w) {
                v->edges.push_front(w);
            }
        }
    }

    // keep one vertex, drop the list: the whole graph stays reachable through it
    tracked_ptr keep = vertices.front();
    vertices.clear();
    // the odd ids leave its edges, kept by the cycles still
    keep->edges.remove_if([](const tracked_ptr<Vertex>& w) { return w->id % 2; });

    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    collector::force_collect(true);
    println("{} numbers {}", std::ranges::distance(numbers), numbers);
    println("vertex {} with {} edges", keep->id, std::ranges::distance(keep->edges));
    println("{} live objects", collector::get_live_object_count() - base);

    keep = nullptr;  // the cycle is unreachable now: collected as a whole
    collector::force_collect(true);  // optional, as above
    println("{} live objects after the graph is gone", collector::get_live_object_count() - base);
}
```

Output:

```text
values and a graph in forward lists
3 numbers [1, 3, 5]
vertex 99 with 50 edges
9954 live objects
3 live objects after the graph is gone
```

The list `numbers` dies at the end of `main` and destroys its elements then; its nodes are freed by the collector
once nothing refers to them.

## See also

- [list](list.md): a doubly linked list with `size()`, `push_back` and reverse iteration
- [tracked_ptr](tracked_ptr.md), [unique_ptr](unique_ptr.md), [make_tracked](make_tracked.md)
- [README: Containers](README.md#containers), [README: The rules](README.md#the-rules),
  [Stack roots](../../garbage_collector/overview.md#stack-roots)
