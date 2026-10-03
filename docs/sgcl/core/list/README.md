[sgcl](../../README.md) › [core](../README.md)

# sgcl::list\<T\>

```cpp
#include "sgcl/core/list.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class T>
    class list;

    template<class T>
    class list<unique_ptr<T>>;
}
```

`sgcl::list<T>` is `std::list` over managed nodes: a circular doubly linked list around a managed sentinel. The
interface is the one of `std::list` (constructors, `assign`, `front` and `back`, bidirectional iterators, modifiers
at both ends and in the middle, `merge`, `splice`, `remove`, `remove_if`, `reverse`, `unique`, `sort`, three-way
comparison, `std::erase` and `std::erase_if`), and so is the behaviour: an element is constructed at insertion and
destroyed at removal, references and iterators to the other elements stay valid through every insertion, erasure
and relink.

What differs is who frees the nodes. The links are `tracked_ptr`s, so a rooted sentinel keeps every node alive and
the list walks them through raw pointers; an `erase` unlinks a node and destroys its element, and the collector
reclaims the node later, once nothing refers to it. Nothing is ever freed by hand, so a cycle through a list is
collected like any other cycle. The list object is two words, the sentinel and the count; the sentinel is created
on first use, so a default-constructed list allocates nothing. A node is as big as its `std` counterpart and pays
no malloc rounding ([Benchmarks: Containers](../benchmarks.md#containers): 15.8 ns per `push_back` against 25.6 ns
for `std::list`, 1.9 ns per step of iteration for both).

There is no allocator parameter. Beside the members of `std::list`, a list carries the questions, the searches and
the writes of the [mixins](../mixin/README.md) every sequence of the library has, so that `l.contains(x)` and
`l.min()` read as `l.push_back(x)` does.

## Rules

- A list holds a `tracked_ptr` (the sentinel), so it lives where one may: on a stack or inside a managed object,
  never in `new`/`malloc` memory, a `std` container, a global or a plain coroutine frame
  ([The rules](../README.md#the-rules), 1).
- The list destroys an element the moment it is erased, popped, cleared, assigned over or the list is destroyed,
  exactly like `std::list`. The one exception is a list dying in a sweep, inside a managed object nobody refers to
  any more: its nodes are garbage of the same sweep, and each destroys its element when the sweep reaches it
  ([Containers](../README.md#containers)).
- An iterator to an erased element is invalid as in `std`: it keeps the node's memory mapped but not the element.
  A `tracked_ptr` may not address an element of a list ([The rules](../README.md#the-rules), 4): elements are reached
  through the list, its iterators, references and raw pointers.
- Iterators are raw node pointers, trivially copyable and at home in any container, a `std::vector` too: the list
  roots every linked node. Stepping and dereferencing are plain loads, with no write barrier.
- Relinking operations (`splice`, `merge`, `sort`, `reverse`) move nodes, never elements: every reference stays
  valid, and a throwing comparator leaves valid lists of the same elements.
- Thread safety is that of `std::list`: concurrent readers, or one writer, with the program's own
  synchronization. The collector never waits for a mutator and never touches a node the list still links.

## Template parameters

| Parameter | Description |
|---|---|
| `T` | The type of the elements: any object type that is not a reference or an array; an operation that copies or moves elements requires `T` to be copyable or movable. `list<unique_ptr<T>>` is a [specialization](#specializations). |

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
| `iterator` | a pointer to a node in a class of the library, `std::bidirectional_iterator` over `T` |
| `const_iterator` | the same over `const T`; an `iterator` converts to it |
| `reverse_iterator` | `std::reverse_iterator<iterator>` |
| `const_reverse_iterator` | `std::reverse_iterator<const_iterator>` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](list.md) | constructs the list |
| `(destructor)` | destroys the elements, unless the list dies in a sweep; the nodes and the sentinel are left to the collector |
| [operator=](operator_assign.md) | assigns values to the container |
| [assign](assign.md) | assigns values to the container, in the nodes it has |

#### Element access

| Function | Description |
|---|---|
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
| [empty](empty.md) | checks whether the list is empty |
| [size](size.md) | the number of elements |
| [max_size](max_size.md) | the largest number of elements a list may hold |

#### Modifiers

| Function | Description |
|---|---|
| [clear](clear.md) | destroys every element, keeps the sentinel |
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

#### Operations

| Function | Description |
|---|---|
| [merge](merge.md) | merges two sorted lists |
| [splice](splice.md) | moves nodes from another list, or within this one |
| [remove, remove_if](remove.md) | erases the elements equal to a value, or satisfying a predicate |
| [reverse](reverse.md) | reverses the order of the nodes |
| [unique](unique.md) | erases consecutive equal elements |
| [sort](sort.md) | sorts the nodes, stable |

#### From mixin::enumerable

The questions asked of the elements, carried by every container of the library
([mixin::enumerable](../mixin/enumerable/README.md)). A question that compares elements exists only for elements that
compare.

| Function | Description |
|---|---|
| [contains](../mixin/enumerable/contains.md) | checks whether an element is equal to a value |
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

The order of the elements ([mixin::ordered](../mixin/ordered/README.md)). The mixin's sorts are not a list's: `sort` is the
list's own, on the nodes, and `sort_by` and `stable_sort` need random access.

| Function | Description |
|---|---|
| `is_sorted` | checks whether the elements are sorted |
| `binary_search` | checks whether a sorted list holds a value |
| `lower_bound`, `upper_bound` | the first element not less than, greater than a value, in a sorted list |
| `sorted_index_of` | the position of a value in a sorted list |

#### From mixin::sequence

The writes over every element ([mixin::sequence](../mixin/sequence/README.md)); `reverse` is the list's own, on the nodes.

| Function | Description |
|---|---|
| `fill` | assigns a value to every element |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator\<=\>](operator_cmp.md) | compare the elements lexicographically |
| [swap](swap2.md) | swaps the contents of two lists |
| [erase, erase_if](erase_if.md) | erase every element equal to a value, or satisfying a predicate |

## Specializations

`list<unique_ptr<T>>` is a `std::list<unique_ptr<T>>` with the constructors of the base. A `unique_ptr` owns its
object and needs no tracing, so a list of them may live anywhere a `std::list` may, and the objects die when their
`unique_ptr` does, at the `pop_front` or the `erase` that removes it.

## Complexity

- Insertion or removal of an element anywhere, given an iterator: constant. A node is allocated per element
  ([Benchmarks: Containers](../benchmarks.md#containers)).
- Access to the first and the last element, `size`: constant. There is no access by position.
- `splice` of a whole list or of one element: constant; of a range from another list, linear in the range, whose
  nodes are counted.
- `merge`, `remove`, `remove_if`, `unique`, `reverse`: linear. `sort`: `N log N` comparisons.

## Iterator invalidation

| Operations | Invalidated |
|---|---|
| all read-only operations, `swap`, `splice`, `merge`, `sort`, `reverse` | never; an iterator to a moved node names its element in the list it is in now |
| `insert`, `emplace`, `push_back`, `emplace_back`, `push_front`, `emplace_front` | never |
| `erase`, `pop_back`, `pop_front`, `remove`, `remove_if`, `unique` | the erased elements |
| `resize`, `assign`, copy `operator=` | the erased elements, when the list shrinks |
| `clear`, move `operator=` | all the elements of this list |

One exception: the `end()` of a list that has no sentinel yet (one that never held an element, or one moved from)
is a null iterator, and the first operation that makes the sentinel invalidates it ([end](end.md)).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Item {
    int key;
    tracked_ptr<Item> twin;  // items may point at each other
};

struct Registry {
    list<tracked_ptr<Item>> items;  // inside a managed object: traced with it
};

int main() {
    println("values and a registry in lists");
    // counted after the first line: io's own objects are not the example's
    auto base = collector::get_live_object_count();

    // a list of values on the stack: the nodes are on the managed heap
    list numbers = {5, 3, 9, 1};
    numbers.push_front(7);
    numbers.sort();  // the nodes relinked in place
    numbers.remove_if([](int x) { return x % 2 == 0; });

    // a registry in a managed object; the items live in its list
    tracked_ptr r = make_tracked<Registry>();
    for (int i : range(1000)) {
        r->items.push_back(make_tracked<Item>(i));
    }
    // a cycle through the list: collected like any other
    r->items.front()->twin = r->items.back();
    r->items.back()->twin = r->items.front();

    // an iterator survives every other erasure: erase the odd keys around it
    auto kept = std::next(r->items.begin(), 500);
    // the unlinked nodes are the collector's
    for (auto it = r->items.begin(); it != r->items.end();) {
        it = (*it)->key % 2 ? r->items.erase(it) : std::next(it);
    }
    list<tracked_ptr<Item>> moved;
    moved.splice(moved.end(), r->items, kept);  // the node moves, the iterator still names it

    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    collector::force_collect(true);
    println("{} odd numbers {}", numbers.size(), numbers);
    println("{} items left in the registry, item {} moved out", r->items.size(), (*kept)->key);
    println("{} live objects", collector::get_live_object_count() - base);
}
```

Output:

```text
values and a registry in lists
5 odd numbers [1, 3, 5, 7, 9]
499 items left in the registry, item 500 moved out
1010 live objects
```

The lists `numbers` and `moved` die at the end of `main` and destroy their elements then; their nodes are freed by
the collector once nothing refers to them.

## See also

- [forward_list](../forward_list/README.md): a singly linked list
- [deque](../deque/README.md), [vector](../vector/README.md): random access
- [tracked_ptr](../tracked_ptr/README.md), [unique_ptr](../unique_ptr/README.md), [make_tracked](../make_tracked.md)
- [README: Containers](../README.md#containers), [README: The rules](../README.md#the-rules),
  [Stack roots](../../../garbage_collector/overview.md#stack-roots)
