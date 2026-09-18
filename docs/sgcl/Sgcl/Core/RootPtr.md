# Sgcl::RootPtr

```cpp
#include "sgcl/Sgcl/Core/Ptr.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class T>
    class RootPtr;
}
```

The same class in the `sgcl` interface: [root_ptr](../../core/root_ptr.md).

`RootPtr<T>` is a root that lives anywhere: a pointer to a managed object held from unmanaged memory (a global, a `std::vector`, a handle table, a lambda on the heap, the frame of a plain coroutine), the object reachable for as long as the `RootPtr` exists. Under it is a cell: one word of a managed block of a cache line of them, taken from the thread's cell allocator in the constructor and given back by the destructor, and this `RootPtr`'s for the whole time between. A block is a root by state, traced by the collector like any object, and freed by the cycle that finds every cell of it given back once its allocator has moved on ([how it works: the cells](../../../garbage_collector/how-it-works.md#the-cells-of-the-root_ptrs)). The cell is a `Ptr` inside a managed object, so a `RootPtr` is a `Ptr` held one step away: `GetPtr()` is that `Ptr`, by reference, every read is its read and every store its store, with its barrier, and an [`AtomicRef`](../Concurrent/AtomicRef.md) over it is the atomic of the root.

`RootPtr` says in its type what it is, with no mode and no test: for code that knows it stands outside the managed heap and wants a root there, the way an interpreter keeps its handles or a program its globals. What it costs: a cell per `RootPtr`, one managed allocation per block of them (a null takes one too: the `RootPtr` may be assigned to later), one indirection per access, a cell of its own per copy. No store ever allocates, so two threads storing into the same `RootPtr` race on one atomic word, as they do on a `Ptr`, and never on the making of a cell; no move ever takes a cell from another `RootPtr`, so a thread reading through the cell of a `RootPtr` another thread moves from reads a cell that lives as long as its `RootPtr`. The family, then: `UniquePtr` owns deterministically, `Ptr` lives on a stack or in a managed object, `ToShared()` is a shared root, `RootPtr` a root of its own anywhere.

## Rules

- A `RootPtr` lives anywhere, a managed object included (where it is pointless: a `Ptr` does the same for a word).
- The object it points at is reachable while the `RootPtr` exists; dropping the last root and every other reference lets the next cycle collect it. Destroying a `RootPtr` gives its cell back at once; the block goes with the cycle that finds every cell of it free.
- Threads share a `RootPtr` the way they share a `Ptr`: one written by one thread and read by another needs the program's own synchronization, or an `AtomicRef` over it ([The rules](../../core/README.md#the-rules), 6). A `RootPtr` may be destroyed on any thread, not only the one that made it.
- It addresses an object no `UniquePtr` owns, as a `Ptr` does ([The rules](../../core/README.md#the-rules), 4).
- The constructor registers the thread with the collector, as a `Ptr`'s does.

## Members

```cpp
using ElementType = T;
using InnerType = sgcl::root_ptr<T>;

RootPtr() noexcept;                                          // empty: a cell, null inside
RootPtr(std::nullptr_t) noexcept;
template<class U> RootPtr(const Ptr<U>& p) noexcept;         // U* convertible to T*
template<class U> RootPtr(UniquePtr<U>&& u) noexcept;        // RootPtr<T> r = Make<T>(...)
RootPtr(const RootPtr&) noexcept;                            // a cell of its own
RootPtr(RootPtr&&) noexcept;                                 // the pointer moves; the source is null, its cell stays
explicit RootPtr(InnerType p) noexcept;
RootPtr& operator=(...) noexcept;                            // the same set, and nullptr
~RootPtr() noexcept;                                         // the cell given back

T* Get() const noexcept;
T& operator*() const noexcept;
T* operator->() const noexcept;
explicit operator bool() const noexcept;
Ptr<T>& GetPtr() noexcept;                                   // the Ptr the root holds its object by: the cell's word
const Ptr<T>& GetPtr() const noexcept;
operator Ptr<T>&() noexcept;
operator const Ptr<T>&() const noexcept;
void Reset() noexcept;
template<class U> void Reset(const Ptr<U>& p) noexcept;
void Swap(RootPtr&) noexcept;                                // the pointers exchanged, the cells stay
const std::type_info& Type() const noexcept;                 // as Ptr: the dynamic type, Is<U>(), As<U>()
template<class U> bool Is() const noexcept;
template<class U> Ptr<U> As() const noexcept;
InnerType& Inner() noexcept;
const InnerType& Inner() const noexcept;
```

The free functions, in `Sgcl`: `swap`, `==` with a `RootPtr`, a `Ptr` and `nullptr`, `operator<<`, `std::hash<RootPtr<T>>` (of the address), the deduction guides from a `Ptr` and a `UniquePtr`.

```cpp
struct Node { int value; Ptr<Node> next; };

std::vector<RootPtr<Node>> handles;            // roots on the unmanaged heap
handles.emplace_back(Make<Node>(Node{1}));     // a cell taken, the object rooted
Ptr<Node> n = handles[0].GetPtr();             // the Ptr, copied onto the stack
n->next = Make<Node>(Node{2});                 // reachable through the root
handles.clear();                                        // the roots gone: both nodes collectable

static RootPtr<Node> current;                  // a global shared between threads
AtomicRef a(current);                          // the atomic of the root: the cell's word
a.Store(Make<Node>(Node{3}));
Ptr<Node> seen = a.Load();
```

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>
#include <string>
#include <unordered_map>

// An interpreter's globals: named roots into the managed heap, kept in
// a std::unordered_map that lives where the interpreter does. A value
// reachable from a global stays; one dropped from the table goes with
// the next cycle, with everything only it reached.
struct Value {
    int number;
    Ptr<Value> next;
};

int main() {
    std::unordered_map<std::string, RootPtr<Value>> globals;
    globals["list"] = Make<Value>(Value{1});
    globals["list"]->next = Make<Value>(Value{2});
    globals["alias"] = globals["list"];                        // a cell of its own, the same object
    std::cout << globals["alias"]->next->number << "\n";       // 2
    globals.erase("list");                                     // the alias still roots the list
    Collector::Collect(true);                         // optional, for the demonstration only
    std::cout << globals["alias"]->number << "\n";             // 1
    globals.clear();                                           // no root left: the list is collectable
    return 0;
}
```

The output:

```
2
1
```

## See also

- [Ptr](Ptr.md), [UniquePtr](UniquePtr.md), [Make](Make.md), [AtomicRef](../Concurrent/AtomicRef.md)
- README: [Stack roots](../../../garbage_collector/overview.md#stack-roots), [The rules](../../core/README.md#the-rules)
