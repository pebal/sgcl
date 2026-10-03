[sgcl](../../README.md) › [core](../README.md) › [tracked_ptr](README.md)

# sgcl::tracked_ptr\<T\>::as

```cpp
template<class U>
tracked_ptr<U> as() const noexcept;
```

Returns a pointer to the whole object as a `U` when [is](is.md)`<U>()`, also from an alias into a member or a base,
and null otherwise.

## Parameters

None.

## Return value

A `tracked_ptr<U>` to the object, or null when the object was not created as a `U` or the pointer is null.

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
    tracked_ptr<Shape> shape = make_tracked<Circle>();
    if (tracked_ptr circle = shape.as<Circle>()) {
        circle->r = 2;
    }
    println("{}", shape.as<Square>() == nullptr);

    tracked_ptr radius(&shape.as<Circle>()->r);  // an alias into the Circle
    println("{} {}", radius.as<Circle>() == shape, radius.as<Circle>()->r);
}
```

Output:

```text
true
true 2
```

## See also

- [is](is.md): checks whether the object was created as a given type
- [static_pointer_cast, const_pointer_cast, dynamic_pointer_cast](pointer_cast.md): the casts of `std::shared_ptr`
- [sgcl::tracked_ptr\<T\>](README.md)
