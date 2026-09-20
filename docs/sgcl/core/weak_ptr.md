# sgcl::weak_ptr

```cpp
#include "sgcl/core/weak_ptr.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class T>
    class weak_ptr;
}
```

`weak_ptr<T>` is a pointer that keeps nothing alive: the object lives as long as something else reaches it through [`tracked_ptr`](tracked_ptr.md)s or a [`unique_ptr`](unique_ptr.md), and `lock()` says whether it still does. `lock()` is the object as a `tracked_ptr` while it is reachable, and null once a cycle has found it unreachable; `expired()` is the same question without the pointer. It is what `std::weak_ptr` is to `std::shared_ptr`, without the counts: a back pointer, a cache entry, an observer that must not extend a lifetime.

It is one word: a `tracked_ptr` to a small cell on the managed heap that holds the target as a word the collector clears instead of tracing. A `weak_ptr` made from a strong pointer allocates a cell of its own (16 bytes); copies share it, and the cell is collected with the last copy. That word is a `tracked_ptr`, so a `weak_ptr` lives where one may. The clearing is a phase of the cycle, after the marking and before the sweep: `lock()` never hands out an object the sweep will destroy or the slot it will be reused for, and a `lock()` that races with the clearing either sees the null or wins, holding the object for at least one more cycle ([Weak pointers](README.md#weak-pointers)). Between the object becoming unreachable and the cycle that notices, `lock()` still returns it: the lag of any garbage collector.

## Rules

- A `weak_ptr<T>` is a `tracked_ptr` (to a cell), so it lives where one may: on a stack or inside a managed object, never in `new`/`malloc` memory, a `std` container, a global or a plain coroutine frame ([The rules](README.md#the-rules), 1 and 4).
- It addresses an object no `unique_ptr` owns: a `tracked_ptr` cannot address one either, and the owner's delete would leave the cell dangling. Debug builds assert it.
- Threads share a `weak_ptr` the way they share a `tracked_ptr`: one written by one thread and read by another needs the program's own synchronization ([The rules](README.md#the-rules), 6). `lock()` itself is safe against the collector clearing the cell at the same time.
- In a destructor, a `weak_ptr` member is a `tracked_ptr` member: the cell may be dying in the same sweep, so it is not to be read there ([The rules](README.md#the-rules), 5).
- A `weak_ptr` in a cycle does not keep it: two objects pointing at each other through a `tracked_ptr` and a `weak_ptr` are collected when nothing else reaches them.

## Members

### element_type

```cpp
using element_type = typename tracked_ptr<T>::element_type;   // T
```

### Constructors

```cpp
constexpr weak_ptr() noexcept = default;
constexpr weak_ptr(std::nullptr_t) noexcept;

template<class U, std::enable_if_t<std::is_convertible_v<typename tracked_ptr<U>::element_type*, element_type*>, int> = 0>
weak_ptr(const tracked_ptr<U>& p);
template<class U, std::enable_if_t<std::is_convertible_v<typename sgcl::tracked_ptr<U>::element_type*, element_type*>, int> = 0>
weak_ptr(const sgcl::tracked_ptr<U>& p);

weak_ptr(const weak_ptr&) noexcept = default;
weak_ptr(weak_ptr&&) noexcept = default;
template<class U, template<class> class P, std::enable_if_t<std::is_convertible_v<typename weak_ptr<U, P>::element_type*, element_type*>, int> = 0>
weak_ptr(const weak_ptr<U, P>& w) noexcept;
```

The default and the `nullptr` constructors make an empty `weak_ptr`, with no cell: expired. The constructors from a `tracked_ptr<U>` (with `U*` convertible to `T*`) allocate a cell holding the object, which is why they are not `noexcept`; from a null pointer they make an empty one. Copies, from a `weak_ptr` to `T` or to a derived class, share the cell; a move is a copy of the word, the source keeps its cell.

```cpp
struct Base { virtual ~Base() = default; };
struct Item : Base { int value = 7; };

sgcl::tracked_ptr item = sgcl::make_tracked<Item>();
sgcl::weak_ptr weak = item;                // weak_ptr<Item>, a cell of its own
sgcl::weak_ptr<Base> base = weak;          // the same cell, seen as the base
sgcl::weak_ptr<Base> from_item = item;     // another cell
sgcl::weak_ptr<Item> none;                 // no cell: expired
assert(weak.lock() == item && base.lock() == item && none.expired());
```

### operator=

```cpp
weak_ptr& operator=(const weak_ptr&) noexcept = default;
weak_ptr& operator=(weak_ptr&&) noexcept = default;
template<class U, std::enable_if_t<std::is_convertible_v<typename weak_ptr<U>::element_type*, element_type*>, int> = 0>
weak_ptr& operator=(const weak_ptr<U>& w) noexcept;
template<class U, std::enable_if_t<std::is_convertible_v<typename tracked_ptr<U>::element_type*, element_type*>, int> = 0>
weak_ptr& operator=(const tracked_ptr<U>& p);
weak_ptr& operator=(std::nullptr_t) noexcept;
```

Assigning a `weak_ptr` shares its cell; assigning a `tracked_ptr` allocates a new cell (the copies that shared the old one keep it); assigning `nullptr` drops the cell.

```cpp
sgcl::tracked_ptr a = sgcl::make_tracked<int>(1);
sgcl::tracked_ptr b = sgcl::make_tracked<int>(2);
sgcl::weak_ptr w = a;
sgcl::weak_ptr copy = w;       // shares the cell of a
w = b;                       // a new cell: the copy still sees a
assert(*w.lock() == 2 && *copy.lock() == 1);
w = nullptr;
assert(w.expired() && *copy.lock() == 1);
```

### lock

```cpp
tracked_ptr<T> lock() const noexcept;
```

The object as a `tracked_ptr`, held from then on, or null when the cell has been cleared or there is none. The read is the cell twice around a hazard pointer, published before the second read: the collector clears a cell before it reads the hazards, so a `lock()` that races with the clearing either sees the null or is seen, and its object is marked and survives the cycle. Never a dangling pointer, never a destroyed object.

```cpp
struct Item { int value = 1; };
sgcl::tracked_ptr item = sgcl::make_tracked<Item>();
sgcl::weak_ptr cached = item;
if (auto p = cached.lock()) {   // tracked_ptr<Item>: the object, held by p
    p->value = 2;
}
```

### expired

```cpp
bool expired() const noexcept;
```

True when the cell has been cleared or there is none: `lock()` would return null. The other way round is not guaranteed: an object found unreachable stays in the cell until the cycle clears it, so `expired()` may be false for an object nothing reaches any more. For a decision that needs the object, `lock()` and test the result.

```cpp
sgcl::weak_ptr<int> weak;
{
    sgcl::tracked_ptr number = sgcl::make_tracked<int>(1);
    weak = number;
    assert(!weak.expired());
}
sgcl::collector::force_collect(true);     // optional, for the demonstration only: the next cycle clears it anyway
assert(weak.expired() && !weak.lock());
```

### reset

```cpp
void reset() noexcept;
```

Drops the cell: the `weak_ptr` is empty afterwards, expired. Copies that share the cell keep it.

### swap

```cpp
void swap(weak_ptr& w) noexcept;
template<class T> void swap(weak_ptr<T>& l, weak_ptr<T>& r) noexcept;   // free function
```

Exchanges the cells of the two pointers.

```cpp
sgcl::tracked_ptr a = sgcl::make_tracked<int>(1);
sgcl::weak_ptr w = a;
sgcl::weak_ptr<int> empty;
swap(w, empty);
assert(w.expired() && *empty.lock() == 1);
```

### Deduction guides

```cpp
template<class T> weak_ptr(const tracked_ptr<T>&) -> weak_ptr<T>;
template<class T> weak_ptr(const sgcl::tracked_ptr<T>&) -> weak_ptr<T, sgcl::tracked_ptr>;
```

`sgcl::weak_ptr w = item` is a `weak_ptr<T>` for a `tracked_ptr<T> item`. The explicit arguments are needed for a base class, an empty `weak_ptr` or a member declaration.

## Example

```cpp
#include "sgcl/sgcl.h"
#include <cassert>
#include <iostream>

// A tree owned downward, with back pointers that keep nothing: dropping a
// parent never keeps it alive through its children, and a subtree held on
// its own outlives its parent with an expired back pointer.
struct Node {
    explicit Node(int id) : id(id) {}
    int id;
    sgcl::vector<sgcl::tracked_ptr<Node>> children;       // owns the subtrees
    sgcl::weak_ptr<Node> parent;                         // a back pointer, no cycle
};

sgcl::tracked_ptr<Node> add_child(const sgcl::tracked_ptr<Node>& parent, int id) {
    sgcl::tracked_ptr child = sgcl::make_tracked<Node>(id);
    child->parent = parent;                 // a cell of its own
    parent->children.push_back(child);
    return child;
}

// The path to the root, in a frame of its own: the copies it makes are
// stack words, and the stack is scanned conservatively
void print_path(sgcl::tracked_ptr<Node> node) {
    for (auto n = node; n; n = n->parent.lock()) {   // lock(): the parent while it lives
        std::cout << n->id << ' ';
    }
    std::cout << '\n';
}

int main() {
    sgcl::tracked_ptr root = sgcl::make_tracked<Node>(0);
    sgcl::tracked_ptr branch = add_child(root, 1);
    sgcl::tracked_ptr leaf = add_child(branch, 2);
    print_path(leaf);                           // 2 1 0

    // The root dropped, the branch held: the branch's back pointer expires,
    // the leaf's still locks, since the branch owns the leaf
    root = nullptr;
    sgcl::collector::force_collect(true);         // optional, for the demonstration only: the next cycle clears it anyway
    assert(branch->parent.expired());
    assert(leaf->parent.lock() == branch);
    print_path(leaf);                           // 2 1
    return 0;
}
```

The output:

```
2 1 0 
2 1 
```

## See also

- [tracked_ptr](tracked_ptr.md), [unique_ptr](unique_ptr.md), [make_tracked](make_tracked.md)
- [weak_map](../containers/weak_map.md), [weak_set](../containers/weak_set.md): containers keyed by objects they do not keep alive
- [expiry_queue](../containers/expiry_queue.md): a `weak_ptr` plus a function called with the object, alive one last time, when it is found unreachable
- [collector](collector.md) for `force_collect`
- README: [Weak pointers](README.md#weak-pointers), [The classes](README.md#the-classes), [The rules](README.md#the-rules)
