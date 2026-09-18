# Sgcl::WeakPtr

```cpp
#include "sgcl/Sgcl/Core/Ptr.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class T>
    class WeakPtr;
}
```

The same class in the `sgcl` interface: [weak_ptr](../../core/weak_ptr.md).

`WeakPtr<T>` is a pointer that keeps nothing alive: the object lives as long as something else reaches it through [`Ptr`](Ptr.md)s or a [`UniquePtr`](UniquePtr.md), and `Lock()` says whether it still does. `Lock()` is the object as a `Ptr` while it is reachable, and null once a cycle has found it unreachable; `IsExpired()` is the same question without the pointer. It is what `std::weak_ptr` is to `std::shared_ptr`, without the counts: a back pointer, a cache entry, an observer that must not extend a lifetime.

It is one word: a `Ptr` to a small cell on the managed heap that holds the target as a word the collector clears instead of tracing. A `WeakPtr` made from a strong pointer allocates a cell of its own (16 bytes); copies share it, and the cell is collected with the last copy. That word is a `Ptr`, so a `WeakPtr` lives where one may. The clearing is a phase of the cycle, after the marking and before the sweep: `Lock()` never hands out an object the sweep will destroy or the slot it will be reused for, and a `Lock()` that races with the clearing either sees the null or wins, holding the object for at least one more cycle ([Weak pointers](../../core/README.md#weak-pointers)). Between the object becoming unreachable and the cycle that notices, `Lock()` still returns it: the lag of any garbage collector.

## Rules

- A `WeakPtr<T>` is a `Ptr` (to a cell), so it lives where one may: on a stack or inside a managed object, never in `new`/`malloc` memory, a `std` container, a global or a plain coroutine frame ([The rules](../../core/README.md#the-rules), 1 and 4).
- It addresses an object no `UniquePtr` owns: a `Ptr` cannot address one either, and the owner's delete would leave the cell dangling. Debug builds assert it.
- Threads share a `WeakPtr` the way they share a `Ptr`: one written by one thread and read by another needs the program's own synchronization ([The rules](../../core/README.md#the-rules), 6). `Lock()` itself is safe against the collector clearing the cell at the same time.
- In a destructor, a `WeakPtr` member is a `Ptr` member: the cell may be dying in the same sweep, so it is not to be read there ([The rules](../../core/README.md#the-rules), 5).
- A `WeakPtr` in a cycle does not keep it: two objects pointing at each other through a `Ptr` and a `WeakPtr` are collected when nothing else reaches them.

## Members

### ElementType, InnerType

```cpp
using ElementType = T;
using InnerType = sgcl::weak_ptr<T>;
```

### Constructors

```cpp
WeakPtr() noexcept;
WeakPtr(std::nullptr_t) noexcept;
template<class U> requires std::is_convertible_v<U*, T*> WeakPtr(const Ptr<U>& p);
WeakPtr(const WeakPtr&) noexcept;
WeakPtr(WeakPtr&&) noexcept;
template<class U> requires std::is_convertible_v<U*, T*> WeakPtr(const WeakPtr<U>& w) noexcept;
explicit WeakPtr(InnerType w) noexcept;
```

The default and the `nullptr` constructors make an empty `WeakPtr`, with no cell: expired. The constructors from a `Ptr<U>` (with `U*` convertible to `T*`) allocate a cell holding the object, which is why they are not `noexcept`; from a null pointer they make an empty one. Copies, from a `WeakPtr` to `T` or to a derived class, share the cell; a move is a copy of the word, the source keeps its cell.

```cpp
struct Base { virtual ~Base() = default; };
struct Item : Base { int value = 7; };

Ptr item = Make<Item>();
WeakPtr weak = item;                // WeakPtr<Item>, a cell of its own
WeakPtr<Base> base = weak;          // the same cell, seen as the base
WeakPtr<Base> fromItem = item;      // another cell
WeakPtr<Item> none;                 // no cell: expired
assert(weak.Lock() == item && base.Lock() == item && none.IsExpired());
```

### operator=

```cpp
WeakPtr& operator=(const WeakPtr&) noexcept;
WeakPtr& operator=(WeakPtr&&) noexcept;
template<class U> requires std::is_convertible_v<U*, T*> WeakPtr& operator=(const WeakPtr<U>& w) noexcept;
template<class U> requires std::is_convertible_v<U*, T*> WeakPtr& operator=(const Ptr<U>& p);
WeakPtr& operator=(std::nullptr_t) noexcept;
```

Assigning a `WeakPtr` shares its cell; assigning a `Ptr` allocates a new cell (the copies that shared the old one keep it); assigning `nullptr` drops the cell.

```cpp
Ptr a = Make<int>(1);
Ptr b = Make<int>(2);
WeakPtr w = a;
WeakPtr copy = w;       // shares the cell of a
w = b;                           // a new cell: the copy still sees a
assert(*w.Lock() == 2 && *copy.Lock() == 1);
w = nullptr;
assert(w.IsExpired() && *copy.Lock() == 1);
```

### Lock

```cpp
Ptr<T> Lock() const noexcept;
```

The object as a `Ptr`, held from then on, or null when the cell has been cleared or there is none. The read is the cell twice around a hazard pointer, published before the second read: the collector clears a cell before it reads the hazards, so a `Lock()` that races with the clearing either sees the null or is seen, and its object is marked and survives the cycle. Never a dangling pointer, never a destroyed object.

```cpp
struct Item { int value = 1; };
Ptr item = Make<Item>();
WeakPtr cached = item;
if (auto p = cached.Lock()) {   // Ptr<Item>: the object, held by p
    p->value = 2;
}
```

### IsExpired

```cpp
bool IsExpired() const noexcept;
```

True when the cell has been cleared or there is none: `Lock()` would return null. The other way round is not guaranteed: an object found unreachable stays in the cell until the cycle clears it, so `IsExpired()` may be false for an object nothing reaches any more. For a decision that needs the object, `Lock()` and test the result.

```cpp
WeakPtr<int> weak;
{
    Ptr number = Make<int>(1);
    weak = number;
    assert(!weak.IsExpired());
}
Collector::Collect(true);     // optional, for the demonstration only: the next cycle clears it anyway
assert(weak.IsExpired() && !weak.Lock());
```

### Reset

```cpp
void Reset() noexcept;
```

Drops the cell: the `WeakPtr` is empty afterwards, expired. Copies that share the cell keep it.

### Swap

```cpp
void Swap(WeakPtr& w) noexcept;
template<class T> void swap(WeakPtr<T>& l, WeakPtr<T>& r) noexcept;   // free function
```

Exchanges the cells of the two pointers.

```cpp
Ptr a = Make<int>(1);
WeakPtr w = a;
WeakPtr<int> empty;
swap(w, empty);
assert(w.IsExpired() && *empty.Lock() == 1);
```

### Inner

```cpp
InnerType& Inner() noexcept;
const InnerType& Inner() const noexcept;
```

The word inside, as its own type.

### Deduction guides

```cpp
template<class T> WeakPtr(const Ptr<T>&) -> WeakPtr<T>;
```

`WeakPtr w = item` is a `WeakPtr<T>` for a `Ptr<T> item`. The explicit arguments are needed for a base class, an empty `WeakPtr` or a member declaration.

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <cassert>
#include <iostream>

// A tree owned downward, with back pointers that keep nothing: dropping a
// parent never keeps it alive through its children, and a subtree held on
// its own outlives its parent with an expired back pointer.
struct Node {
    explicit Node(int id) : id(id) {}
    int id;
    List<Ptr<Node>> children;        // owns the subtrees
    WeakPtr<Node> parent;                     // a back pointer, no cycle
};

Ptr<Node> AddChild(const Ptr<Node>& parent, int id) {
    Ptr child = Make<Node>(id);
    child->parent = parent;                 // a cell of its own
    parent->children.Add(child);
    return child;
}

// The path to the root, in a frame of its own: the copies it makes are
// stack words, and the stack is scanned conservatively
void PrintPath(Ptr<Node> node) {
    for (auto n = node; n; n = n->parent.Lock()) {   // Lock(): the parent while it lives
        std::cout << n->id << ' ';
    }
    std::cout << '\n';
}

int main() {
    Ptr root = Make<Node>(0);
    Ptr branch = AddChild(root, 1);
    Ptr leaf = AddChild(branch, 2);
    PrintPath(leaf);                            // 2 1 0

    // The root dropped, the branch held: the branch's back pointer expires,
    // the leaf's still locks, since the branch owns the leaf
    root = nullptr;
    Collector::Collect(true);          // optional, for the demonstration only: the next cycle clears it anyway
    assert(branch->parent.IsExpired());
    assert(leaf->parent.Lock() == branch);
    PrintPath(leaf);                            // 2 1
    return 0;
}
```

The output:

```
2 1 0 
2 1 
```

## See also

- [Ptr](Ptr.md), [UniquePtr](UniquePtr.md), [Make](Make.md)
- [WeakDictionary](../Containers/WeakDictionary.md), [WeakHashSet](../Containers/WeakHashSet.md): containers keyed by objects they do not keep alive
- [ExpiryQueue](../Containers/ExpiryQueue.md): a `WeakPtr` plus a function called with the object, alive one last time, when it is found unreachable
- [Collector](Collector.md) for `Collect`
- README: [Weak pointers](../../core/README.md#weak-pointers), [The classes](../../core/README.md#the-classes), [The rules](../../core/README.md#the-rules)
