# Sgcl::Make

```cpp
#include "sgcl/Sgcl/Core/Ptr.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class T, class ...A>
    UniquePtr<T> Make(A&&... a);
}
```

The same function in the `sgcl` interface: [make_tracked](../../core/make_tracked.md).

`Make<T>(args...)` creates an object of type `T` on the managed heap, constructed from `args...`, and returns it as a [`UniquePtr<T>`](UniquePtr.md). It is the one way an object enters the managed heap (the containers make their nodes and buffers the same way, internally), and the counterpart of `std::make_unique`: deterministic ownership until the `UniquePtr` is moved into a [`Ptr`](Ptr.md) (or a [`RootPtr`](RootPtr.md), or an [`Atomic`](../Concurrent/Atomic.md)), which any of them does from the expression itself, `Ptr p = Make<T>(...)`, after which the collector owns the object and destroys it when nothing reaches it, or until the `UniquePtr` is dropped, which destroys the object at once.

The allocation is a slot from a thread-local pool of the object's size class: no lock, no wait for the collector, and no header on the object (the metadata, the type included, is the page's). The collector may read the words of the slot while the constructor runs; the words at the type's pointer offsets are null then (a page is zero when it is issued to a type, and every object of the type that died in the slot since left its pointers null: the destructors of `Ptr` and `UniquePtr` store a null) and the constructor's stores land one by one, which is why a `Ptr` member initialized in the constructor is a root from its store on. A constructor that throws gives the slot back and lets the exception through.

## Rules

- `T` is an object type: not an array (`Make<T[]>` is a compile error: managed arrays are not a public type, `List` and `Array<T>` own theirs), not `void`, and not larger than a page (`config::PageSize`, 64 KB, less a 16-byte header; a compile error past that). Large data goes into a container.
- `T` may be `const`; it may hold `Ptr`s, `WeakPtr`s, `UniquePtr`s, containers and atomics, since a managed object is where all of those may live. It may be a `Ptr` itself: `Make<Ptr<T>>()` is the managed word a root from outside the managed world holds ([Stack roots](../../../garbage_collector/overview.md#stack-roots)).
- The constructor of `T` runs on the calling thread, at once; the destructor runs when the owner destroys the object (a `UniquePtr`, on its thread) or when the collector does (a `Ptr`, on a collector thread, under the rules of destructors: [The rules](../../core/README.md#the-rules), 5).
- Any thread may call it; the first managed object a thread creates registers the thread with the collector, and the first one in the program starts the collector ([Threads](../../async/README.md#threads)).
- When the committed memory would cross the ceiling (`Collector::MemoryLimit()`), the call first forces a full collection and throws `std::bad_alloc` if that does not free enough ([Memory](../../../garbage_collector/overview.md#memory)).

## Members

### Make

```cpp
template<class T, class ...A>
UniquePtr<T> Make(A&&... a);
```

Constructs a `T` from `args...` on the managed heap, as `new (slot) T(std::forward<A>(a)...)`, or `new (slot) T` with no arguments (default-initialization, like `std::make_unique_for_overwrite`: a trivial type is left uninitialized: the slot holds whatever the last object of the type left there, null at the pointer offsets). Aggregates take their arguments in parentheses (C++20). Returns a `UniquePtr<T>` owning the object.

```cpp
struct Point { int x, y; };
struct Node { int value; Ptr<Node> next; };

auto number = Make<int>(42);                    // UniquePtr<int>
UniquePtr point = Make<Point>(1, 2);   // an aggregate, in parentheses
Ptr node = Make<Node>(7);              // moved into a Ptr: the collector's from now on
node->next = Make<Node>(8, node);               // a cycle, built from a UniquePtr temporary
UniquePtr owned = Make<Node>();  // default-initialized: next is null
assert(*number == 42 && point->y == 2 && node->next->next == node && !owned->next);
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

struct Label {
    Label(String text, Ptr<Shape> shape) : text(std::move(text)), shape(shape) {}
    String text;
    Ptr<Shape> shape;     // stored by the constructor: a root from that store on
};

int main() {
    // Deterministic: a UniquePtr, destroyed at the end of the scope
    {
        UniquePtr circle = Make<Circle>(1.0);       // UniquePtr<Circle>
        std::cout << "area " << circle->Area() << '\n';
    }   // the Circle is destroyed here, on this thread

    // Collected: the UniquePtr moved into a Ptr of the base class
    Ptr<Shape> shape = Make<Circle>(2.0);
    Ptr label = Make<Label>("big", shape);      // arguments forwarded to the constructor
    shape = nullptr;                                              // the Circle lives on: the Label holds it
    std::cout << label->text << ": area " << label->shape->Area() << '\n';

    // The dynamic type is the one the object was created with
    assert(label->shape.Is<Circle>());

    label = nullptr;                          // nothing reaches the Label or the Circle now
    Collector::Collect(true);        // optional, for the demonstration only: the collector runs its cycles by itself
    return 0;
}
```

The output:

```
area 3.14159
big: area 12.5664
```

## See also

- [UniquePtr](UniquePtr.md), [Ptr](Ptr.md), [RootPtr](RootPtr.md), [WeakPtr](WeakPtr.md)
- [List](../Containers/List.md), [Array](../Containers/Array.md) for managed sequences, which take the place of managed arrays
- [Collector](Collector.md), [config](../../core/config.md) for the memory ceiling and the page size
- README: [make_tracked](../../core/README.md#make_tracked), [The classes](../../core/README.md#the-classes), [The rules](../../core/README.md#the-rules), [Memory](../../../garbage_collector/overview.md#memory)
