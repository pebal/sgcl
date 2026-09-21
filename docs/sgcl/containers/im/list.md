# sgcl::im::list

```cpp
#include "sgcl/containers/im/list.h"   // or "sgcl/containers/im/im.h", "sgcl/sgcl.h"

namespace sgcl::im {
    template<class T>
    class list;
}
```

`sgcl::im::list<T>` is the immutable list — the list of Lisp, ML and Elm, what Go's `container/list` is not: a sequence read from the front, every change of which returns a new list and leaves the old one as it was. A chain of cells, each holding an element and the rest; `push_front` is one new cell in front of the old chain, which the new list shares with the old, so the old list stays a list of its own and the two share everything but the first cell: O(1), one allocation, whatever the length. `pop_front` is the rest of the chain, no allocation at all. What is not O(1) is not offered: no index, no `push_back`; `size()` is a word of the list, kept as cells are added and dropped. The tail of a list is a list; a list built from a range holds the elements in the range's order.

The list is two words: the count and a `tracked_ptr` to the first cell. The cells are managed objects that no version owns: a cell reached by ten lists is one cell, collected once the last of them is dropped. A list of a million cells is dropped in one sweep, not by a chain of destructors: the cells die together, in no particular order, which is what a linked list of a million nodes cannot do with reference counts (a recursion or a loop of frees) and what a collector does for free. This is the structure of a stack that keeps its history, of a path through a tree, of the "rest of the arguments"; for a sequence read by position, [im::vector](vector.md).

## Rules

- `sgcl::im::list` holds its first cell by `tracked_ptr`, so it lives where one may: on a thread's stack or inside a managed object ([The rules](../../core/README.md#the-rules), 1). Its iterators hold nothing alive: an iterator is valid while a list holding the chain exists.
- Every member is `const`. `push_front`, `emplace_front`, `pop_front` and `reverse` return the new list; the one they were called on is unchanged, and stays so for as long as it is held. The elements are reached as `const`.
- A change copies nothing but what it adds: `push_front` constructs one `T` in a new cell, `pop_front` copies nothing, `reverse` makes a new chain of every cell.
- A `T` holding tracked pointers is traced where it lives, in its cell; a `T` with a destructor is destroyed when the collector frees its cell, once no version reaches it. Nothing is destroyed by a `pop_front`: the old version still holds the element.
- Sharing between threads: any number of threads read any version; a `list` variable one thread replaces while others read it is published through [copy_on_write](../../concurrent/copy_on_write.md) or an [atomic](../../concurrent/atomic.md), an update costing one cell.
- `front` and `pop_front` on an empty list are undefined, as `front` and `pop_front` of `std::forward_list` are.

## Members

```cpp
using value_type = T;
using reference = const T&;
using const_reference = const T&;
using pointer = const T*;
using const_pointer = const T*;
using size_type = size_t;
using difference_type = ptrdiff_t;
using const_iterator = /* forward iterator over const T */;
using iterator = const_iterator;

list() noexcept;
template<std::input_iterator InputIt> list(InputIt first, InputIt last);   // the range's order: the cells made from the back
list(std::initializer_list<T> ilist);
template<std::ranges::input_range R> explicit list(R&& r);                // from a range of what the elements are made of
list(const list&) noexcept;  list(list&&) noexcept;  list& operator=(const list&) noexcept;  list& operator=(list&&) noexcept;

const_iterator begin() const noexcept;  const_iterator end() const noexcept;  cbegin, cend
size_type size() const noexcept;
bool empty() const noexcept;
const_reference front() const noexcept;

list push_front(const T& value) const;                // one cell, the rest shared
list push_front(T&& value) const;
template<class... A> list emplace_front(A&&... a) const;
list pop_front() const noexcept;                      // the rest of the chain
list reverse() const;                                 // a new chain of every cell

friend bool operator==(const list& a, const list& b);   // by the elements; a version and its copy by the chain
friend bool operator!=(const list& a, const list& b);
```

```cpp
im::list<int> l = {1, 2, 3};
auto m = l.push_front(0);          // 0 1 2 3: one cell, the chain of l behind it
auto n = l.pop_front();            // 2 3: the rest of l's chain, nothing made
assert(m.pop_front() == l);        // the same chain
for (int x : m) ...                // forward, from the front
auto r = m.reverse();              // 3 2 1 0: a new chain
```

### The mixins

`im::list` carries [mixin::immutable](../../core/mixin/immutable.md), [mixin::enumerable](../../core/mixin/enumerable.md), [mixin::comparable](../../core/mixin/comparable.md) and [mixin::ordered](../../core/mixin/ordered.md) ([the mixins](../../core/mixin/README.md)): `contains`, `index_of`, `min`, `is_sorted`; no `sort()`.

```cpp
im::list<int> l = {1, 2, 3};
assert(l.contains(2) && l.index_of(3) == 2 && l.is_sorted() && l.min() == 1);
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace sgcl;

// An undo history as a list of states: a change pushes the new state,
// undo pops it, and every state is shared with every history that
// holds it
struct Document {
    im::vector<string> lines;
};

struct Editor {
    im::list<Document> history = {Document{}};   // the current state in front

    const Document& current() const { return history.front(); }

    void append(string line) {
        history = history.push_front(Document{current().lines.push_back(std::move(line))});   // a cell and a leaf: the rest shared
    }

    bool undo() {
        if (history.size() == 1) return false;
        history = history.pop_front();
        return true;
    }
};

int main() {
    Editor e;
    e.append("first");
    e.append("second");
    e.append("third");
    auto snapshot = e.history;                          // every state, for as long as this is held
    e.undo();
    e.undo();
    std::cout << e.current().lines.size() << " line, " << snapshot.size() - 1 << " states in the snapshot\n";   // 1 line, 3 states
}
```

## Measured

On an Apple M-series core, `-O2`, `list<long>` of a million cells (`bench_im list`): `push_front` 13–16 ns a version each (`std::forward_list` in place: 11), `pop_front` 7 ns (8: a free each), a walk of the million 1.5 ns per cell (0.4) — the cells of a list built one version at a time lie in the pool in the order they were made, which is the order of the walk; the reference is a node freed per pop that the list never pays ([Benchmarks](benchmarks.md)).

## See also

- [im::vector](vector.md): the immutable sequence read by position; [im::map](map.md), [im::set](set.md)
- [copy_on_write](../../concurrent/copy_on_write.md), [atomic](../../concurrent/atomic.md): publishing a version
- [README: The structures](README.md#the-structures), [README: The rules](../../core/README.md#the-rules)
- `tests/containers/im/list.cpp`: versions, sharing and collection, tracked elements, a million cells dropped at once, the stress test against `std::list`, readers under `copy_on_write`.
