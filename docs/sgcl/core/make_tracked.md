# sgcl::make_tracked

```cpp
#include "sgcl/core/make_tracked.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class T, class ...A>
    auto make_tracked(A&&... a);   // -> unique_ptr<T>
}
```

`make_tracked<T>(args...)` creates an object of type `T` on the managed heap, constructed from `args...`, and returns it as a [`unique_ptr<T>`](unique_ptr.md). It is the one way an object enters the managed heap (the containers make their nodes and buffers the same way, internally), and the counterpart of `std::make_unique`: deterministic ownership until the `unique_ptr` is moved into a [`tracked_ptr`](tracked_ptr.md), after which the collector owns the object and destroys it when nothing reaches it, or until the `unique_ptr` is dropped, which destroys the object at once.

The allocation is a slot from a thread-local pool of the object's size class: no lock, no wait for the collector, and no header on the object (the metadata, the type included, is the page's). The collector may read the words of the slot while the constructor runs; the words at the type's pointer offsets are null then (a page is zero when it is issued to a type, and every object of the type that died in the slot since left its pointers null: the destructors of `tracked_ptr` and `unique_ptr` store a null) and the constructor's stores land one by one, which is why a `tracked_ptr` member initialized in the constructor is a root from its store on. A constructor that throws gives the slot back and lets the exception through.

## Rules

- `T` is an object type: not an array (`make_tracked<T[]>` is a compile error: managed arrays are not a public type, `sgcl::vector` and `sgcl::dynamic_array<T>` own theirs), not `void`, and not larger than a page (`config::PageSize`, 64 KB, less a 16-byte header; a compile error past that). Large data goes into a container.
- `T` may be `const`; it may hold `tracked_ptr`s, `weak_ptr`s, `unique_ptr`s, containers and atomics, since a managed object is where all of those may live. It may be a `tracked_ptr` itself: `make_tracked<tracked_ptr<T>>()` is the managed word a root from outside the managed world holds ([Stack roots](../../garbage_collector/overview.md#stack-roots)).
- The constructor of `T` runs on the calling thread, at once; the destructor runs when the owner destroys the object (a `unique_ptr`, on its thread) or when the collector does (a `tracked_ptr`, on a collector thread, under the rules of destructors: [The rules](README.md#the-rules), 5).
- Any thread may call it; the first managed object a thread creates registers the thread with the collector, and the first one in the program starts the collector ([Threads](../async/README.md#threads)).
- When the committed memory would cross the ceiling (`collector::get_memory_limit()`), the call first forces a full collection and throws `std::bad_alloc` if that does not free enough ([Memory](../../garbage_collector/overview.md#memory)).

## Members

### make_tracked

```cpp
template<class T, class ...A>
auto make_tracked(A&&... a);   // -> unique_ptr<T>
```

Constructs a `T` from `args...` on the managed heap, as `new (slot) T(std::forward<A>(a)...)`, or `new (slot) T` with no arguments (default-initialization, like `std::make_unique_for_overwrite`: a trivial type is left uninitialized: the slot holds whatever the last object of the type left there, null at the pointer offsets). Aggregates take their arguments in parentheses (C++20). Returns a `unique_ptr<T>` owning the object.

```cpp
struct Point { int x, y; };
struct Node { int value; tracked_ptr<Node> next; };

auto number = make_tracked<int>(42);                    // unique_ptr<int>
unique_ptr point = make_tracked<Point>(1, 2);       // an aggregate, in parentheses
tracked_ptr node = make_tracked<Node>(7);           // moved into a tracked_ptr: the collector's from now on
node->next = make_tracked<Node>(8, node);               // a cycle, built from a unique_ptr temporary
unique_ptr owned = make_tracked<Node>();      // default-initialized: next is null
assert(*number == 42 && point->y == 2 && node->next->next == node && !owned->next);
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

struct Label {
    Label(string text, tracked_ptr<Shape> shape) : text(std::move(text)), shape(shape) {}
    string text;
    tracked_ptr<Shape> shape;     // stored by the constructor: a root from that store on
};

int main() {
    // Deterministic: a unique_ptr, destroyed at the end of the scope
    {
        unique_ptr circle = make_tracked<Circle>(1.0);       // unique_ptr<Circle>
        std::cout << "area " << circle->area() << '\n';
    }   // the Circle is destroyed here, on this thread

    // Collected: the unique_ptr moved into a tracked_ptr of the base class
    tracked_ptr<Shape> shape = make_tracked<Circle>(2.0);
    tracked_ptr label = make_tracked<Label>("big", shape);       // arguments forwarded to the constructor
    shape = nullptr;                                                     // the Circle lives on: the Label holds it
    std::cout << label->text << ": area " << label->shape->area() << '\n';

    // The dynamic type is the one the object was created with
    assert(label->shape.is<Circle>());

    label = nullptr;                          // nothing reaches the Label or the Circle now
    collector::force_collect(true);       // optional, for the demonstration only: the collector runs its cycles by itself
    return 0;
}
```

The output:

```
area 3.14159
big: area 12.5664
```

## See also

- [unique_ptr](unique_ptr.md), [tracked_ptr](tracked_ptr.md), [weak_ptr](weak_ptr.md)
- [vector](../containers/vector.md), [array](../containers/array.md) for managed sequences, which take the place of managed arrays
- [collector](collector.md), [config](config.md) for the memory ceiling and the page size
- README: [make_tracked](README.md#make_tracked), [The classes](README.md#the-classes), [The rules](README.md#the-rules), [Memory](../../garbage_collector/overview.md#memory)
- `examples/example.cpp`
