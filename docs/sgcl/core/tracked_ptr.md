# sgcl::tracked_ptr

```cpp
#include "sgcl/core/tracked_ptr.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class T>
    class tracked_ptr;
}
```

`tracked_ptr<T>` is the pointer the collector follows. It is one word, the address of the object; copying it is a store of that word and the write barrier, a byte of state on the target. There is no reference count and no control block: an object lives as long as some `tracked_ptr` in a live object or on a stack, some [`unique_ptr`](unique_ptr.md), or a word on a stack reaches it, cycles included, and the collector destroys it in a later cycle, on a collector thread, when nothing does. What `std::shared_ptr` does by counting, `tracked_ptr` leaves to the collector, and what `std::shared_ptr` cannot do, a cycle of objects, needs nothing special.

The object comes from [`make_tracked`](make_tracked.md) as a `unique_ptr`; moving that into a `tracked_ptr` hands the object to the collector. A `tracked_ptr` converts to a base class, points into the middle of an object (a member, a base subobject: an alias that keeps the whole object), knows the dynamic type of its object without virtual functions (`type()`, `is<U>()`, `as<U>()`), and `tracked_ptr<void>` holds any of them. A move is a copy: the moved-from pointer keeps its value.

## Rules

- A `tracked_ptr` lives inside a managed object (one created with `make_tracked`, a node or buffer of an `sgcl` container, a managed coroutine frame) or on a thread's stack. Never in `new`/`malloc` memory, a `std` container, a global, a `thread_local`, a lambda copied to the heap, or the frame of a plain coroutine. Debug builds assert it in the constructor; a release build loses the object ([The rules](README.md#the-rules), 1; [Stack roots](../../garbage_collector/overview.md#stack-roots)). A global root is a `unique_ptr`, to the object or to a managed object holding the `tracked_ptr`.
- The object held from unmanaged memory is a `std::shared_ptr` from `to_shared()` (below): a `unique_ptr` for one owner, a `shared_ptr` for many, a `tracked_ptr` for neither.
- It does not share its storage with data: no `union` with a value, no `std::variant`, no small-buffer `std::function` or `std::any` holding one; [variant](variant.md), [any](any.md), [function](function.md) and [expected](expected.md) are the ones that keep it apart. A union of two `tracked_ptr`s and `std::optional<tracked_ptr<T>>` are fine ([Pointer maps](../../garbage_collector/overview.md#pointer-maps)).
- It addresses a managed object or a part of it, a member or a base ([Pointer aliases](README.md#pointer-aliases)); never an element of a container's buffer (`sgcl::vector`, `sgcl::dynamic_array<T>`), and never an object a `unique_ptr` owns. Debug builds assert both.
- A destructor reads the `tracked_ptr` members of its object only through `if_alive()`: the object dies with everything reachable only from it, in no particular order, on a collector thread ([Pointer maps](../../garbage_collector/overview.md#pointer-maps), [Threads](../async/README.md#threads)).
- A `tracked_ptr` written by one thread and read by another needs [`atomic`](atomic.md) or [`atomic_ref`](atomic_ref.md), or the program's own synchronization. The word itself is atomic: a race is never a torn pointer, and the collector is correct under any interleaving ([The rules](README.md#the-rules), 6).
- `tracked_ptr<T[]>` is declared but not defined: managed arrays belong to the containers ([vector](vector.md), [array](array.md)).

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
tracked_ptr(std::nullptr_t, detail::unregistered_t) noexcept;   // null, without registering the thread

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

template<class U, std::enable_if_t<!std::is_same_v<U, T> && std::is_convertible_v<U*, element_type*>, int> = 0>
tracked_ptr(const root_ptr<U>& r) noexcept;   // a root_ptr of a derived class or of T made const; root_ptr<T> converts by its own operator
```

The default and the `nullptr` constructors make a null pointer. The raw-pointer constructor is explicit and takes the address of a managed object or of a part of it, a member or a base subobject: the alias keeps the whole object alive. Copies and moves from a `tracked_ptr<U>` with `U*` convertible to `T*` convert to the base class (or to `void`); a move is a copy, the source keeps its value. The constructor from a `unique_ptr<U>&&` releases the object from its owner: from then on the collector destroys it, when nothing reaches it any more.

Every constructor registers the calling thread with the collector on first contact (one thread-local load), stores the word with the barrier and checks, in a debug build, that the pointer lives where the rules allow and addresses what they allow. The one exception is `tracked_ptr(nullptr, detail::unregistered)`: a null pointer made without the thread-local load, for a type that holds a `tracked_ptr` it may never use — a [slice](slice.md) over unmanaged memory has no owner, and a thread that only makes such slices never touches the collector. A pointer so made is stored to and copied as any other; the store of a non-null value registers the thread then.

```cpp
struct Base { virtual ~Base() = default; };
struct Item : Base { int value = 7; };

tracked_ptr item = make_tracked<Item>();   // from a unique_ptr: the collector owns the Item now
tracked_ptr<Base> base = item;                    // a base class
tracked_ptr alias(&item->value);                  // tracked_ptr<int>, into the Item
tracked_ptr<Item> none;                           // null
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
struct Node { tracked_ptr<Node> next; };

tracked_ptr head = make_tracked<Node>();
head->next = make_tracked<Node>();   // from a unique_ptr
tracked_ptr<Node> second;
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
tracked_ptr number = make_tracked<int>(1);
tracked_ptr<void>& ref = number;    // the same word
tracked_ptr<void> any = number;     // a copy
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
tracked_ptr p = make_tracked<Point>(1, 2);
p->x = (*p).y;
```

### get

```cpp
element_type* get() const noexcept;
```

The raw pointer. A raw pointer keeps nothing alive by itself: it is valid while a `tracked_ptr`, a `unique_ptr` or a container keeps its target, as with `std` ([The rules](README.md#the-rules), 3). A raw pointer that stays in a stack frame does keep its target until the word is overwritten, since the stack is scanned conservatively; that is a delay, not a guarantee ([Stack roots](../../garbage_collector/overview.md#stack-roots)).

```cpp
struct Item { int value = 1; };
tracked_ptr item = make_tracked<Item>();
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
    tracked_ptr<Node> peer;
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
tracked_ptr item = make_tracked<Item>();
tracked_ptr<int> alias;
alias.reset(&item->value);   // an alias into the Item
alias.reset();               // null
```

### swap

```cpp
void swap(tracked_ptr& p) noexcept;
```

Exchanges the two pointers, through a temporary on the stack. There is no free `swap` for `tracked_ptr`; `std::swap` works through the move operations.

```cpp
tracked_ptr a = make_tracked<int>(1);
tracked_ptr b = make_tracked<int>(2);
a.swap(b);
assert(*a == 2 && *b == 1);
```

### shade, store(p, barrier::off)

```cpp
struct barrier { struct off_t {}; static constexpr off_t off; };   // sgcl::barrier (types.h): the tag of the store without the write barrier
void shade() const noexcept;                                     // the write barrier for the target, on demand
void store(const tracked_ptr& p, barrier::off_t) noexcept;       // the word alone, relaxed, no barrier: an overload of its own
tracked_ptr(const tracked_ptr& p, barrier::off_t) noexcept;      // the same as a constructor
```

For an immutable structure copying one of its nodes ([im](../immutable/README.md)). The write barrier's promise is that whatever a pointer is stored to is reachable in the current cycle; a node that never changes holds exactly the words its copy holds, so the copy may take the words without the barrier — `dst.store(src, barrier::off)`, a relaxed store of the word, one word at a time (the collector may read the copy meanwhile: a word, never a torn vector store) — and then, the copy complete, one `shade()` of a pointer to the source makes the source reachable in this cycle, and the marking, visiting it, marks every child the copy holds: one barrier for the node in place of one per word. Two rules: the source is held by the caller through the copy and the shade (the version being copied holds it), and the shade comes **after** the copy is complete, never before (the copies of `im` made with a shade of the root ahead of them lost nodes). `shade()` is `_update` of the pointer's target: the state and the card, as a store of the pointer would leave them; on a pointer just made from a raw address it is the barrier that construction ran, once more.

```cpp
struct branch {
    tracked_ptr<void> children[32];
    static unique_ptr<branch> make(const branch& from) {   // the copy, then the shade
        unique_ptr<branch> copy = make_tracked<branch>(from);
        ((tracked_ptr<const branch>)&from).shade();
        return copy;
    }
    branch(const branch& o) noexcept {
        for (auto i : range(32)) children[i].store(o.children[i], barrier::off);
    }
};
```

### to_shared

```cpp
std::shared_ptr<element_type> to_shared() const;
```

The object held from unmanaged memory. Returns a `std::shared_ptr` to the object whose control block owns a managed holder of this pointer: the holder is a root (its `unique_ptr` lives in the control block), so the object is reachable for as long as any copy of the `shared_ptr` lives, and the `shared_ptr` itself may live anywhere a `tracked_ptr` may not: `new` memory, a `std` container, a global, a lambda copied to the heap or run on another thread. Copies of the `shared_ptr` share the control block and the holder; only the call allocates, twice (the holder on the managed heap, the control block on the unmanaged one), which is why this is a named function and not a conversion. A null pointer gives a null `shared_ptr`. An alias into a member gives a `shared_ptr` to the member that keeps the whole object.

The object stays managed: when the last `shared_ptr` is gone the holder is released and the object lives on if anything else reaches it, and is destroyed on a collector thread, under the rules of destructors, once nothing does. A `shared_ptr` from `to_shared()` is not a deterministic owner the way `unique_ptr` is.

```cpp
struct Node { int value = 7; tracked_ptr<Node> next; };

std::vector<std::shared_ptr<Node>> kept;           // a std container: no tracked_ptr may live in it
tracked_ptr node = make_tracked<Node>();
node->next = make_tracked<Node>();
kept.push_back(node.to_shared());                  // the Node and its next live while the shared_ptr does
node = nullptr;
collector::force_collect();                  // optional, to show the result at once
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

tracked_ptr<Shape> shape = make_tracked<Circle>();
assert(shape.type() == typeid(Circle));
assert(shape.is<Circle>() && !shape.is<Shape>());
if (tracked_ptr circle = shape.as<Circle>()) {   // tracked_ptr<Circle>
    circle->r = 2;
}
tracked_ptr radius(&shape.as<Circle>()->r);      // an alias into the Circle
assert(radius.is<Circle>());                            // the object's type, not the alias's
assert(radius.as<Circle>() == shape);                   // back to the whole object
```

### Deduction guides

```cpp
template<typename T> tracked_ptr(T*) -> tracked_ptr<T>;
template<typename T> tracked_ptr(tracked_ptr<T>) -> tracked_ptr<T>;
template<typename T> tracked_ptr(unique_ptr<T>&&) -> tracked_ptr<T>;
template<class T> tracked_ptr(const root_ptr<T>&) -> tracked_ptr<T>;
```

`sgcl::tracked_ptr p = sgcl::make_tracked<T>(...)` is a `tracked_ptr<T>`; the explicit argument is needed only where the deduction would pick another type, a base class or a null initializer, or for a member declaration.

```cpp
struct Item { int value; };
tracked_ptr item = make_tracked<Item>(4);   // tracked_ptr<Item>
tracked_ptr copy = item;                          // tracked_ptr<Item>
tracked_ptr value(&item->value);                  // tracked_ptr<int>
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
tracked_ptr a = make_tracked<int>(1);
tracked_ptr b = a;
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

tracked_ptr<const Shape> shape = make_tracked<Circle>();
tracked_ptr circle = dynamic_pointer_cast<const Circle>(shape);   // null for another Shape
tracked_ptr mutable_shape = const_pointer_cast<Shape>(shape);
tracked_ptr known = static_pointer_cast<Circle>(mutable_shape);
assert(circle && known->r == 1);
```

### operator<<

```cpp
template<class T> std::ostream& operator<<(std::ostream& s, const tracked_ptr<T>& p);
```

Prints the address, as `s << p.get()`.

### std::hash

```cpp
template<class T> struct std::hash<tracked_ptr<T>>;
```

The hash of the address, `std::hash<T*>`. A `tracked_ptr` can be the key of an `sgcl::map` or `set` (not of a `std` one, where it would live in unmanaged memory).

```cpp
tracked_ptr item = make_tracked<int>(1);
set<tracked_ptr<int>> seen;
seen.insert(item);
assert(seen.contains(item));
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <cassert>
#include <iostream>

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
    ~Node() {
        // A destructor runs on a collector thread; its tracked_ptr members
        // are read through if_alive() only: a copy when the target lives,
        // null when it dies in the same sweep.
        if (auto p = next.if_alive()) {
            p->id = -1;
        }
    }
    int id;
    tracked_ptr<Node> next;
};

int main() {
    // A ring of three nodes: a cycle, collected like anything else
    tracked_ptr a = make_tracked<Node>(1);   // tracked_ptr<Node>, deduced
    a->next = make_tracked<Node>(2);
    a->next->next = make_tracked<Node>(3);
    a->next->next->next = a;
    tracked_ptr b = a->next;                       // a second root into the ring
    a = nullptr;                                         // the ring lives on through b
    assert(b->next->next->next == b);                    // three steps around the ring

    // A base class and the dynamic type
    tracked_ptr<Shape> shape = make_tracked<Circle>(2);
    std::cout << "area " << shape->area() << '\n';
    if (shape.is<Circle>()) {
        tracked_ptr circle = shape.as<Circle>();   // tracked_ptr<Circle>
        std::cout << "radius " << circle->r << '\n';
    }

    // An alias into a member keeps the whole object
    tracked_ptr id(&b->id);                        // tracked_ptr<int>
    b = nullptr;
    std::cout << "id " << *id << '\n';                   // 2: the ring is still alive

    id = nullptr;                                        // nothing reaches the ring now
    collector::force_collect(true);                // optional, for the demonstration only: the collector runs its cycles by itself
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
- [atomic](atomic.md), [atomic_ref](atomic_ref.md) for a `tracked_ptr` shared between threads
- [collector](collector.md) for `force_collect` and the counting functions
- README: [The classes](README.md#the-classes), [Pointer aliases](README.md#pointer-aliases), [The rules](README.md#the-rules), [Pointer maps](../../garbage_collector/overview.md#pointer-maps), [Stack roots](../../garbage_collector/overview.md#stack-roots), [Threads](../async/README.md#threads)
- `examples/example.cpp`, `examples/threads.cpp`
