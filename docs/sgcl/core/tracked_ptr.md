[sgcl](../README.md) › [core](README.md)

# sgcl::tracked_ptr\<T\>

```cpp
#include "sgcl/core/tracked_ptr.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class T>
    class tracked_ptr;
}
```

`sgcl::tracked_ptr<T>` is the pointer the collector follows. It is one word, the address of the object; copying it
is a store of that word and the write barrier, a byte of state on the target. There is no reference count and no
control block: an object lives as long as some `tracked_ptr` in a live object or on a stack, some
[unique_ptr](unique_ptr.md), or a word on a stack reaches it, cycles included, and the collector destroys it in a
later cycle, on a collector thread, when nothing does. What `std::shared_ptr` does by counting, `tracked_ptr` leaves
to the collector, and what `std::shared_ptr` cannot do, a cycle of objects, needs nothing special. It is what a
pointer is in Go or a reference in Java, with the interface of a `std` smart pointer.

The object comes from [make_tracked](make_tracked.md) as a `unique_ptr`; moving that into a `tracked_ptr` hands the
object to the collector. A `tracked_ptr` converts to a base class, points into the middle of an object (a member,
a base subobject: an alias that keeps the whole object), knows the dynamic type of its object without virtual
functions (`type()`, `is<U>()`, `as<U>()`), and `tracked_ptr<void>` holds any of them. A move is a copy: the
moved-from pointer keeps its value.

## Rules

- A `tracked_ptr` lives inside a managed object (one created with `make_tracked`, a node or buffer of an `sgcl`
  container, a managed coroutine frame) or on a thread's stack. Never in `new`/`malloc` memory, a `std`
  container, a global, a `thread_local`, a lambda copied to the heap, or the frame of a plain coroutine. Debug
  builds assert it in the constructor; a release build loses the object ([The rules](README.md#the-rules), 1;
  [Stack roots](../../garbage_collector/overview.md#stack-roots)). A global root is a `unique_ptr`, to the object
  or to a managed object holding the `tracked_ptr`, or a [root_ptr](root_ptr.md).
- The object held from unmanaged memory is a `std::shared_ptr` from [to_shared](tracked_ptr/to_shared.md): a
  `unique_ptr` for one owner, a `shared_ptr` for many, a `tracked_ptr` for neither.
- It does not share its storage with data: no `union` with a value, no `std::variant`, no small-buffer
  `std::function` or `std::any` holding one; [variant](variant.md), [any](any.md), [function](function.md) and
  [expected](expected.md) are the ones that keep it apart. A union of two `tracked_ptr`s and
  `std::optional<tracked_ptr<T>>` are fine ([Pointer maps](../../garbage_collector/overview.md#pointer-maps)).
- It addresses a managed object or a part of it, a member or a base; never an element of a container's buffer
  (`sgcl::vector`, `sgcl::dynamic_array<T>`), and never an object a `unique_ptr` owns. Debug builds assert both.
- A destructor reads the `tracked_ptr` members of its object only through [if_alive](tracked_ptr/if_alive.md): the
  object dies with everything reachable only from it, in no particular order, on a collector thread
  ([Pointer maps](../../garbage_collector/overview.md#pointer-maps), [Threads](../async/README.md#threads)).
- A `tracked_ptr` written by one thread and read by another needs [atomic](atomic.md) or
  [atomic_ref](atomic_ref.md), or the program's own synchronization. The word itself is atomic: a race is never a
  torn pointer, and the collector is correct under any interleaving ([The rules](README.md#the-rules), 6).

## Template parameters

| Parameter | Description |
|---|---|
| `T` | The type pointed to: an object type, `const` or not, or `void`. Not an array: `tracked_ptr<T[]>` is declared but not defined ([Specializations](#specializations)). |

## Member types

| Type | Definition |
|---|---|
| `element_type` | `T`; `void` for `tracked_ptr<void>` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](tracked_ptr/tracked_ptr.md) | constructs the pointer |
| `(destructor)` | clears the word; the object is left to the collector |
| [operator=](tracked_ptr/operator_assign.md) | assigns the pointer |

#### Modifiers

| Function | Description |
|---|---|
| [reset](tracked_ptr/reset.md) | replaces the pointer with null or a raw one |
| [swap](tracked_ptr/swap.md) | exchanges two pointers |
| [store](tracked_ptr/store.md) | stores the word without the write barrier, for a copy of an immutable node |
| [shade](tracked_ptr/shade.md) | the write barrier for the target, on demand |

#### Observers

| Function | Description |
|---|---|
| [get](tracked_ptr/get.md) | the raw pointer |
| [operator\*, operator->](tracked_ptr/operator_deref.md) | the object |
| [operator bool](tracked_ptr/operator_bool.md) | checks whether the pointer is not null |
| [operator tracked_ptr\<void\>&](tracked_ptr/operator_conv.md) | the same word without its type |
| [if_alive](tracked_ptr/if_alive.md) | the pointer, or null when its target dies in the same sweep: for destructors |
| [to_shared](tracked_ptr/to_shared.md) | the object as a `std::shared_ptr`, held from unmanaged memory |

#### Dynamic type

| Function | Description |
|---|---|
| [type](tracked_ptr/type.md) | the type the object was created with |
| [is](tracked_ptr/is.md) | checks whether the object was created as a given type |
| [as](tracked_ptr/as.md) | the pointer to the whole object as a given type, or null |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator\<=\>](tracked_ptr/operator_cmp.md) | compare the addresses, with another pointer or with `nullptr` |
| [static_pointer_cast, const_pointer_cast, dynamic_pointer_cast](tracked_ptr/pointer_cast.md) | the casts of `std::shared_ptr`, to an alias of the same object |
| `operator<<` | writes the address to a `std::ostream`, as `s << p.get()` |

## Deduction guides

```cpp
template<typename T>
tracked_ptr(T*) -> tracked_ptr<T>;

template<typename T>
tracked_ptr(tracked_ptr<T>) -> tracked_ptr<T>;

template<typename T>
tracked_ptr(unique_ptr<T>&&) -> tracked_ptr<T>;

template<class T>
tracked_ptr(const root_ptr<T>&) -> tracked_ptr<T>;
```

`tracked_ptr p = make_tracked<T>(...)` is a `tracked_ptr<T>`; the explicit argument is needed only where the
deduction would pick another type, a base class or a null initializer, or for a member declaration.

## Specializations

- `tracked_ptr<T[]>` is declared but not defined: managed arrays belong to the containers ([vector](vector.md),
  [array](array.md)).
- `std::hash<tracked_ptr<T>>` is the hash of the address, `std::hash<T*>`. A `tracked_ptr` can be the key of an
  `sgcl::map` or `set` (not of a `std` one, where it would live in unmanaged memory).
- `atomic<tracked_ptr<T>>` and `atomic_ref<tracked_ptr<T>>` are the atomics of the word ([atomic](atomic.md),
  [atomic_ref](atomic_ref.md)).

## Complexity

Every operation is constant. A construction registers the calling thread with the collector on first contact (one
thread-local load), stores the word with the barrier, and checks, in a debug build, that the pointer lives where
the rules allow and addresses what they allow. An assignment is one store with the barrier, no temporary.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Shape {
    virtual ~Shape() = default;
    virtual double area() const = 0;
};

struct Circle : Shape {
    explicit Circle(double r) : r(r) {}
    double area() const override { return 3.14159 * r * r; }
    double r;
};

struct Node {
    explicit Node(int id) : id(id) {}
    int id;
    tracked_ptr<Node> next;
};

int main() {
    // a ring of three nodes: a cycle, collected like anything else
    tracked_ptr a = make_tracked<Node>(1);
    a->next = make_tracked<Node>(2);
    a->next->next = make_tracked<Node>(3);
    a->next->next->next = a;
    tracked_ptr b = a->next;  // a second root into the ring
    a = nullptr;              // the ring lives on through b
    println("{}", b->next->next->next == b);

    // a base class and the dynamic type
    tracked_ptr<Shape> shape = make_tracked<Circle>(2);
    println("area {}", shape->area());
    if (tracked_ptr circle = shape.as<Circle>()) {
        println("radius {}", circle->r);
    }

    // an alias into a member keeps the whole object
    tracked_ptr id(&b->id);
    b = nullptr;
    println("id {}", *id);
}
```

Output:

```text
true
area 12.56636
radius 2
id 2
```

## See also

- [unique_ptr](unique_ptr.md): the owner `make_tracked` returns
- [make_tracked](make_tracked.md): creates a managed object
- [weak_ptr](weak_ptr.md): a pointer that keeps nothing alive
- [root_ptr](root_ptr.md): a root that lives anywhere
- [atomic](atomic.md), [atomic_ref](atomic_ref.md): a `tracked_ptr` shared between threads
- [collector](collector.md): `force_collect` and the counting functions
- [README: Pointers](README.md#pointers), [README: The rules](README.md#the-rules),
  [Pointer maps](../../garbage_collector/overview.md#pointer-maps),
  [Stack roots](../../garbage_collector/overview.md#stack-roots), [Threads](../async/README.md#threads)
