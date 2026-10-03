[sgcl](../../README.md) › [core](../README.md) › [unique_ptr](README.md)

# sgcl::unique_ptr\<T\>::is

```cpp
template<class U>
bool is() const noexcept;
```

Checks whether the owner holds an object created as a `U`: `type() == typeid(U)` for an owner that is not empty.
An exact match, not an "is derived from" test. An empty owner holds no object, so it is no `U` for any `U`, `T`
included, although its [type](type.md) is `typeid(T)`.

## Parameters

None.

## Return value

`true` when the owner is not empty and the object's type is `U`, `false` otherwise.

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
    println("{} {}", shape.is<Circle>(), shape.is<Shape>());
    shape.reset();
    println("{}", shape.is<Shape>());
}
```

Output:

```text
true false
false
```

## See also

- [type](type.md): the type the object was created with
- [as](as.md): moves the object into an owner of a given type
- [sgcl::unique_ptr\<T\>](README.md)
