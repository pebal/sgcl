[sgcl](../../README.md) › [core](../README.md) › [unique_ptr](README.md)

# sgcl::unique_ptr\<T\>::type

```cpp
const std::type_info& type() const noexcept;
```

Returns the dynamic type of the object, read from the metadata of its page, without virtual functions: the
`std::type_info` of the type the object was created with (`make_tracked<U>`), whatever the pointer's `T`.

## Parameters

None.

## Return value

The type the object was created with; `typeid(T)` for an empty owner, which still holds no `T`: [is](is.md)`<T>()`
of it is `false`.

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

int main() {
    unique_ptr<Shape> shape = make_tracked<Circle>();
    println("{} {}", shape.type() == typeid(Circle), unique_ptr<Shape>().type() == typeid(Shape));
}
```

Output:

```text
true true
```

## See also

- [is](is.md): checks whether the object was created as a given type
- [as](as.md): moves the object into an owner of a given type
- [sgcl::unique_ptr\<T\>](README.md)
