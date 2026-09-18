# sgcl::tracked_ptr

```cpp
#include "sgcl/core/tracked_ptr.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class T>
    class tracked_ptr;
}
```

The same class in the `Sgcl` interface: [Ptr](../Sgcl/Core/Ptr.md).

`tracked_ptr<T>` is the pointer the collector follows. It is one word, the address of the object; copying it is a store of that word and the write barrier, a byte of state on the target. There is no reference count and no control block: an object lives as long as some `tracked_ptr` in a live object or on a stack, some [`unique_ptr`](unique_ptr.md), or a word on a stack reaches it, cycles included, and the collector destroys it in a later cycle, on a collector thread, when nothing does. What `std::shared_ptr` does by counting, `tracked_ptr` leaves to the collector, and what `std::shared_ptr` cannot do, a cycle of objects, needs nothing special.

The object comes from [`make_tracked`](make_tracked.md) as a `unique_ptr`; moving that into a `tracked_ptr` hands the object to the collector. A `tracked_ptr` converts to a base class, points into the middle of an object (a member, a base subobject: an alias that keeps the whole object), knows the dynamic type of its object without virtual functions (`type()`, `is<U>()`, `as<U>()`), and `tracked_ptr<void>` holds any of them. A move is a copy: the moved-from pointer keeps its value.

## Rules

- A `tracked_ptr` lives inside a managed object (one created with `make_tracked`, a node or buffer of an `sgcl` container, a managed coroutine frame) or on a thread's stack. Never in `new`/`malloc` memory, a `std` container, a global, a `thread_local`, a lambda copied to the heap, or the frame of a plain coroutine. Debug builds assert it in the constructor; a release build loses the object ([The rules](README.md#the-rules), 1; [Stack roots](../../garbage_collector/overview.md#stack-roots)). A global root is a `unique_ptr`, to the object or to a managed object holding the `tracked_ptr`.
- The object held from unmanaged memory is a `std::shared_ptr` from `to_shared()` (below): a `unique_ptr` for one owner, a `shared_ptr` for many, a `tracked_ptr` for neither.
- It does not share its storage with data: no `union` with a value, no `std::variant`, no small-buffer `std::function` or `std::any` holding one; [variant](variant.md), [any](any.md), [function](function.md) and [expected](expected.md) are the ones that keep it apart. A union of two `tracked_ptr`s and `std::optional<tracked_ptr<T>>` are fine ([Pointer maps](../../garbage_collector/overview.md#pointer-maps)).
- It addresses a managed object or a part of it, a member or a base ([Pointer aliases](README.md#pointer-aliases)); never an element of a container's buffer (`sgcl::vector`, `sgcl::array<T>`), and never an object a `unique_ptr` owns. Debug builds assert both.
- A destructor reads the `tracked_ptr` members of its object only through `if_alive()`: the object dies with everything reachable only from it, in no particular order, on a collector thread ([Pointer maps](../../garbage_collector/overview.md#pointer-maps), [Threads](../async/README.md#threads)).
- A `tracked_ptr` written by one thread and read by another needs [`atomic`](../concurrent/atomic.md) or [`atomic_ref`](../concurrent/atomic_ref.md), or the program's own synchronization. The word itself is atomic: a race is never a torn pointer, and the collector is correct under any interleaving ([The rules](README.md#the-rules), 6).
- `tracked_ptr<T[]>` is declared but not defined: managed arrays belong to the containers ([vector](../containers/vector.md), [array](../containers/array.md)).

## Members

### element_type

```cpp
using element_type = T;
```

The type pointed to; `void` for `tracked_ptr<void>`.

### Constructors

```cpp
tracked_ptr() noexcept;
tracked_ptr(std::nullptr_t) noexcept;

template<class U, std::enable_if_t<std::is_convertible_v<U*, element_type*>, int> = 0>
explicit tracked_ptr(U* p) noexcept;

tracked_ptr(const tracked_ptr& p) noexcept;
template<class U, std::enable_if_t<std::is_convertible_v<typename tracked_ptr<U>::element_type*, element_type*>, int> = 0>
tracked_ptr(const tracked_ptr<U>& p) noexcept;

tracked_ptr(tracked_ptr&& p) noexcept;
template<class U, std::enable_if_t<std::is_convertible_v<typename tracked_ptr<U>::element_type*, element_type*>, int> = 0>
tracked_ptr(tracked_ptr<U>&& p) noexcept;

template<class U, std::enable_if_t<std::is_convertible_v<typename unique_ptr<U>::element_type*, element_type*>, int> = 0>
tracked_ptr(unique_ptr<U>&& u) noexcept;
```

The default and the `nullptr` constructors make a null pointer. The raw-pointer constructor is explicit and takes the address of a managed object or of a part of it, a member or a base subobject: the alias keeps the whole object alive. Copies and moves from a `tracked_ptr<U>` with `U*` convertible to `T*` convert to the base class (or to `void`); a move is a copy, the source keeps its value. The constructor from a `unique_ptr<U>&&` releases the object from its owner: from then on the collector destroys it, when nothing reaches it any more.

Every constructor registers the calling thread with the collector on first contact (one thread-local load), stores the word with the barrier and checks, in a debug build, that the pointer lives where the rules allow and addresses what they allow.

```cpp
struct Base { virtual ~Base() = default; };
struct Item : Base { int value = 7; };

sgcl::tracked_ptr item = sgcl::make_tracked<Item>();   // from a unique_ptr: the collector owns the Item now
sgcl::tracked_ptr<Base> base = item;                    // a base class
sgcl::tracked_ptr alias(&item->value);                  // tracked_ptr<int>, into the Item
sgcl::tracked_ptr<Item> none;                           // null
item = nullptr;
base = nullptr;
assert(*alias == 7);                                    // the alias keeps the Item
```

### Destructor

```cpp
~tracked_ptr() noexcept;
```

Clears the word: a dead stack word or a slot in a destroyed object keeps nothing alive. It does not destroy the object; the collector does, once nothing reaches it.

### operator=

```cpp
tracked_ptr& operator=(std::nullptr_t) noexcept;
tracked_ptr& operator=(const tracked_ptr& p) noexcept;
template<class U, std::enable_if_t<std::is_convertible_v<typename tracked_ptr<U>::element_type*, element_type*>, int> = 0>
tracked_ptr& operator=(const tracked_ptr<U>& p) noexcept;
template<class U, std::enable_if_t<std::is_convertible_v<typename unique_ptr<U>::element_type*, element_type*>, int> = 0>
tracked_ptr& operator=(unique_ptr<U>&& u) noexcept;
tracked_ptr& operator=(tracked_ptr&& p) noexcept;
template<class U, std::enable_if_t<std::is_convertible_v<typename tracked_ptr<U>::element_type*, element_type*>, int> = 0>
tracked_ptr& operator=(tracked_ptr<U>&& p) noexcept;
```

One store with the barrier, no temporary. Assigning `nullptr` drops the reference; the object lives on if anything else reaches it. Assigning a `unique_ptr&&` releases its object to the collector. A move assignment is a copy: the source keeps its value.

```cpp
struct Node { sgcl::tracked_ptr<Node> next; };

sgcl::tracked_ptr head = sgcl::make_tracked<Node>();
head->next = sgcl::make_tracked<Node>();   // from a unique_ptr
sgcl::tracked_ptr<Node> second;
second = head->next;                       // a copy
head = nullptr;                            // the first Node is garbage, the second lives through `second`
```

### operator tracked_ptr<void>&

```cpp
operator tracked_ptr<void>&() noexcept;
operator const tracked_ptr<void>&() const noexcept;
```

Every `tracked_ptr<T>` is a `tracked_ptr<void>&`: the same word seen without its type, so a function taking a `tracked_ptr<void>&` takes any of them. `type()`, `is<U>()` and `as<U>()` still work on it, since the type is the object's, not the pointer's. A `tracked_ptr<void>` by value is made through the converting constructor (`T*` converts to `void*`).

```cpp
sgcl::tracked_ptr number = sgcl::make_tracked<int>(1);
sgcl::tracked_ptr<void>& ref = number;    // the same word
sgcl::tracked_ptr<void> any = number;     // a copy
assert(ref.is<int>() && any.is<int>());
```

### operator bool

```cpp
explicit operator bool() const noexcept;
```

True when the pointer is not null.

### operator*, operator->

```cpp
template<class U = element_type, std::enable_if_t<!std::is_void_v<U>, int> = 0>
U& operator*() const noexcept;
template<class U = element_type, std::enable_if_t<!std::is_void_v<U>, int> = 0>
U* operator->() const noexcept;
```

The object. Not for `tracked_ptr<void>`. Debug builds assert the pointer is not null.

```cpp
struct Point { int x, y; };
sgcl::tracked_ptr p = sgcl::make_tracked<Point>(1, 2);
p->x = (*p).y;
```

### get

```cpp
element_type* get() const noexcept;
```

The raw pointer. A raw pointer keeps nothing alive by itself: it is valid while a `tracked_ptr`, a `unique_ptr` or a container keeps its target, as with `std` ([The rules](README.md#the-rules), 3). A raw pointer that stays in a stack frame does keep its target until the word is overwritten, since the stack is scanned conservatively; that is a delay, not a guarantee ([Stack roots](../../garbage_collector/overview.md#stack-roots)).

```cpp
struct Item { int value = 1; };
sgcl::tracked_ptr item = sgcl::make_tracked<Item>();
Item* raw = item.get();     // valid while `item` (or another tracked_ptr) keeps the object
raw->value = 2;
```

### if_alive

```cpp
tracked_ptr if_alive() const noexcept;
```

For destructors. A copy of the pointer, or null when its target is dying in the same sweep as the object being destroyed: an object dies together with everything reachable only from it, in no particular order and on several threads, so a `tracked_ptr` member of a dying object may point at an object destroyed already, and the collector does not null such members beforehand. Outside a sweep, `if_alive()` is a plain copy: a `tracked_ptr` in a live object points at a live object. A pointer to a dying object is never handed out; a live one may be used and stored freely ([Pointer maps](../../garbage_collector/overview.md#pointer-maps); [The rules](README.md#the-rules), 5).

```cpp
struct Node {
    ~Node() {
        if (auto p = peer.if_alive()) {   // a copy when the peer lives, null when it dies in the same sweep
            p->detached = true;
        }
    }
    bool detached = false;
    sgcl::tracked_ptr<Node> peer;
};
```

### reset

```cpp
void reset() noexcept;
void reset(element_type* p) noexcept;
```

`reset()` is `*this = nullptr`. `reset(p)` replaces the pointer with a raw one under the rules of the raw-pointer constructor (a managed object or a part of it, never an element of a container's buffer, never an object a `unique_ptr` owns): one store with its barrier, no temporary. Debug builds assert the rules.

```cpp
struct Item { int value = 3; };
sgcl::tracked_ptr item = sgcl::make_tracked<Item>();
sgcl::tracked_ptr<int> alias;
alias.reset(&item->value);   // an alias into the Item
alias.reset();               // null
```

### swap

```cpp
void swap(tracked_ptr& p) noexcept;
```

Exchanges the two pointers, through a temporary on the stack. There is no free `swap` for `tracked_ptr`; `std::swap` works through the move operations.

```cpp
sgcl::tracked_ptr a = sgcl::make_tracked<int>(1);
sgcl::tracked_ptr b = sgcl::make_tracked<int>(2);
a.swap(b);
assert(*a == 2 && *b == 1);
```

### to_shared

```cpp
std::shared_ptr<element_type> to_shared() const;
```

The object held from unmanaged memory. Returns a `std::shared_ptr` to the object whose control block owns a managed holder of this pointer: the holder is a root (its `unique_ptr` lives in the control block), so the object is reachable for as long as any copy of the `shared_ptr` lives, and the `shared_ptr` itself may live anywhere a `tracked_ptr` may not: `new` memory, a `std` container, a global, a lambda copied to the heap or run on another thread. Copies of the `shared_ptr` share the control block and the holder; only the call allocates, twice (the holder on the managed heap, the control block on the unmanaged one), which is why this is a named function and not a conversion. A null pointer gives a null `shared_ptr`. An alias into a member gives a `shared_ptr` to the member that keeps the whole object.

The object stays managed: when the last `shared_ptr` is gone the holder is released and the object lives on if anything else reaches it, and is destroyed on a collector thread, under the rules of destructors, once nothing does. A `shared_ptr` from `to_shared()` is not a deterministic owner the way `unique_ptr` is.

```cpp
struct Node { int value = 7; sgcl::tracked_ptr<Node> next; };

std::vector<std::shared_ptr<Node>> kept;           // a std container: no tracked_ptr may live in it
sgcl::tracked_ptr node = sgcl::make_tracked<Node>();
node->next = sgcl::make_tracked<Node>();
kept.push_back(node.to_shared());                  // the Node and its next live while the shared_ptr does
node = nullptr;
sgcl::collector::force_collect();                  // optional, to show the result at once
assert(kept[0]->next->value == 7);
kept.clear();                                      // the last copy: the holder is released, the Node is garbage
```

### is, as, type

```cpp
template<class U>
bool is() const noexcept;

template<class U>
tracked_ptr<U> as() const noexcept;

const std::type_info& type() const noexcept;
```

The dynamic type of the object, read from the metadata of its page, without virtual functions and through a `tracked_ptr<void>` as well. `type()` is the `std::type_info` of the type the object was created with (`make_tracked<U>`), whatever the pointer's `T` and wherever it points inside the object; for a null pointer it is `typeid(T)`. `is<U>()` is `type() == typeid(U)`: an exact match, not an "is derived from" test (a `tracked_ptr<Base>` to a `Derived` is `is<Derived>()`, not `is<Base>()`; a derived-class test is `dynamic_pointer_cast`). `as<U>()` is a `tracked_ptr<U>` to the whole object when `is<U>()`, also from an alias into a member or a base, and null otherwise.

```cpp
struct Shape { virtual ~Shape() = default; };
struct Circle : Shape { double r = 1; };

sgcl::tracked_ptr<Shape> shape = sgcl::make_tracked<Circle>();
assert(shape.type() == typeid(Circle));
assert(shape.is<Circle>() && !shape.is<Shape>());
if (sgcl::tracked_ptr circle = shape.as<Circle>()) {   // tracked_ptr<Circle>
    circle->r = 2;
}
sgcl::tracked_ptr radius(&shape.as<Circle>()->r);      // an alias into the Circle
assert(radius.is<Circle>());                            // the object's type, not the alias's
assert(radius.as<Circle>() == shape);                   // back to the whole object
```

### Deduction guides

```cpp
template<typename T> tracked_ptr(T*) -> tracked_ptr<T>;
template<typename T> tracked_ptr(tracked_ptr<T>) -> tracked_ptr<T>;
template<typename T> tracked_ptr(unique_ptr<T>&&) -> tracked_ptr<T>;
```

`sgcl::tracked_ptr p = sgcl::make_tracked<T>(...)` is a `tracked_ptr<T>`; the explicit argument is needed only where the deduction would pick another type, a base class or a null initializer, or for a member declaration.

```cpp
struct Item { int value; };
sgcl::tracked_ptr item = sgcl::make_tracked<Item>(4);   // tracked_ptr<Item>
sgcl::tracked_ptr copy = item;                          // tracked_ptr<Item>
sgcl::tracked_ptr value(&item->value);                  // tracked_ptr<int>
```

### Comparisons

```cpp
template<class T, class U> std::strong_ordering operator<=>(const tracked_ptr<T>& l, const tracked_ptr<U>& r) noexcept;
template<class T, class U> bool operator==(const tracked_ptr<T>& l, const tracked_ptr<U>& r) noexcept;
template<class T> std::strong_ordering operator<=>(const tracked_ptr<T>& l, std::nullptr_t) noexcept;
template<class T> bool operator==(const tracked_ptr<T>& l, std::nullptr_t) noexcept;
template<class T> std::strong_ordering operator<=>(std::nullptr_t, const tracked_ptr<T>& r) noexcept;
template<class T> bool operator==(std::nullptr_t, const tracked_ptr<T>& r) noexcept;
```

The addresses compared, as with raw pointers: all six relational operators between two `tracked_ptr`s (of any two types whose raw pointers have a common type) and between a `tracked_ptr` and `nullptr`.

```cpp
sgcl::tracked_ptr a = sgcl::make_tracked<int>(1);
sgcl::tracked_ptr b = a;
assert(a == b && a != nullptr && nullptr < a && a <= b);
```

### static_pointer_cast, const_pointer_cast, dynamic_pointer_cast

```cpp
template<class T, class U> tracked_ptr<T> static_pointer_cast(const tracked_ptr<U>& p) noexcept;
template<class T, class U> tracked_ptr<T> const_pointer_cast(const tracked_ptr<U>& p) noexcept;
template<class T, class U> tracked_ptr<T> dynamic_pointer_cast(const tracked_ptr<U>& p) noexcept;
```

The casts of `std::shared_ptr`: a `tracked_ptr<T>` to the result of the cast of the raw pointer (null when the `dynamic_cast` fails). The result is an alias of the same object, which lives as long as the alias does.

```cpp
struct Shape { virtual ~Shape() = default; };
struct Circle : Shape { double r = 1; };

sgcl::tracked_ptr<const Shape> shape = sgcl::make_tracked<Circle>();
sgcl::tracked_ptr circle = sgcl::dynamic_pointer_cast<const Circle>(shape);   // null for another Shape
sgcl::tracked_ptr mutable_shape = sgcl::const_pointer_cast<Shape>(shape);
sgcl::tracked_ptr known = sgcl::static_pointer_cast<Circle>(mutable_shape);
assert(circle && known->r == 1);
```

### operator<<

```cpp
template<class T> std::ostream& operator<<(std::ostream& s, const tracked_ptr<T>& p);
```

Prints the address, as `s << p.get()`.

### std::hash

```cpp
template<class T> struct std::hash<sgcl::tracked_ptr<T>>;
```

The hash of the address, `std::hash<T*>`. A `tracked_ptr` can be the key of an `sgcl::unordered_map` or `unordered_set` (not of a `std` one, where it would live in unmanaged memory).

```cpp
sgcl::tracked_ptr item = sgcl::make_tracked<int>(1);
sgcl::unordered_set<sgcl::tracked_ptr<int>> seen;
seen.insert(item);
assert(seen.contains(item));
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <cassert>
#include <iostream>

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
    ~Node() {
        // A destructor runs on a collector thread; its tracked_ptr members
        // are read through if_alive() only: a copy when the target lives,
        // null when it dies in the same sweep.
        if (auto p = next.if_alive()) {
            p->id = -1;
        }
    }
    int id;
    sgcl::tracked_ptr<Node> next;
};

int main() {
    // A ring of three nodes: a cycle, collected like anything else
    sgcl::tracked_ptr a = sgcl::make_tracked<Node>(1);   // tracked_ptr<Node>, deduced
    a->next = sgcl::make_tracked<Node>(2);
    a->next->next = sgcl::make_tracked<Node>(3);
    a->next->next->next = a;
    sgcl::tracked_ptr b = a->next;                       // a second root into the ring
    a = nullptr;                                         // the ring lives on through b
    assert(b->next->next->next == b);                    // three steps around the ring

    // A base class and the dynamic type
    sgcl::tracked_ptr<Shape> shape = sgcl::make_tracked<Circle>(2);
    std::cout << "area " << shape->area() << '\n';
    if (shape.is<Circle>()) {
        sgcl::tracked_ptr circle = shape.as<Circle>();   // tracked_ptr<Circle>
        std::cout << "radius " << circle->r << '\n';
    }

    // An alias into a member keeps the whole object
    sgcl::tracked_ptr id(&b->id);                        // tracked_ptr<int>
    b = nullptr;
    std::cout << "id " << *id << '\n';                   // 2: the ring is still alive

    id = nullptr;                                        // nothing reaches the ring now
    sgcl::collector::force_collect(true);                // optional, for the demonstration only: the collector runs its cycles by itself
    return 0;
}
```

The output:

```
area 12.5664
radius 2
id 2
```

## See also

- [unique_ptr](unique_ptr.md), [make_tracked](make_tracked.md), [weak_ptr](weak_ptr.md)
- [atomic](../concurrent/atomic.md), [atomic_ref](../concurrent/atomic_ref.md) for a `tracked_ptr` shared between threads
- [collector](collector.md) for `force_collect` and the counting functions
- README: [The classes](README.md#the-classes), [Pointer aliases](README.md#pointer-aliases), [The rules](README.md#the-rules), [Pointer maps](../../garbage_collector/overview.md#pointer-maps), [Stack roots](../../garbage_collector/overview.md#stack-roots), [Threads](../async/README.md#threads)
- `examples/example.cpp`, `examples/threads.cpp`
