[sgcl](../../README.md) › [immutable](../README.md)

# sgcl::immutable::list\<T\>

```cpp
#include "sgcl/immutable/list.h"   // or "sgcl/immutable.h"

namespace sgcl::immutable {
    template<class T>
    class list;
}
```

`sgcl::immutable::list<T>` is the immutable list, the list of Lisp, ML and Elm: a sequence read from the front,
every change of which returns a new list and leaves the old one as it was. It is a chain of cells, each holding
an element and the rest. `push_front` is one new cell in front of the old chain, which the new list shares with
the old, so the old list stays a list of its own and the two share everything but the first cell: O(1), one
allocation, whatever the length. `pop_front` is the rest of the chain, with nothing allocated at all. What is not
O(1) is not offered: no index, no `push_back`; `size()` is a word of the list, kept as cells are added and
dropped. The tail of a list is a list; a list built from a range holds the elements in the range's order.

The list is two words: the count and a `tracked_ptr` to the first cell. The cells are managed objects that no
version owns: a cell reached by ten lists is one cell, collected once the last of them is dropped. A list of a
million cells is dropped in one sweep, not by a chain of destructors: the cells die together, in no particular
order, which is what a linked list of a million nodes cannot do with reference counts (a recursion or a loop of
frees) and what a collector does for free.

What differs from `std::forward_list` and from Go's `container/list`: those are mutable chains changed in place,
this one is a value. It is the structure of a stack that keeps its history, of a path through a tree, of "the
rest of the arguments"; for a sequence read by position, [vector](../vector/README.md).

## Rules

- A list holds its first cell by `tracked_ptr`, so it lives where one may: on a thread's stack or inside a managed
  object ([The rules](../../core/README.md#the-rules), 1).
- Every member but the assignment is `const`. `push_front`, `emplace_front`, `pop_front` and `reverse` return
  the new list; the one they were called on is unchanged, and stays so for as long as it is held. The elements
  are reached as `const`.
- A change copies nothing but what it adds: `push_front` constructs one `T` in a new cell, `pop_front` copies
  nothing, `reverse` makes a new chain of every cell.
- A `T` holding tracked pointers is traced where it lives, in its cell; a `T` with a destructor is destroyed when
  the collector frees its cell, once no version reaches it. Nothing is destroyed by a `pop_front`: the old version
  still holds the element.
- An iterator holds nothing alive: it is a pointer to a cell, valid while some list holds the chain through that
  cell.
- Sharing between threads: any number of threads read any version; a list variable one thread replaces while
  others read it is published through [concurrent::copy_on_write](../../concurrent/copy_on_write/README.md) or an
  [atomic](../../core/atomic.md), an update costing one cell.
- `front` and `pop_front` on an empty list are undefined, as `front` and `pop_front` of `std::forward_list` are.

## Template parameters

| Parameter | Description |
|---|---|
| `T` | The type of the elements: an object type that is not a reference or an array. `reverse` copies the elements and requires `T` to be copy constructible; `push_front` of an rvalue, `emplace_front` and the constructor from a range of rvalues construct an element from what they are given, so a move-only `T` is held as well. |

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
| `const_iterator` | a forward iterator over `const T`, `std::forward_iterator` |
| `iterator` | `const_iterator` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](list.md) | constructs the list |
| `(destructor)` | drops this version; the cells no other version reaches are left to the collector |
| [operator=](operator_assign.md) | makes the variable hold another version |

#### Element access

| Function | Description |
|---|---|
| [front](front.md) | access the first element |

#### Iterators

| Function | Description |
|---|---|
| [begin, cbegin](begin.md) | an iterator to the beginning |
| [end, cend](end.md) | an iterator to the end |

#### Capacity

| Function | Description |
|---|---|
| [empty](empty.md) | checks whether the list is empty |
| [size](size.md) | the number of elements |

#### New versions

| Function | Description |
|---|---|
| [push_front](push_front.md) | the list with one more element in front |
| [emplace_front](emplace_front.md) | the same, the element constructed from arguments |
| [pop_front](pop_front.md) | the list without its first element |
| [reverse](reverse.md) | the list of the elements in the reverse order |

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
| `binary_search` | checks whether a sorted list holds a value, in linear steps |
| `lower_bound`, `upper_bound` | the first element not less than, greater than a value, in a sorted list |
| `sorted_index_of` | the position of a value in a sorted list |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator!=](operator_cmp.md) | compare the elements |
| `operator<=>` | compares the elements lexicographically ([mixin::comparable](../../core/mixin/comparable/README.md)) |

## Deduction guides

```cpp
template<std::input_iterator It>
list(It, It) -> list<std::iter_value_t<It>>;
```

## Complexity

- `front`, `push_front`, `emplace_front`, `pop_front`, `size`: constant.
- `reverse`, a walk, the comparison: linear in the size.
- Copy and assignment: constant, two words.

## Iterator invalidation

| Operations | Invalidated |
|---|---|
| every member but `operator=` | never: nothing changes the list |
| `operator=` | those to the cells that no list holds any more |

An iterator addresses a cell, not the list it came from: after `l = l.pop_front()` an iterator to the second
cell is still valid, since `l` holds it; an iterator to the first is valid while another list holds that cell.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

// An undo history as a list of states: a change pushes the new state, undo
// pops it, and every state is shared with every history that holds it
struct Document {
    immutable::vector<string> lines;
};

struct Editor {
    immutable::list<Document> history = {Document{}};  // the current state in front

    const Document& current() const {
        return history.front();
    }

    void append(string line) {
        // a cell and a leaf: the rest shared
        history = history.push_front(Document{current().lines.push_back(std::move(line))});
    }

    bool undo() {
        if (history.size() == 1) {
            return false;
        }
        history = history.pop_front();
        return true;
    }
};

int main() {
    Editor e;
    e.append("first");
    e.append("second");
    e.append("third");
    auto snapshot = e.history;  // every state, for as long as this is held
    e.undo();
    e.undo();
    println("{}, {} states in the snapshot", e.current().lines, snapshot.size());
}
```

Output:

```text
["first"], 4 states in the snapshot
```

## See also

- [vector](../vector/README.md): the immutable sequence read by position
- [map](../map/README.md), [set](../set/README.md): the immutable hash map and set
- [concurrent::copy_on_write](../../concurrent/copy_on_write/README.md), [atomic](../../core/atomic.md): publishing a version
- [list](../../core/list/README.md), [forward_list](../../core/forward_list/README.md): the mutable ones
- [README: The rules](../README.md#the-rules), [Benchmarks](../benchmarks.md)
