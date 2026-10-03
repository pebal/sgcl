[sgcl](../../README.md) › [core](../README.md) › [unique_ptr](README.md)

# sgcl::static_pointer_cast, const_pointer_cast, dynamic_pointer_cast (sgcl::unique_ptr)

```cpp
template<class T, class U>
unique_ptr<T> static_pointer_cast(unique_ptr<U>&& r) noexcept;     // (1)
template<class T, class U>
unique_ptr<T> const_pointer_cast(unique_ptr<U>&& r) noexcept;      // (2)
template<class T, class U>
unique_ptr<T> dynamic_pointer_cast(unique_ptr<U>&& r) noexcept;    // (3)
```

The casts, on an rvalue: the object is released from `r` and owned by the result, a `unique_ptr` having one owner.

1. `static_cast`.
2. `const_cast`.
3. `dynamic_cast`; null when it fails, and the object is lost with it: it was released before the cast.

## Parameters

| Parameter | Description |
|---|---|
| `r` | the owner the object is taken from; empty after |

## Return value

An owner of the object as a `T`; (3) null when the object is not a `T`.

## Complexity

Constant; (3) what the `dynamic_cast` costs.

## Exceptions

None.

## Notes

When the type is in doubt, test with [is](is.md) or move with [as](as.md) first: a failed `as` keeps the object.

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

int main() {
    unique_ptr<const Shape> shape = make_tracked<Circle>();
    unique_ptr mutable_shape = const_pointer_cast<Shape>(std::move(shape));
    unique_ptr circle = dynamic_pointer_cast<Circle>(std::move(mutable_shape));
    circle->r = 2;
    unique_ptr<Shape> back = static_pointer_cast<Shape>(std::move(circle));
    println("{} {} {} {}", bool(shape), bool(mutable_shape), bool(circle), bool(back));
}
```

Output:

```text
false false false true
```

## See also

- [as](as.md): moves the object into an owner of a given type, keeping it when the type does not match
- [sgcl::unique_ptr\<T\>](README.md)
