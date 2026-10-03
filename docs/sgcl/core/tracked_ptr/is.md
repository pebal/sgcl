[sgcl](../../README.md) › [core](../README.md) › [tracked_ptr](README.md)

# sgcl::tracked_ptr\<T\>::is

```cpp
template<class U>
bool is() const noexcept;
```

Checks whether the pointer points to an object created as a `U`: `type() == typeid(U)` for a pointer that is not
null. An exact match, not an "is derived from" test: a `tracked_ptr<Base>` to a `Derived` is `is<Derived>()`, not
`is<Base>()`; a derived-class test is [dynamic_pointer_cast](pointer_cast.md). The `const` of `U` is ignored, as
`typeid` ignores it. A null pointer points to no object, so it is no `U` for any `U`, `T` included, although its
[type](type.md) is `typeid(T)`.

## Parameters

None.

## Return value

`true` when the pointer is not null and the object's type is `U`, `false` otherwise.

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
    tracked_ptr<Shape> shape = make_tracked<Circle>();
    println("{} {}", shape.is<Circle>(), shape.is<Shape>());
    println("{}", dynamic_pointer_cast<Shape>(shape) != nullptr);
    println("{}", tracked_ptr<Shape>().is<Shape>());
}
```

Output:

```text
true false
true
false
```

## See also

- [type](type.md): the type the object was created with
- [as](as.md): the pointer to the whole object as a given type
- [sgcl::tracked_ptr\<T\>](README.md)
