[sgcl](../../README.md) › [core](../README.md)

# sgcl::root_ptr\<T\>

```cpp
#include "sgcl/core/root_ptr.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class T>
    class root_ptr;
}
```

`sgcl::root_ptr<T>` is a root that lives anywhere: a pointer to a managed object held from unmanaged memory (a
global, a `std::vector`, a handle table, a lambda on the heap, the frame of a plain coroutine), the object reachable
for as long as the `root_ptr` exists. Under it is a cell: one word of a managed block of a cache line of them, taken
from the thread's cell allocator in the constructor and given back by the destructor, and this `root_ptr`'s for the
whole time between. A block is a root by state, traced by the collector like any object, and freed by the cycle
that finds every cell of it given back once its allocator has moved on
([how it works: the cells](../../../garbage_collector/how-it-works.md#the-cells-of-the-root_ptrs)). The cell is a
`tracked_ptr` inside a managed object, so a `root_ptr` is a `tracked_ptr` held one step away:
[ptr](ptr.md) is that `tracked_ptr`, by reference, every read is its read and every store its store, with
its barrier, and an [atomic_ref](../atomic_ref.md) over it is the atomic of the root.

`root_ptr` says in its type what it is, with no mode and no test: for code that knows it stands outside the managed
heap and wants a root there, the way an interpreter keeps its handles or a program its globals. No store ever
allocates, so two threads storing into the same `root_ptr` race on one atomic word, as they do on a `tracked_ptr`,
and never on the making of a cell; no move ever takes a cell from another `root_ptr`, so a thread reading through
the cell of a `root_ptr` another thread moves from reads a cell that lives as long as its `root_ptr`. The family,
then: `unique_ptr` owns deterministically, `tracked_ptr` lives on a stack or in a managed object, `to_shared()` is a
shared root, `root_ptr` a root of its own anywhere. Go needs no such type: its collector scans the globals and
the whole heap; here unmanaged memory is the program's, and a root in it says so.

## Rules

- A `root_ptr` lives anywhere, a managed object included (where it is pointless: a `tracked_ptr` does the same for
  a word).
- The object it points at is reachable while the `root_ptr` exists; dropping the last root and every other
  reference lets the next cycle collect it. Destroying a `root_ptr` gives its cell back at once; the block goes with
  the cycle that finds every cell of it free.
- Threads share a `root_ptr` the way they share a `tracked_ptr`: one written by one thread and read by another needs
  the program's own synchronization, or an `atomic_ref` over it ([The rules](../README.md#the-rules), 6). A `root_ptr`
  may be destroyed on any thread, not only the one that made it.
- It addresses an object no `unique_ptr` owns, as a `tracked_ptr` does ([The rules](../README.md#the-rules), 4).
- The constructor registers the thread with the collector, as a `tracked_ptr`'s does.

## Template parameters

| Parameter | Description |
|---|---|
| `T` | The type pointed to: an object type, `const` or not, or `void`. |

## Member types

| Type | Definition |
|---|---|
| `element_type` | `T` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](root_ptr.md) | takes a cell and stores the pointer in it |
| `(destructor)` | gives the cell back |
| [operator=](operator_assign.md) | stores another pointer in the cell |

#### Modifiers

| Function | Description |
|---|---|
| [reset](reset.md) | stores null or another pointer in the cell |
| [swap](swap.md) | exchanges the pointers of two roots; the cells stay |

#### Observers

| Function | Description |
|---|---|
| [get](get.md) | the raw pointer |
| [operator\*, operator->](operator_deref.md) | the object |
| [operator bool](operator_bool.md) | checks whether the pointer is not null |
| [ptr, operator tracked_ptr\<T\>&](ptr.md) | the `tracked_ptr` the root holds its object by: the cell's word |

#### Dynamic type

| Function | Description |
|---|---|
| [type](type.md) | the type the object was created with |
| [is](is.md) | checks whether the object was created as a given type |
| [as](as.md) | the pointer to the whole object as a given type, or null |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator\<=\>](operator_cmp.md) | compare the addresses, with a `root_ptr`, a `tracked_ptr` or `nullptr` |
| [swap](swap.md) | exchanges the pointers of two roots |
| `operator<<` | writes the address to a `std::ostream`, as `s << p.get()` |

A `weak_ptr` and a `tracked_ptr` of a base class are made from a `root_ptr` directly (`weak_ptr w = root;`,
`tracked_ptr<Base> b = derived_root;`): their constructors and guides from `root_ptr`
([weak_ptr](../weak_ptr/README.md), [tracked_ptr](../tracked_ptr/tracked_ptr.md)).

## Deduction guides

```cpp
template<class T>
root_ptr(tracked_ptr<T>) -> root_ptr<T>;

template<class T>
root_ptr(unique_ptr<T>&&) -> root_ptr<T>;
```

## Specializations

`std::hash<root_ptr<T>>` is the hash of the address, `std::hash<const void*>`.

## Complexity

Every operation is constant. What a `root_ptr` costs: a cell per `root_ptr`, one managed allocation per block of
them (a null takes one too: the `root_ptr` may be assigned to later), one indirection per access, a cell of its own
per copy.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <string>
#include <unordered_map>

using namespace sgcl;

// an interpreter's globals: named roots into the managed heap, kept in a
// std::unordered_map that lives where the interpreter does
struct Value {
    int number;
    tracked_ptr<Value> next;
};

int main() {
    std::unordered_map<std::string, root_ptr<Value>> globals;
    globals["list"] = make_tracked<Value>(1);
    globals["list"]->next = make_tracked<Value>(2);
    globals["alias"] = globals["list"];  // a cell of its own, the same object
    println("{}", globals["alias"]->next->number);

    globals.erase("list");  // the alias still roots the list
    collector::force_collect(true);  // optional, for the demonstration
    println("{}", globals["alias"]->number);
    globals.clear();  // no root left: the list is collectable
}
```

Output:

```text
2
1
```

## See also

- [rooted](../rooted/README.md): a value under a root of its own, never null, made by its constructor
- [tracked_ptr](../tracked_ptr/README.md): the pointer the root holds its object by
- [unique_ptr](../unique_ptr/README.md), [make_tracked](../make_tracked.md): one deterministic owner
- [atomic_ref](../atomic_ref.md): the atomic of the root
- [Stack roots](../../../garbage_collector/overview.md#stack-roots), [README: The rules](../README.md#the-rules)
- `tests/core/root_ptr.cpp`: every behaviour above, checked
