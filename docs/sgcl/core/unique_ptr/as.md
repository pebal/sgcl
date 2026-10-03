[sgcl](../../README.md) › [core](../README.md) › [unique_ptr](README.md)

# sgcl::unique_ptr\<T\>::as

```cpp
template<class U>
unique_ptr<U> as() noexcept;
```

Moves the object into a `unique_ptr<U>` when [is](is.md)`<U>()`, leaving this owner empty. When the type does not
match it returns null and keeps the object.

## Parameters

None.

## Return value

An owner of the object as a `U`; null when the object was not created as a `U` or this owner is empty.

## Complexity

Constant.

## Exceptions

None.

## Notes

The safe form of [dynamic_pointer_cast](pointer_cast.md) for an owner: a failed cast loses the object, a failed
`as` keeps it.

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
    unique_ptr<Shape> shape = make_tracked<Circle>();
    unique_ptr square = shape.as<Square>();  // not a Square: shape keeps the object
    println("{} {}", bool(square), bool(shape));

    unique_ptr circle = shape.as<Circle>();  // unique_ptr<Circle>: the object moves
    println("{} {} {}", bool(circle), bool(shape), circle->r);
}
```

Output:

```text
false true
true false 1
```

## See also

- [is](is.md): checks whether the object was created as a given type
- [static_pointer_cast, const_pointer_cast, dynamic_pointer_cast](pointer_cast.md): the casts, moving the ownership
- [sgcl::unique_ptr\<T\>](README.md)
