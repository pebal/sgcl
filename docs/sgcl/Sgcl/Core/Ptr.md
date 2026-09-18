# Sgcl::Ptr

```cpp
#include "sgcl/Sgcl/Core/Ptr.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class T>
    class Ptr;
}
```

The same class in the `sgcl` interface: [tracked_ptr](../../core/tracked_ptr.md).

`Ptr<T>` is the pointer the collector follows. It is one word, the address of the object; copying it is a store of that word and the write barrier, a byte of state on the target. There is no reference count and no control block: an object lives as long as some `Ptr` in a live object or on a stack, some [`UniquePtr`](UniquePtr.md), or a word on a stack reaches it, cycles included, and the collector destroys it in a later cycle, on a collector thread, when nothing does. What `std::shared_ptr` does by counting, `Ptr` leaves to the collector, and what `std::shared_ptr` cannot do, a cycle of objects, needs nothing special.

The object comes from [`Make`](Make.md) as a `UniquePtr`; moving that into a `Ptr` hands the object to the collector. A `Ptr` converts to a base class, points into the middle of an object (a member, a base subobject: an alias that keeps the whole object), knows the dynamic type of its object without virtual functions (`Type()`, `Is<U>()`, `As<U>()`), and `Ptr<void>` holds any of them. A move is a copy: the moved-from pointer keeps its value.

## Rules

- A `Ptr` lives inside a managed object (one created with `Make`, a node or buffer of a `Sgcl` container, a managed coroutine frame) or on a thread's stack. Never in `new`/`malloc` memory, a `std` container, a global, a `thread_local`, a lambda copied to the heap, or the frame of a plain coroutine. Debug builds assert it in the constructor; a release build loses the object ([The rules](../../core/README.md#the-rules), 1; [Stack roots](../../../garbage_collector/overview.md#stack-roots)). A global root is a `UniquePtr`, to the object or to a managed object holding the `Ptr`, or a [`RootPtr`](RootPtr.md).
- The object held from unmanaged memory is a `std::shared_ptr` from `ToShared()` (below): a `UniquePtr` for one owner, a `shared_ptr` for many, a `Ptr` for neither.
- It does not share its storage with data: no `union` with a value, no `std::variant`, no small-buffer `std::function` or `std::any` holding one; [Variant](Variant.md), [Any](Any.md), [Function](Function.md) and [Expected](Expected.md) are the ones that keep it apart. A union of two `Ptr`s and an `Optional<Ptr<T>>` are fine ([Pointer maps](../../../garbage_collector/overview.md#pointer-maps)).
- It addresses a managed object or a part of it, a member or a base ([Pointer aliases](../../core/README.md#pointer-aliases)); never an element of a container's buffer (`List`, `Array<T>`), and never an object a `UniquePtr` owns. Debug builds assert both.
- A destructor reads the `Ptr` members of its object only through `IfAlive()`: the object dies with everything reachable only from it, in no particular order, on a collector thread ([Pointer maps](../../../garbage_collector/overview.md#pointer-maps), [Threads](../../async/README.md#threads)).
- A `Ptr` written by one thread and read by another needs [`Atomic`](../Concurrent/Atomic.md) or [`AtomicRef`](../Concurrent/AtomicRef.md), or the program's own synchronization. The word itself is atomic: a race is never a torn pointer, and the collector is correct under any interleaving ([The rules](../../core/README.md#the-rules), 6).
- There is no `Ptr<T[]>`: managed arrays belong to the containers ([List](../Containers/List.md), [Array](../Containers/Array.md)).

## Members

### ElementType, InnerType

```cpp
using ElementType = T;
using InnerType = sgcl::tracked_ptr<T>;
```

The type pointed to (`void` for `Ptr<void>`), and the type of the one word inside, which `Inner()` hands out.

### Constructors

```cpp
Ptr() noexcept;
Ptr(std::nullptr_t) noexcept;
template<class U> requires std::is_convertible_v<U*, T*> explicit Ptr(U* p) noexcept;
Ptr(const Ptr& p) noexcept;
template<class U> requires std::is_convertible_v<U*, T*> Ptr(const Ptr<U>& p) noexcept;
Ptr(Ptr&& p) noexcept;
template<class U> requires std::is_convertible_v<U*, T*> Ptr(Ptr<U>&& p) noexcept;
template<class U> requires std::is_convertible_v<U*, T*> Ptr(UniquePtr<U>&& u) noexcept;
template<class U> requires std::is_convertible_v<U*, T*> Ptr(const RootPtr<U>& r) noexcept;
explicit Ptr(InnerType p) noexcept;
```

The default and the `nullptr` constructors make a null pointer. The raw-pointer constructor is explicit and takes the address of a managed object or of a part of it, a member or a base subobject: the alias keeps the whole object alive. Copies and moves from a `Ptr<U>` with `U*` convertible to `T*` convert to the base class (or to `void`); a move is a copy, the source keeps its value. The constructor from a `UniquePtr<U>&&` releases the object from its owner: from then on the collector destroys it, when nothing reaches it any more. The constructor from a `RootPtr` copies the word the root holds.

Every constructor registers the calling thread with the collector on first contact (one thread-local load), stores the word with the barrier and checks, in a debug build, that the pointer lives where the rules allow and addresses what they allow.

```cpp
struct Base { virtual ~Base() = default; };
struct Item : Base { int value = 7; };

Ptr item = Make<Item>();   // from a UniquePtr: the collector owns the Item now
Ptr<Base> base = item;              // a base class
Ptr alias(&item->value);            // Ptr<int>, into the Item
Ptr<Item> none;                     // null
item = nullptr;
base = nullptr;
assert(*alias == 7);                         // the alias keeps the Item
```

### Destructor

```cpp
~Ptr() noexcept;
```

Clears the word: a dead stack word or a slot in a destroyed object keeps nothing alive. It does not destroy the object; the collector does, once nothing reaches it.

### operator=

```cpp
Ptr& operator=(std::nullptr_t) noexcept;
Ptr& operator=(const Ptr& p) noexcept;
template<class U> requires std::is_convertible_v<U*, T*> Ptr& operator=(const Ptr<U>& p) noexcept;
template<class U> requires std::is_convertible_v<U*, T*> Ptr& operator=(UniquePtr<U>&& u) noexcept;
Ptr& operator=(Ptr&& p) noexcept;
template<class U> requires std::is_convertible_v<U*, T*> Ptr& operator=(Ptr<U>&& p) noexcept;
```

One store with the barrier, no temporary. Assigning `nullptr` drops the reference; the object lives on if anything else reaches it. Assigning a `UniquePtr&&` releases its object to the collector. A move assignment is a copy: the source keeps its value.

```cpp
struct Node { Ptr<Node> next; };

Ptr head = Make<Node>();
head->next = Make<Node>();   // from a UniquePtr
Ptr<Node> second;
second = head->next;                  // a copy
head = nullptr;                       // the first Node is garbage, the second lives through `second`
```

### operator Ptr<void>&

```cpp
operator Ptr<void>&() noexcept;
operator const Ptr<void>&() const noexcept;
```

Every `Ptr<T>` is a `Ptr<void>&`: the same word seen without its type, so a function taking a `Ptr<void>&` takes any of them. `Type()`, `Is<U>()` and `As<U>()` still work on it, since the type is the object's, not the pointer's. A `Ptr<void>` by value is made through the converting constructor (`T*` converts to `void*`).

```cpp
Ptr number = Make<int>(1);
Ptr<void>& ref = number;    // the same word
Ptr<void> any = number;     // a copy
assert(ref.Is<int>() && any.Is<int>());
```

### operator bool

```cpp
explicit operator bool() const noexcept;
```

True when the pointer is not null.

### operator*, operator->

```cpp
T& operator*() const noexcept;    // not for Ptr<void>
T* operator->() const noexcept;
```

The object. Not for `Ptr<void>`. Debug builds assert the pointer is not null.

```cpp
struct Point { int x, y; };
Ptr p = Make<Point>(1, 2);
p->x = (*p).y;
```

### Get

```cpp
T* Get() const noexcept;
```

The raw pointer. A raw pointer keeps nothing alive by itself: it is valid while a `Ptr`, a `UniquePtr` or a container keeps its target, as with `std` ([The rules](../../core/README.md#the-rules), 3). A raw pointer that stays in a stack frame does keep its target until the word is overwritten, since the stack is scanned conservatively; that is a delay, not a guarantee ([Stack roots](../../../garbage_collector/overview.md#stack-roots)).

```cpp
struct Item { int value = 1; };
Ptr item = Make<Item>();
Item* raw = item.Get();     // valid while `item` (or another Ptr) keeps the object
raw->value = 2;
```

### IfAlive

```cpp
Ptr IfAlive() const noexcept;
```

For destructors. A copy of the pointer, or null when its target is dying in the same sweep as the object being destroyed: an object dies together with everything reachable only from it, in no particular order and on several threads, so a `Ptr` member of a dying object may point at an object destroyed already, and the collector does not null such members beforehand. Outside a sweep, `IfAlive()` is a plain copy: a `Ptr` in a live object points at a live object. A pointer to a dying object is never handed out; a live one may be used and stored freely ([Pointer maps](../../../garbage_collector/overview.md#pointer-maps); [The rules](../../core/README.md#the-rules), 5).

```cpp
struct Node {
    ~Node() {
        if (auto p = peer.IfAlive()) {   // a copy when the peer lives, null when it dies in the same sweep
            p->detached = true;
        }
    }
    bool detached = false;
    Ptr<Node> peer;
};
```

### Reset

```cpp
void Reset() noexcept;
void Reset(T* p) noexcept;
```

`Reset()` is `*this = nullptr`. `Reset(p)` replaces the pointer with a raw one under the rules of the raw-pointer constructor (a managed object or a part of it, never an element of a container's buffer, never an object a `UniquePtr` owns): one store with its barrier, no temporary. Debug builds assert the rules.

```cpp
struct Item { int value = 3; };
Ptr item = Make<Item>();
Ptr<int> alias;
alias.Reset(&item->value);   // an alias into the Item
alias.Reset();               // null
```

### Swap

```cpp
void Swap(Ptr& p) noexcept;
```

Exchanges the two pointers, through a temporary on the stack. The free `swap(Ptr&, Ptr&)` does the same.

```cpp
Ptr a = Make<int>(1);
Ptr b = Make<int>(2);
a.Swap(b);
assert(*a == 2 && *b == 1);
```

### ToShared

```cpp
std::shared_ptr<T> ToShared() const;
```

The object held from unmanaged memory. Returns a `std::shared_ptr` to the object whose control block owns a managed holder of this pointer: the holder is a root (its owner lives in the control block), so the object is reachable for as long as any copy of the `shared_ptr` lives, and the `shared_ptr` itself may live anywhere a `Ptr` may not: `new` memory, a `std` container, a global, a lambda copied to the heap or run on another thread. Copies of the `shared_ptr` share the control block and the holder; only the call allocates, twice (the holder on the managed heap, the control block on the unmanaged one), which is why this is a named function and not a conversion. A null pointer gives a null `shared_ptr`. An alias into a member gives a `shared_ptr` to the member that keeps the whole object.

The object stays managed: when the last `shared_ptr` is gone the holder is released and the object lives on if anything else reaches it, and is destroyed on a collector thread, under the rules of destructors, once nothing does. A `shared_ptr` from `ToShared()` is not a deterministic owner the way `UniquePtr` is.

```cpp
struct Node { int value = 7; Ptr<Node> next; };

std::vector<std::shared_ptr<Node>> kept;      // a std container: no Ptr may live in it
Ptr node = Make<Node>();
node->next = Make<Node>();
kept.push_back(node.ToShared());              // the Node and its next live while the shared_ptr does
node = nullptr;
Collector::Collect();                // optional, to show the result at once
assert(kept[0]->next->value == 7);
kept.clear();                                 // the last copy: the holder is released, the Node is garbage
```

### Is, As, Type

```cpp
template<class U> bool Is() const noexcept;
template<class U> Ptr<U> As() const noexcept;
const std::type_info& Type() const noexcept;
```

The dynamic type of the object, read from the metadata of its page, without virtual functions and through a `Ptr<void>` as well. `Type()` is the `std::type_info` of the type the object was created with (`Make<U>`), whatever the pointer's `T` and wherever it points inside the object; for a null pointer it is `typeid(T)`. `Is<U>()` is `Type() == typeid(U)`: an exact match, not an "is derived from" test (a `Ptr<Base>` to a `Derived` is `Is<Derived>()`, not `Is<Base>()`; a derived-class test is `DynamicCast`). `As<U>()` is a `Ptr<U>` to the whole object when `Is<U>()`, also from an alias into a member or a base, and null otherwise.

```cpp
struct Shape { virtual ~Shape() = default; };
struct Circle : Shape { double r = 1; };

Ptr<Shape> shape = Make<Circle>();
assert(shape.Type() == typeid(Circle));
assert(shape.Is<Circle>() && !shape.Is<Shape>());
if (Ptr circle = shape.As<Circle>()) {   // Ptr<Circle>
    circle->r = 2;
}
Ptr radius(&shape.As<Circle>()->r);      // an alias into the Circle
assert(radius.Is<Circle>());                      // the object's type, not the alias's
assert(radius.As<Circle>() == shape);             // back to the whole object
```

### Inner

```cpp
InnerType& Inner() noexcept;
const InnerType& Inner() const noexcept;
```

The one word inside, as its own type, for the code that wants it that way.

### Deduction guides

```cpp
template<class T> Ptr(T*) -> Ptr<T>;
template<class T> Ptr(InnerType) -> Ptr<T>;
template<class T> Ptr(UniquePtr<T>&&) -> Ptr<T>;
template<class T> Ptr(const RootPtr<T>&) -> Ptr<T>;
```

`Ptr p = Make<T>(...)` is a `Ptr<T>`; the explicit argument is needed only where the deduction would pick another type, a base class or a null initializer, or for a member declaration.

```cpp
struct Item { int value; };
Ptr item = Make<Item>(4);   // Ptr<Item>
Ptr copy = item;                     // Ptr<Item>
Ptr value(&item->value);             // Ptr<int>
```

### Comparisons

```cpp
template<class T, class U> std::strong_ordering operator<=>(const Ptr<T>& l, const Ptr<U>& r) noexcept;
template<class T, class U> bool operator==(const Ptr<T>& l, const Ptr<U>& r) noexcept;
template<class T> std::strong_ordering operator<=>(const Ptr<T>& l, std::nullptr_t) noexcept;
template<class T> bool operator==(const Ptr<T>& l, std::nullptr_t) noexcept;
```

The addresses compared, as with raw pointers: all six relational operators between two `Ptr`s (of any two types whose raw pointers have a common type) and between a `Ptr` and `nullptr`.

```cpp
Ptr a = Make<int>(1);
Ptr b = a;
assert(a == b && a != nullptr && nullptr < a && a <= b);
```

### StaticCast, ConstCast, DynamicCast

```cpp
template<class T, class U> Ptr<T> StaticCast(const Ptr<U>& p) noexcept;
template<class T, class U> Ptr<T> ConstCast(const Ptr<U>& p) noexcept;
template<class T, class U> Ptr<T> DynamicCast(const Ptr<U>& p) noexcept;
```

The casts of `std::shared_ptr`: a `Ptr<T>` to the result of the cast of the raw pointer (null when the `dynamic_cast` fails). The result is an alias of the same object, which lives as long as the alias does.

```cpp
struct Shape { virtual ~Shape() = default; };
struct Circle : Shape { double r = 1; };

Ptr<const Shape> shape = Make<Circle>();
Ptr circle = DynamicCast<const Circle>(shape);   // null for another Shape
Ptr mutableShape = ConstCast<Shape>(shape);
Ptr known = StaticCast<Circle>(mutableShape);
assert(circle && known->r == 1);
```

### operator<<

```cpp
template<class T> std::ostream& operator<<(std::ostream& s, const Ptr<T>& p);
```

Prints the address, as `s << p.Get()`.

### std::hash

```cpp
template<class T> struct std::hash<Ptr<T>>;
```

The hash of the address, `std::hash<T*>`. A `Ptr` can be the key of a `Dictionary` or a `HashSet` (not of a `std` one, where it would live in unmanaged memory).

```cpp
Ptr item = Make<int>(1);
HashSet<Ptr<int>> seen;
seen.Add(item);
assert(seen.Contains(item));
```

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <cassert>
#include <iostream>

struct Shape {
    virtual ~Shape() = default;
    virtual double Area() const = 0;
};

struct Circle : Shape {
    explicit Circle(double r) : r(r) {}
    double Area() const override { return 3.14159 * r * r; }
    double r;
};

struct Node {
    explicit Node(int id) : id(id) {}
    ~Node() {
        // A destructor runs on a collector thread; its Ptr members are
        // read through IfAlive() only: a copy when the target lives, null
        // when it dies in the same sweep.
        if (auto p = next.IfAlive()) {
            p->id = -1;
        }
    }
    int id;
    Ptr<Node> next;
};

int main() {
    // A ring of three nodes: a cycle, collected like anything else
    Ptr a = Make<Node>(1);   // Ptr<Node>, deduced
    a->next = Make<Node>(2);
    a->next->next = Make<Node>(3);
    a->next->next->next = a;
    Ptr b = a->next;                  // a second root into the ring
    a = nullptr;                               // the ring lives on through b
    assert(b->next->next->next == b);          // three steps around the ring

    // A base class and the dynamic type
    Ptr<Shape> shape = Make<Circle>(2);
    std::cout << "area " << shape->Area() << '\n';
    if (shape.Is<Circle>()) {
        Ptr circle = shape.As<Circle>();   // Ptr<Circle>
        std::cout << "radius " << circle->r << '\n';
    }

    // An alias into a member keeps the whole object
    Ptr id(&b->id);                   // Ptr<int>
    b = nullptr;
    std::cout << "id " << *id << '\n';         // 2: the ring is still alive

    id = nullptr;                              // nothing reaches the ring now
    Collector::Collect(true);         // optional, for the demonstration only: the collector runs its cycles by itself
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

- [UniquePtr](UniquePtr.md), [Make](Make.md), [RootPtr](RootPtr.md), [WeakPtr](WeakPtr.md)
- [Atomic](../Concurrent/Atomic.md), [AtomicRef](../Concurrent/AtomicRef.md) for a `Ptr` shared between threads
- [Collector](Collector.md) for `Collect` and the counting functions
- README: [The classes](../../core/README.md#the-classes), [Pointer aliases](../../core/README.md#pointer-aliases), [The rules](../../core/README.md#the-rules), [Pointer maps](../../../garbage_collector/overview.md#pointer-maps), [Stack roots](../../../garbage_collector/overview.md#stack-roots), [Threads](../../async/README.md#threads)
