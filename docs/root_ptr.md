# sgcl::root_ptr

```cpp
#include "sgcl/root_ptr.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class T>
    class root_ptr;
}
```

`root_ptr<T>` is a root that lives anywhere: a pointer to a managed object held from unmanaged memory (a global, a `std::vector`, a handle table, a lambda on the heap), the object reachable for as long as the `root_ptr` exists. Under it is a managed holder, the one of [`tracked_ptr::to_shared`](tracked_ptr.md#to_shared), owned by a [`unique_ptr`](unique_ptr.md) and so a root by the state of its slot, with a `tracked_ptr` inside it that the collector follows. The holder is made once, in the constructor, and lives as long as the `root_ptr` does: a `root_ptr` is never without one, and an empty one holds null.

Where [`gc::tracked_ptr`](gc/tracked_ptr.md) is the pointer that lives anywhere by choosing its mode from its address, `root_ptr` says in its type what it is, with no mode and no test: for code that knows it stands outside the managed heap and wants a root there, the way an interpreter keeps its handles or a program its globals. What it costs: a managed allocation per `root_ptr` (the holder, one word), one indirection per access, a holder of its own per copy; a `gc::tracked_ptr` takes a cell from a block instead and pays a test of its mode per access. The family, then: `unique_ptr` owns deterministically, `tracked_ptr` lives on a stack or in a managed object, `gc::tracked_ptr` lives anywhere by itself, `to_shared()` is a shared root, `root_ptr` a root of its own.

## Rules

- A `root_ptr` lives anywhere, a managed object included (where it is pointless: a `tracked_ptr` does the same for a word).
- The object it points at is reachable while the `root_ptr` exists; dropping the last root and every other reference lets the next cycle collect it. Destroying a `root_ptr` destroys the holder at once (a `unique_ptr`'s delete) and its slot goes back with the next sweep.
- Threads share a `root_ptr` the way they share a `tracked_ptr`: one written by one thread and read by another needs the program's own synchronization ([The rules](../README.md#the-rules), 6).
- It addresses an object no `unique_ptr` owns, as a `tracked_ptr` does ([The rules](../README.md#the-rules), 4).

## Members

```cpp
using element_type = T;
using tracked_type = tracked_ptr<T>;

root_ptr();                                            // empty: a holder, null inside
root_ptr(std::nullptr_t);
template<class U> root_ptr(const tracked_ptr<U>& p);   // U* convertible to T*
template<class U> root_ptr(const gc::tracked_ptr<U>& p);
template<class U> root_ptr(unique_ptr<U>&& u);         // root_ptr<T> r = make_tracked<T>(...)
root_ptr(const root_ptr&);                             // a holder of its own
template<class U> root_ptr(const root_ptr<U>&);
root_ptr(root_ptr&&);                                  // the pointer moves; the source is null, its holder stays
root_ptr& operator=(...);                              // the same set, and nullptr

element_type* get() const noexcept;
element_type& operator*() const noexcept;
element_type* operator->() const noexcept;
explicit operator bool() const noexcept;
tracked_ptr<T> ptr() const noexcept;                   // the tracked_ptr, for the code that lives where one may
operator tracked_ptr<T>() const noexcept;
void reset() noexcept;
template<class U> void reset(const tracked_ptr<U>& p) noexcept;
void swap(root_ptr&) noexcept;                         // the pointers exchanged, the holders stay
const std::type_info& type() const noexcept;           // as tracked_ptr: the dynamic type, is<U>(), as<U>()
template<class U> bool is() const noexcept;
template<class U> tracked_ptr<U> as() const noexcept;
```

The free functions, in `sgcl`: `swap`, `==` with a `root_ptr`, a `tracked_ptr` of either kind and `nullptr`, `<=>` between `root_ptr`s (by address), `std::hash<root_ptr<T>>` (of the address), the deduction guides from a `tracked_ptr`, a `gc::tracked_ptr` and a `unique_ptr`.

```cpp
struct Node { int value; sgcl::tracked_ptr<Node> next; };

std::vector<sgcl::root_ptr<Node>> handles;                 // roots on the unmanaged heap
handles.push_back(sgcl::make_tracked<Node>(Node{1}));      // a holder made, the object rooted
sgcl::tracked_ptr<Node> n = handles[0].ptr();              // the tracked_ptr, on the stack
n->next = sgcl::make_tracked<Node>(Node{2});               // reachable through the root
handles.clear();                                           // the roots gone: both nodes collectable
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
    globals["alias"] = globals["list"];                       // a holder of its own, the same object
    std::cout << globals["alias"]->next->number << "\n";      // 2
    globals.erase("list");                                     // the alias still roots the list
    sgcl::collector::force_collect(true);                      // optional, for the demonstration only
    std::cout << globals["alias"]->number << "\n";             // 1
    globals.clear();                                           // no root left: the list is collectable
    return 0;
}
```

## See also

- [tracked_ptr](tracked_ptr.md), [gc::tracked_ptr](gc/tracked_ptr.md), [unique_ptr](unique_ptr.md), [make_tracked](make_tracked.md)
- README: [The two namespaces](../README.md#the-two-namespaces), [Stack roots](../README.md#stack-roots), [The rules](../README.md#the-rules)
- `tests/root_ptr.cpp`: every behaviour above, checked.
