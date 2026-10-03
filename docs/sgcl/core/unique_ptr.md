[sgcl](../README.md) › [core](README.md)

# sgcl::unique_ptr\<T\>

```cpp
#include "sgcl/core/unique_ptr.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class T>
    class unique_ptr;   // derives from std::unique_ptr<T, deleter_type>
}
```

`sgcl::unique_ptr<T>` is what [make_tracked](make_tracked.md)`<T>(...)` returns: a `std::unique_ptr` whose object
lives on the managed heap and whose deleter destroys it there. It behaves like any `std::unique_ptr`: sole owner,
move-only, the object destroyed at scope exit, deterministically, on the thread that drops it. What it adds is the
collector's side of the deal: while a `unique_ptr` owns an object, that object is a root, and so is everything
reachable from it; and the `unique_ptr` converts into a [tracked_ptr](tracked_ptr.md), after which the object
belongs to the collector, destroyed when nothing reaches it any more. Go has no such owner: there every object is
the collector's, as here every object handed to a `tracked_ptr` is.

The class derives publicly from `std::unique_ptr` with a deleter of the library's own (`deleter_type`), so `get()`,
`release()`, `reset()`, `swap()`, `operator*`, `operator->`, `operator bool`, the comparisons and `std::hash` are the
standard ones. On top it has the dynamic type of its object (`type()`, `is<U>()`, `as<U>()`), a conversion to
`unique_ptr<void>&`, and the three pointer casts as free functions. There is no constructor from a raw pointer: an
object enters a `unique_ptr` through `make_tracked` only.

## Rules

- A `unique_ptr` may live anywhere: on a stack, inside a managed object, in `new`/`malloc` memory, a `std`
  container, a global or a `thread_local`. The object it owns is a root by its state, wherever its owner is. A
  global root is a `unique_ptr` ([The rules](README.md#the-rules)); its object dies with the static destructors,
  after the main thread has stopped the collector, and may still make objects and copy pointers then. A
  `thread_local` `unique_ptr` of another thread is destroyed with that thread's other `thread_local`s, in the
  reverse order of their construction: one constructed before the thread's first contact with the library outlives
  the thread's registration, and its object's destructor may not use the library any more (destroy the object,
  copy no pointer, make none).
- The object a `unique_ptr` owns may not be addressed by a `tracked_ptr` or a [weak_ptr](weak_ptr.md): the owner's
  delete would leave them dangling. Debug builds assert it. Hand the object to the collector first (move the
  `unique_ptr` into a `tracked_ptr`), then take as many pointers as needed ([The rules](README.md#the-rules), 4).
- The destructor destroys the object at once, on the calling thread, like `std::unique_ptr`: its destructor runs,
  and its slot goes back to the managed heap. Anything the object reached through `tracked_ptr`s lives on if
  something else reaches it, and becomes garbage otherwise. A `unique_ptr` member
  of a managed object is destroyed with the object, wherever that happens, on a stack or in a sweep on a collector
  thread; a destructor may use its `unique_ptr` members freely, unlike its `tracked_ptr` members
  ([The rules](README.md#the-rules), 5).
- The `tracked_ptr` members of the owned object follow the rules of `tracked_ptr`: the object is on the managed
  heap, so they may live in it, and the collector traces them for as long as the owner lives.
- Thread safety is that of `std::unique_ptr`: one thread at a time, or the program's own synchronization.
- Where it belongs: at the edge of the managed world, holding an object from unmanaged memory or for the moment
  between `make_tracked` and the `tracked_ptr` that takes the object. Inside a managed object it buys only a
  deterministic destructor for the sub-object, and it costs what a `tracked_ptr` member does not: the owned object
  is a root the collector finds by its state in every cycle, and the owner's word is one the marking visits. A
  structure of managed objects is held by `tracked_ptr`s; a million `unique_ptr` members is a million roots, and a
  full cycle pays for each.

## Template parameters

| Parameter | Description |
|---|---|
| `T` | The type of the object: an object type, `const` or not, or `void`. Not an array: `unique_ptr<T[]>` is declared but not defined ([Specializations](#specializations)). |

## Member types

| Type | Definition |
|---|---|
| `element_type` | `T` |
| `pointer` | `T*`, from `std::unique_ptr` |
| `deleter_type` | the library's deleter, which destroys the object on the managed heap, from `std::unique_ptr` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](unique_ptr/unique_ptr.md) | constructs the owner |
| `(destructor)` | destroys the object, if any, at once and on the calling thread |
| [operator=](unique_ptr/operator_assign.md) | destroys the object, if any, and takes another over |

#### Modifiers

| Function | Description |
|---|---|
| [release](unique_ptr/release.md) | hands the raw pointer out and leaves the owner empty |
| [reset](unique_ptr/reset.md) | destroys the object, or replaces it |
| [swap](unique_ptr/swap.md) | exchanges the objects of two owners |

#### Observers

| Function | Description |
|---|---|
| [get](unique_ptr/get.md) | the raw pointer |
| [operator\*, operator->](unique_ptr/operator_deref.md) | the object |
| [operator bool](unique_ptr/operator_bool.md) | checks whether there is an object |
| [operator unique_ptr\<void\>&](unique_ptr/operator_conv.md) | the same owner without its type |
| `get_deleter` | the deleter, from `std::unique_ptr` |

#### Dynamic type

| Function | Description |
|---|---|
| [type](unique_ptr/type.md) | the type the object was created with |
| [is](unique_ptr/is.md) | checks whether the object was created as a given type |
| [as](unique_ptr/as.md) | moves the object into an owner of a given type, when it is one |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator\<=\>](unique_ptr/operator_cmp.md) | the comparisons of `std::unique_ptr`, on the addresses |
| [static_pointer_cast, const_pointer_cast, dynamic_pointer_cast](unique_ptr/pointer_cast.md) | the casts, moving the ownership into the result |

## Specializations

- `unique_ptr<T[]>` is declared but not defined: managed arrays belong to the containers ([vector](vector.md),
  [array](array.md)).
- `std::hash<unique_ptr<T>>` is the hash of the address, `std::hash<T*>`.

## Complexity

Every operation is constant; a destruction is what the destructor of `T` costs.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    explicit Node(int id) : id(id) {}
    ~Node() { println("Node {} destroyed", id); }
    int id;
    tracked_ptr<Node> next;
};

// a registry that owns its root: a unique_ptr may live in a global, and
// everything reachable from the Node it owns lives with it
unique_ptr registry = make_tracked<Node>(0);

// the caller decides whether the object stays deterministic (kept as a
// unique_ptr) or goes to the collector (moved into a tracked_ptr)
unique_ptr<Node> make_node(int id) {
    return make_tracked<Node>(id);
}

int main() {
    {
        unique_ptr scoped = make_node(1);
    }  // destroyed here, on this thread

    tracked_ptr shared = make_node(3);  // the collector's Node
    registry->next = shared;            // reachable from the global root
    shared = nullptr;
    println("registry -> {}", registry->next->id);
}
```

Output:

```text
Node 1 destroyed
registry -> 3
Node 0 destroyed
```

## See also

- [make_tracked](make_tracked.md): creates a managed object, owned by a `unique_ptr`
- [tracked_ptr](tracked_ptr.md): the pointer the collector follows
- [weak_ptr](weak_ptr.md): a pointer that keeps nothing alive
- [README: Pointers](README.md#pointers), [README: The rules](README.md#the-rules),
  [Pointer maps](../../garbage_collector/overview.md#pointer-maps)
