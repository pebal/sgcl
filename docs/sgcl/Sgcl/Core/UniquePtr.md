# Sgcl::UniquePtr

```cpp
#include "sgcl/Sgcl/Core/Ptr.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class T>
    class UniquePtr;
}
```

The same class in the `sgcl` interface: [unique_ptr](../../core/unique_ptr.md).

`UniquePtr<T>` is what [`Make<T>(...)`](Make.md) returns: a `std::unique_ptr` in spirit, whose object lives on the managed heap and whose deleter destroys it there. It behaves like any `std::unique_ptr`: sole owner, move-only, the object destroyed at scope exit, deterministically, on the thread that drops it. What it adds is the collector's side of the deal: while a `UniquePtr` owns an object, that object is a root, and so is everything reachable from it; and the `UniquePtr` converts into a [`Ptr`](Ptr.md), after which the object belongs to the collector, destroyed when nothing reaches it any more.

`Get()`, `Release()`, `Reset()`, `Swap()`, `operator*`, `operator->`, `operator bool`, the comparisons and `std::hash` are the standard ones under the interface's names. On top it has the dynamic type of its object (`Type()`, `Is<U>()`, `As<U>()`), a conversion to `UniquePtr<void>&`, and the three pointer casts as free functions.

## Rules

- A `UniquePtr` may live anywhere: on a stack, inside a managed object, in `new`/`malloc` memory, a `std` container, a global or a `thread_local`. The object it owns is a root by its state, wherever its owner is. A global root is a `UniquePtr` ([The rules](../../core/README.md#the-rules)).
- The object a `UniquePtr` owns may not be addressed by a `Ptr` or a [`WeakPtr`](WeakPtr.md): the owner's delete would leave them dangling. Debug builds assert it. Hand the object to the collector first (move the `UniquePtr` into a `Ptr`), then take as many pointers as needed ([The rules](../../core/README.md#the-rules), 4).
- The destructor destroys the object at once, on the calling thread, like `std::unique_ptr`. A `UniquePtr` member of a managed object is destroyed with the object, wherever that happens, on a stack or in a sweep on a collector thread; a destructor may use its `UniquePtr` members freely, unlike its `Ptr` members ([The rules](../../core/README.md#the-rules), 5).
- The `Ptr` members of the owned object follow the rules of `Ptr`: the object is on the managed heap, so they may live in it, and the collector traces them for as long as the owner lives.
- Thread safety is that of `std::unique_ptr`: one thread at a time, or the program's own synchronization.
- Where it belongs: at the edge of the managed world, holding an object from unmanaged memory or for the moment between `Make` and the `Ptr` that takes the object. Inside a managed object it buys only a deterministic destructor for the sub-object, and it costs what a `Ptr` member does not: the owned object is a root the collector finds by its state in every cycle, and the owner's word is one the marking visits. A structure of managed objects is held by `Ptr`s; a million `UniquePtr` members is a million roots, and a full cycle pays for each.
- There is no `UniquePtr<T[]>`: managed arrays belong to the containers ([List](../Containers/List.md), [Array](../Containers/Array.md)).

## Members

### Types

```cpp
using ElementType = T;
using InnerType = sgcl::unique_ptr<T>;
```

### Constructors

```cpp
UniquePtr() noexcept;
UniquePtr(std::nullptr_t) noexcept;
UniquePtr(UniquePtr&& p) noexcept;
template<class U> requires std::is_convertible_v<U*, T*> UniquePtr(UniquePtr<U>&& p) noexcept;
explicit UniquePtr(InnerType&& p) noexcept;
```

The default and the `nullptr` constructors make an empty pointer. The move constructors take the object over, from a `UniquePtr<T>` or from a `UniquePtr<U>` with `U*` convertible to `T*` (a base class, or `void`); the source is null afterwards. There is no constructor from a raw pointer: an object enters a `UniquePtr` through `Make` only.

```cpp
struct Base { virtual ~Base() = default; };
struct Item : Base { int value = 7; };

UniquePtr item = Make<Item>();       // UniquePtr<Item>
UniquePtr<Base> base = std::move(item);       // the Item, as its base; item is null now
UniquePtr<Item> none;                         // empty
assert(!item && base && !none);
```

### Destructor

```cpp
~UniquePtr();
```

Destroys the owned object, if any, at once and on the calling thread: its destructor runs, and its slot goes back to the managed heap. Anything the object reached through `Ptr`s lives on if something else reaches it, and becomes garbage otherwise.

### operator=

```cpp
UniquePtr& operator=(UniquePtr&& p) noexcept;
template<class U> requires std::is_convertible_v<U*, T*> UniquePtr& operator=(UniquePtr<U>&& p) noexcept;
UniquePtr& operator=(std::nullptr_t) noexcept;
```

The assignments of `std::unique_ptr`: the current object, if any, is destroyed, the new one taken over (a `UniquePtr<U>` with `U*` convertible to `T*` converts), or the pointer left empty for `nullptr`.

```cpp
UniquePtr number = Make<int>(1);
number = Make<int>(2);     // the 1 is destroyed here
number = nullptr;                   // the 2 is destroyed here
```

### operator UniquePtr<void>&

```cpp
operator UniquePtr<void>&() noexcept;
operator const UniquePtr<void>&() const noexcept;
```

Every `UniquePtr<T>` is a `UniquePtr<void>&`: the same owner seen without its type. `Type()`, `Is<U>()` and `As<U>()` still work on it.

```cpp
UniquePtr number = Make<int>(1);
UniquePtr<void>& any = number;
assert(any.Is<int>());
```

### Get, Release, Reset, Swap, operator*, operator->, operator bool

```cpp
T* Get() const noexcept;
T* Release() noexcept;
void Reset() noexcept;
void Reset(T* p) noexcept;
void Swap(UniquePtr& p) noexcept;
T& operator*() const;              // not for UniquePtr<void>
T* operator->() const noexcept;
explicit operator bool() const noexcept;
```

The interface of `std::unique_ptr`, unchanged. `Get()` is the raw pointer, valid while the `UniquePtr` owns the object. `Release()` hands the raw pointer out and leaves the `UniquePtr` empty: the object is then owned by nobody and is not tracked either, so `Release()` is for handing the object to something that will own it (a `Ptr` does it for you; see its constructor from a `UniquePtr&&`). `Reset()` destroys the object; `Reset(p)` with a raw pointer to a managed object that nothing owns takes it over. `operator*` and `operator->` are not for `UniquePtr<void>`.

```cpp
struct Point { int x, y; };
UniquePtr p = Make<Point>(1, 2);
p->x = (*p).y;
Point* raw = p.Get();       // valid while p owns the Point
assert(raw->x == 2 && p);
p.Reset();                  // the Point is destroyed here
assert(!p);
```

### Is, As, Type

```cpp
template<class U> bool Is() const noexcept;
template<class U> UniquePtr<U> As() noexcept;
const std::type_info& Type() const noexcept;
```

The dynamic type of the object, read from the metadata of its page, without virtual functions. `Type()` is the `std::type_info` of the type the object was created with (`Make<U>`), whatever the pointer's `T`; for an empty pointer it is `typeid(T)`. `Is<U>()` is `Type() == typeid(U)`: an exact match, not an "is derived from" test. `As<U>()` moves the object into a `UniquePtr<U>` when `Is<U>()`, leaving this pointer empty; when the type does not match it returns null and keeps the object.

```cpp
struct Shape { virtual ~Shape() = default; };
struct Circle : Shape { double r = 1; };
struct Square : Shape { double a = 1; };

UniquePtr<Shape> shape = Make<Circle>();
assert(shape.Type() == typeid(Circle));
assert(shape.Is<Circle>() && !shape.Is<Shape>());
UniquePtr square = shape.As<Square>();     // null: not a Square, shape keeps the object
UniquePtr circle = shape.As<Circle>();     // UniquePtr<Circle>: the object moved, shape is empty
assert(!square && circle && !shape);
```

### Inner

```cpp
InnerType& Inner() noexcept;
const InnerType& Inner() const noexcept;
```

The owner inside, as its own type.

### Comparisons, std::hash

```cpp
template<class T, class U> bool operator==(const UniquePtr<T>& l, const UniquePtr<U>& r) noexcept;
template<class T, class U> std::strong_ordering operator<=>(const UniquePtr<T>& l, const UniquePtr<U>& r) noexcept;
template<class T> bool operator==(const UniquePtr<T>& l, std::nullptr_t) noexcept;
template<class T> std::strong_ordering operator<=>(const UniquePtr<T>& l, std::nullptr_t) noexcept;
template<class T> struct std::hash<UniquePtr<T>>;
```

The comparisons of `std::unique_ptr`, on the addresses. `std::hash<UniquePtr<T>>` is the hash of the address, `std::hash<T*>`.

```cpp
UniquePtr a = Make<int>(1);
UniquePtr b = Make<int>(1);
assert(a != b && a != nullptr && nullptr < a);
size_t h = std::hash<UniquePtr<int>>{}(a);
(void)h;
```

### StaticCast, ConstCast, DynamicCast

```cpp
template<class T, class U> UniquePtr<T> StaticCast(UniquePtr<U>&& r) noexcept;
template<class T, class U> UniquePtr<T> ConstCast(UniquePtr<U>&& r) noexcept;
template<class T, class U> UniquePtr<T> DynamicCast(UniquePtr<U>&& r) noexcept;
```

The casts, on an rvalue: the object is released from `r` and owned by the result. A failed `dynamic_cast` returns null, and the object is lost with it (it was released before the cast): test with `Is<U>()` or `As<U>()` first when the type is in doubt.

```cpp
struct Shape { virtual ~Shape() = default; };
struct Circle : Shape { double r = 1; };

UniquePtr<const Shape> shape = Make<Circle>();
UniquePtr mutableShape = ConstCast<Shape>(std::move(shape));          // UniquePtr<Shape>
UniquePtr circle = DynamicCast<Circle>(std::move(mutableShape));      // UniquePtr<Circle>
UniquePtr<Shape> back = StaticCast<Shape>(std::move(circle));
assert(!shape && !mutableShape && !circle && back);
```

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <cassert>
#include <iostream>

struct Node {
    explicit Node(int id) : id(id) {}
    ~Node() { std::cout << "Node " << id << " destroyed\n"; }
    int id;
    Ptr<Node> next;
};

// A registry that owns its root: a UniquePtr may live in a global, and
// everything reachable from the Node it owns lives with it.
static UniquePtr registry = Make<Node>(0);

// A helper that creates and returns an object: the caller decides whether
// it stays deterministic (kept as a UniquePtr) or goes to the collector
// (moved into a Ptr).
UniquePtr<Node> MakeNode(int id) {
    return Make<Node>(id);
}

int main() {
    {
        UniquePtr scoped = MakeNode(1);   // UniquePtr<Node>
        scoped->next = MakeNode(2);                // the 2 belongs to the collector, rooted by the 1
        assert(scoped.Is<Node>());
    }   // "Node 1 destroyed", here and now; the 2 is garbage, for the collector

    Ptr shared = MakeNode(3);             // the 3 belongs to the collector
    registry->next = shared;                       // and is reachable from the global root
    shared = nullptr;                              // still alive: the registry keeps it

    Collector::Collect(true);             // optional, for the demonstration only: "Node 2 destroyed", on a collector thread
    std::cout << "registry -> " << registry->next->id << '\n';   // 3
    return 0;
}
```

The output:

```
Node 1 destroyed
Node 2 destroyed
registry -> 3
Node 0 destroyed
```

## See also

- [Make](Make.md), [Ptr](Ptr.md), [RootPtr](RootPtr.md), [WeakPtr](WeakPtr.md)
- README: [The classes](../../core/README.md#the-classes), [make_tracked](../../core/README.md#make_tracked), [The rules](../../core/README.md#the-rules), [Pointer maps](../../../garbage_collector/overview.md#pointer-maps)
