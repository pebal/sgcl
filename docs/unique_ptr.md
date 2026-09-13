# sgcl::unique_ptr

```cpp
#include "sgcl/unique_ptr.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class T>
    class unique_ptr : public std::unique_ptr<T, detail::UniqueDeleter>;
}
```

`unique_ptr<T>` is what [`make_tracked<T>(...)`](make_tracked.md) returns: a `std::unique_ptr` whose object lives on the managed heap and whose deleter destroys it there. It behaves like any `std::unique_ptr`: sole owner, move-only, the object destroyed at scope exit, deterministically, on the thread that drops it. What it adds is the collector's side of the deal: while a `unique_ptr` owns an object, that object is a root, and so is everything reachable from it; and the `unique_ptr` converts into a [`tracked_ptr`](tracked_ptr.md), after which the object belongs to the collector, destroyed when nothing reaches it any more.

The class derives from `std::unique_ptr<T, detail::UniqueDeleter>`, so `get()`, `release()`, `reset()`, `swap()`, `operator*`, `operator->`, `operator bool`, the comparisons and `std::hash` are the standard ones. On top it has the dynamic type of its object (`type()`, `is<U>()`, `as<U>()`), a conversion to `unique_ptr<void>&`, and the three pointer casts as free functions.

## Rules

- A `unique_ptr` may live anywhere: on a stack, inside a managed object, in `new`/`malloc` memory, a `std` container, a global or a `thread_local`. The object it owns is a root by its state, wherever its owner is. A global root is a `unique_ptr` ([The rules](../README.md#the-rules)).
- The object a `unique_ptr` owns may not be addressed by a `tracked_ptr` or a [`weak_ptr`](weak_ptr.md): the owner's delete would leave them dangling. Debug builds assert it. Hand the object to the collector first (move the `unique_ptr` into a `tracked_ptr`), then take as many pointers as needed ([The rules](../README.md#the-rules), 4).
- The destructor destroys the object at once, on the calling thread, like `std::unique_ptr`. A `unique_ptr` member of a managed object is destroyed with the object, wherever that happens, on a stack or in a sweep on a collector thread; a destructor may use its `unique_ptr` members freely, unlike its `tracked_ptr` members ([The rules](../README.md#the-rules), 5).
- The `tracked_ptr` members of the owned object follow the rules of `tracked_ptr`: the object is on the managed heap, so they may live in it, and the collector traces them for as long as the owner lives.
- Thread safety is that of `std::unique_ptr`: one thread at a time, or the program's own synchronization.
- `unique_ptr<T[]>` is declared but not defined: managed arrays belong to the containers ([vector](vector.md), [array](array.md)).

## Members

### Types

```cpp
using element_type = T;
using pointer = T*;                          // from std::unique_ptr
using deleter_type = detail::UniqueDeleter;  // from std::unique_ptr
```

### Constructors

```cpp
unique_ptr() = default;
constexpr unique_ptr(std::nullptr_t) noexcept;
unique_ptr(unique_ptr&& p) noexcept;         // implicit, from std::unique_ptr
template<class U, std::enable_if_t<std::is_convertible_v<typename unique_ptr<U>::element_type*, element_type*>, int> = 0>
unique_ptr(detail::UniquePtr<U>&& p) noexcept;
```

The default and the `nullptr` constructors make an empty pointer. The move constructors take the object over, from a `unique_ptr<T>` or from a `unique_ptr<U>` with `U*` convertible to `T*` (a base class, or `void`); the source is null afterwards. There is no constructor from a raw pointer: an object enters a `unique_ptr` through `make_tracked` only.

```cpp
struct Base { virtual ~Base() = default; };
struct Item : Base { int value = 7; };

sgcl::unique_ptr item = sgcl::make_tracked<Item>();   // unique_ptr<Item>
sgcl::unique_ptr<Base> base = std::move(item);         // the Item, as its base; item is null now
sgcl::unique_ptr<Item> none;                           // empty
assert(!item && base && !none);
```

### Destructor

```cpp
~unique_ptr();   // from std::unique_ptr
```

Destroys the owned object, if any, at once and on the calling thread: its destructor runs, and its slot goes back to the managed heap. Anything the object reached through `tracked_ptr`s lives on if something else reaches it, and becomes garbage otherwise.

### operator=

```cpp
unique_ptr& operator=(unique_ptr&& p) noexcept;                                    // implicit
template<class U, class E>
std::unique_ptr<T, deleter_type>& operator=(std::unique_ptr<U, E>&& p) noexcept;   // from std::unique_ptr, U* convertible to T*
std::unique_ptr<T, deleter_type>& operator=(std::nullptr_t) noexcept;              // from std::unique_ptr
```

The assignments of `std::unique_ptr`: the current object, if any, is destroyed, the new one taken over (a `unique_ptr<U>` with `U*` convertible to `T*` converts), or the pointer left empty for `nullptr`.

```cpp
sgcl::unique_ptr number = sgcl::make_tracked<int>(1);
number = sgcl::make_tracked<int>(2);   // the 1 is destroyed here
number = nullptr;                      // the 2 is destroyed here
```

### operator unique_ptr<void>&

```cpp
operator unique_ptr<void>&() noexcept;
operator const unique_ptr<void>&() const noexcept;
```

Every `unique_ptr<T>` is a `unique_ptr<void>&`: the same owner seen without its type. `type()`, `is<U>()` and `as<U>()` still work on it.

```cpp
sgcl::unique_ptr number = sgcl::make_tracked<int>(1);
sgcl::unique_ptr<void>& any = number;
assert(any.is<int>());
```

### get, release, reset, swap, operator*, operator->, operator bool

```cpp
pointer get() const noexcept;                 // from std::unique_ptr
pointer release() noexcept;
void reset(pointer p = pointer()) noexcept;
void swap(unique_ptr& p) noexcept;
T& operator*() const;
pointer operator->() const noexcept;
explicit operator bool() const noexcept;
```

The interface of `std::unique_ptr`, unchanged. `get()` is the raw pointer, valid while the `unique_ptr` owns the object. `release()` hands the raw pointer out and leaves the `unique_ptr` empty: the object is then owned by nobody and is not tracked either, so `release()` is for handing the object to something that will own it (a `tracked_ptr` does it for you; see its constructor from a `unique_ptr&&`). `reset()` destroys the object; `reset(p)` with a raw pointer to a managed object that nothing owns takes it over. `operator*` and `operator->` are not for `unique_ptr<void>`.

```cpp
struct Point { int x, y; };
sgcl::unique_ptr p = sgcl::make_tracked<Point>(1, 2);
p->x = (*p).y;
Point* raw = p.get();       // valid while p owns the Point
assert(raw->x == 2 && p);
p.reset();                  // the Point is destroyed here
assert(!p);
```

### is, as, type

```cpp
template<class U>
bool is() const noexcept;

template<class U>
unique_ptr<U> as() noexcept;

const std::type_info& type() const noexcept;
```

The dynamic type of the object, read from the metadata of its page, without virtual functions. `type()` is the `std::type_info` of the type the object was created with (`make_tracked<U>`), whatever the pointer's `T`; for an empty pointer it is `typeid(T)`. `is<U>()` is `type() == typeid(U)`: an exact match, not an "is derived from" test. `as<U>()` moves the object into a `unique_ptr<U>` when `is<U>()`, leaving this pointer empty; when the type does not match it returns null and keeps the object.

```cpp
struct Shape { virtual ~Shape() = default; };
struct Circle : Shape { double r = 1; };
struct Square : Shape { double a = 1; };

sgcl::unique_ptr<Shape> shape = sgcl::make_tracked<Circle>();
assert(shape.type() == typeid(Circle));
assert(shape.is<Circle>() && !shape.is<Shape>());
sgcl::unique_ptr square = shape.as<Square>();   // null: not a Square, shape keeps the object
sgcl::unique_ptr circle = shape.as<Circle>();   // unique_ptr<Circle>: the object moved, shape is empty
assert(!square && circle && !shape);
```

### Comparisons, std::hash

```cpp
// from std::unique_ptr: ==, !=, <, <=, >, >=, <=> between two unique_ptrs and with nullptr
template<class T> struct std::hash<sgcl::unique_ptr<T>>;
```

The comparisons of `std::unique_ptr`, on the addresses. `std::hash<sgcl::unique_ptr<T>>` is the hash of the address, `std::hash<T*>`.

```cpp
sgcl::unique_ptr a = sgcl::make_tracked<int>(1);
sgcl::unique_ptr b = sgcl::make_tracked<int>(1);
assert(a != b && a != nullptr && nullptr < a);
size_t h = std::hash<sgcl::unique_ptr<int>>{}(a);
(void)h;
```

### static_pointer_cast, const_pointer_cast, dynamic_pointer_cast

```cpp
template<class T, class U> unique_ptr<T> static_pointer_cast(unique_ptr<U>&& r) noexcept;
template<class T, class U> unique_ptr<T> const_pointer_cast(unique_ptr<U>&& r) noexcept;
template<class T, class U> unique_ptr<T> dynamic_pointer_cast(unique_ptr<U>&& r) noexcept;
```

The casts, on an rvalue: the object is released from `r` and owned by the result. A failed `dynamic_cast` returns null, and the object is lost with it (it was released before the cast): test with `is<U>()` or `as<U>()` first when the type is in doubt.

```cpp
struct Shape { virtual ~Shape() = default; };
struct Circle : Shape { double r = 1; };

sgcl::unique_ptr<const Shape> shape = sgcl::make_tracked<Circle>();
sgcl::unique_ptr mutable_shape = sgcl::const_pointer_cast<Shape>(std::move(shape));      // unique_ptr<Shape>
sgcl::unique_ptr circle = sgcl::dynamic_pointer_cast<Circle>(std::move(mutable_shape));  // unique_ptr<Circle>
sgcl::unique_ptr<Shape> back = sgcl::static_pointer_cast<Shape>(std::move(circle));
assert(!shape && !mutable_shape && !circle && back);
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <cassert>
#include <iostream>

struct Node {
    explicit Node(int id) : id(id) {}
    ~Node() { std::cout << "Node " << id << " destroyed\n"; }
    int id;
    sgcl::tracked_ptr<Node> next;
};

// A registry that owns its root: a unique_ptr may live in a global, and
// everything reachable from the Node it owns lives with it.
static sgcl::unique_ptr registry = sgcl::make_tracked<Node>(0);

// A helper that creates and returns an object: the caller decides whether
// it stays deterministic (kept as a unique_ptr) or goes to the collector
// (moved into a tracked_ptr).
sgcl::unique_ptr<Node> make_node(int id) {
    return sgcl::make_tracked<Node>(id);
}

int main() {
    {
        sgcl::unique_ptr scoped = make_node(1);   // unique_ptr<Node>
        scoped->next = make_node(2);              // the 2 belongs to the collector, rooted by the 1
        assert(scoped.is<Node>());
    }   // "Node 1 destroyed", here and now; the 2 is garbage, for the collector

    sgcl::tracked_ptr shared = make_node(3);      // the 3 belongs to the collector
    registry->next = shared;                      // and is reachable from the global root
    shared = nullptr;                             // still alive: the registry keeps it

    sgcl::collector::force_collect(true);         // optional, for the demonstration only: "Node 2 destroyed", on a collector thread
    std::cout << "registry -> " << registry->next->id << '\n';   // 3
    return 0;
}
```

## See also

- [make_tracked](make_tracked.md), [tracked_ptr](tracked_ptr.md), [weak_ptr](weak_ptr.md)
- README: [SGCL classes](../README.md#sgcl-classes), [make_tracked](../README.md#make_tracked), [The rules](../README.md#the-rules), [Pointer maps](../README.md#pointer-maps)
- `examples/example.cpp`
