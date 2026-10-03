[sgcl](../../README.md) › [core](../README.md) › [tracked_ptr](README.md)

# sgcl::static_pointer_cast, const_pointer_cast, dynamic_pointer_cast (sgcl::tracked_ptr)

```cpp
template<class T, class U>
tracked_ptr<T> static_pointer_cast(const tracked_ptr<U>& p) noexcept;     // (1)
template<class T, class U>
tracked_ptr<T> const_pointer_cast(const tracked_ptr<U>& p) noexcept;      // (2)
template<class T, class U>
tracked_ptr<T> dynamic_pointer_cast(const tracked_ptr<U>& p) noexcept;    // (3)
```

The casts of `std::shared_ptr`: a `tracked_ptr<T>` to the result of the cast of the raw pointer.

1. `static_cast`.
2. `const_cast`.
3. `dynamic_cast`; null when it fails.

The result addresses a base or a derived subobject of the same object, an alias that keeps the whole object and
lives as long as the alias does.

## Parameters

| Parameter | Description |
|---|---|
| `p` | the pointer to cast |

## Return value

The pointer cast to `T`; (3) null when the object is not a `T`.

## Complexity

Constant; (3) what the `dynamic_cast` costs.

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
    tracked_ptr<const Shape> shape = make_tracked<Circle>();
    tracked_ptr circle = dynamic_pointer_cast<const Circle>(shape);
    tracked_ptr square = dynamic_pointer_cast<const Square>(shape);
    tracked_ptr mutable_shape = const_pointer_cast<Shape>(shape);
    tracked_ptr known = static_pointer_cast<Circle>(mutable_shape);
    known->r = 3;
    println("{} {} {}", circle->r, square == nullptr, known == shape);
}
```

Output:

```text
3 true true
```

## See also

- [as](as.md): the pointer to the whole object as the type it was created with
- [is](is.md): checks the type the object was created with
- [sgcl::tracked_ptr\<T\>](README.md)
