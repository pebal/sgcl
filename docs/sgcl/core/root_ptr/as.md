[sgcl](../../README.md) › [core](../README.md) › [root_ptr](../root_ptr.md)

# sgcl::root_ptr\<T\>::as

```cpp
template<class U>
tracked_ptr<U> as() const noexcept;
```

Returns a `tracked_ptr` to the whole object as a `U` when [is](is.md)`<U>()`, and null otherwise, as `tracked_ptr`
does ([as](../tracked_ptr/as.md)). The root keeps its pointer.

## Parameters

None.

## Return value

A `tracked_ptr<U>` to the object, or null when the object was not created as a `U` or the root is null.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Shape {
    virtual ~Shape() = default;
};

struct Circle : Shape {
    double r = 1;
};

struct Square : Shape {
    double a = 1;
};

int main() {
    root_ptr<Shape> shape = make_tracked<Circle>();
    if (tracked_ptr circle = shape.as<Circle>()) {
        circle->r = 3;
    }
    println("{} {}", shape.as<Square>() == nullptr, shape.as<Circle>()->r);
}
```

Output:

```text
true 3
```

## See also

- [is](is.md): checks whether the object was created as a given type
- [sgcl::root_ptr\<T\>](../root_ptr.md)
