# sgcl::root_ptr

```cpp
#include "sgcl/root_ptr.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class T>
    class root_ptr;
}
```

`root_ptr<T>` is a root that lives anywhere: a pointer to a managed object held from unmanaged memory (a global, a `std::vector`, a handle table, a lambda on the heap, the frame of a plain coroutine), the object reachable for as long as the `root_ptr` exists. Under it is a cell: one word of a managed block of a cache line of them, taken from the thread's cell allocator in the constructor and given back by the destructor, and this `root_ptr`'s for the whole time between. A block is a root by state, traced by the collector like any object, and freed by the cycle that finds every cell of it given back once its allocator has moved on ([how it works: the cells](how-it-works.md#the-cells-of-the-root_ptrs)). The cell is a `tracked_ptr` inside a managed object, so a `root_ptr` is a `tracked_ptr` held one step away: `ptr()` is that `tracked_ptr`, by reference, every read is its read and every store its store, with its barrier, and an [`atomic_ref`](atomic_ref.md) over it is the atomic of the root.

`root_ptr` says in its type what it is, with no mode and no test: for code that knows it stands outside the managed heap and wants a root there, the way an interpreter keeps its handles or a program its globals. What it costs: a cell per `root_ptr`, one managed allocation per block of them (a null takes one too: the `root_ptr` may be assigned to later), one indirection per access, a cell of its own per copy. No store ever allocates, so two threads storing into the same `root_ptr` race on one atomic word, as they do on a `tracked_ptr`, and never on the making of a cell; no move ever takes a cell from another `root_ptr`, so a thread reading through the cell of a `root_ptr` another thread moves from reads a cell that lives as long as its `root_ptr`. The family, then: `unique_ptr` owns deterministically, `tracked_ptr` lives on a stack or in a managed object, `to_shared()` is a shared root, `root_ptr` a root of its own anywhere.

## Rules

- A `root_ptr` lives anywhere, a managed object included (where it is pointless: a `tracked_ptr` does the same for a word).
- The object it points at is reachable while the `root_ptr` exists; dropping the last root and every other reference lets the next cycle collect it. Destroying a `root_ptr` gives its cell back at once; the block goes with the cycle that finds every cell of it free.
- Threads share a `root_ptr` the way they share a `tracked_ptr`: one written by one thread and read by another needs the program's own synchronization, or an `atomic_ref` over it ([The rules](../README.md#the-rules), 6). A `root_ptr` may be destroyed on any thread, not only the one that made it.
- It addresses an object no `unique_ptr` owns, as a `tracked_ptr` does ([The rules](../README.md#the-rules), 4).
- The constructor registers the thread with the collector, as a `tracked_ptr`'s does.

## Members

```cpp
using element_type = T;

root_ptr() noexcept;                                            // empty: a cell, null inside
root_ptr(std::nullptr_t) noexcept;
template<class U> root_ptr(const tracked_ptr<U>& p) noexcept;   // U* convertible to T*
template<class U> root_ptr(unique_ptr<U>&& u) noexcept;         // root_ptr<T> r = make_tracked<T>(...)
root_ptr(const root_ptr&) noexcept;                             // a cell of its own
template<class U> root_ptr(const root_ptr<U>&) noexcept;
root_ptr(root_ptr&&) noexcept;                                  // the pointer moves; the source is null, its cell stays
root_ptr& operator=(...) noexcept;                              // the same set, and nullptr
~root_ptr() noexcept;                                           // the cell given back

element_type* get() const noexcept;
element_type& operator*() const noexcept;
element_type* operator->() const noexcept;
explicit operator bool() const noexcept;
tracked_ptr<T>& ptr() noexcept;                                 // the tracked_ptr the root holds its object by: the cell's word
const tracked_ptr<T>& ptr() const noexcept;
operator tracked_ptr<T>&() noexcept;
operator const tracked_ptr<T>&() const noexcept;
void reset() noexcept;
template<class U> void reset(const tracked_ptr<U>& p) noexcept;
void swap(root_ptr&) noexcept;                                  // the pointers exchanged, the cells stay
const std::type_info& type() const noexcept;                    // as tracked_ptr: the dynamic type, is<U>(), as<U>()
template<class U> bool is() const noexcept;
template<class U> tracked_ptr<U> as() const noexcept;
```

The free functions, in `sgcl`: `swap`, `==` with a `root_ptr`, a `tracked_ptr` and `nullptr`, `<=>` between `root_ptr`s (by address), `operator<<`, `std::hash<root_ptr<T>>` (of the address), the deduction guides from a `tracked_ptr` and a `unique_ptr`.

```cpp
struct Node { int value; sgcl::tracked_ptr<Node> next; };

std::vector<sgcl::root_ptr<Node>> handles;                 // roots on the unmanaged heap
handles.emplace_back(sgcl::make_tracked<Node>(Node{1}));   // a cell taken, the object rooted
sgcl::tracked_ptr<Node> n = handles[0].ptr();              // the tracked_ptr, copied onto the stack
n->next = sgcl::make_tracked<Node>(Node{2});               // reachable through the root
handles.clear();                                           // the roots gone: both nodes collectable

static sgcl::root_ptr<Node> current;                       // a global shared between threads
sgcl::atomic_ref a(current);                               // the atomic of the root: the cell's word
a.store(sgcl::make_tracked<Node>(Node{3}));
sgcl::tracked_ptr<Node> seen = a.load();
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>
#include <unordered_map>

// An interpreter's globals: named roots into the managed heap, kept in
// a std::unordered_map that lives where the interpreter does. A value
// reachable from a global stays; one dropped from the table goes with
// the next cycle, with everything only it reached.
struct Value {
    int number;
    sgcl::tracked_ptr<Value> next;
};

int main() {
    std::unordered_map<std::string, sgcl::root_ptr<Value>> globals;
    globals["list"] = sgcl::make_tracked<Value>(Value{1});
    globals["list"]->next = sgcl::make_tracked<Value>(Value{2});
    globals["alias"] = globals["list"];                       // a cell of its own, the same object
    std::cout << globals["alias"]->next->number << "\n";      // 2
    globals.erase("list");                                     // the alias still roots the list
    sgcl::collector::force_collect(true);                      // optional, for the demonstration only
    std::cout << globals["alias"]->number << "\n";             // 1
    globals.clear();                                           // no root left: the list is collectable
    return 0;
}
```

## See also

- [tracked_ptr](tracked_ptr.md), [unique_ptr](unique_ptr.md), [make_tracked](make_tracked.md), [atomic_ref](atomic_ref.md)
- README: [Stack roots](../README.md#stack-roots), [The rules](../README.md#the-rules)
- `tests/root_ptr.cpp`: every behaviour above, checked.
