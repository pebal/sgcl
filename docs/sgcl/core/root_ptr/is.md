[sgcl](../../README.md) › [core](../README.md) › [root_ptr](README.md)

# sgcl::root_ptr\<T\>::is

```cpp
template<class U>
bool is() const noexcept;
```

Checks whether the root holds an object created as a `U`, as `tracked_ptr` does ([is](../tracked_ptr/is.md)): an
exact match, not an "is derived from" test. A null root holds no object, so it is no `U` for any `U`, `T` included,
although its [type](type.md) is `typeid(T)`.

## Parameters

None.

## Return value

`true` when the root is not null and the object's type is `U`, `false` otherwise.

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
    root_ptr<Shape> shape = make_tracked<Circle>();
    println("{} {}", shape.is<Circle>(), shape.is<Shape>());
    shape = nullptr;
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
- [as](as.md): the pointer to the whole object as a given type
- [sgcl::root_ptr\<T\>](README.md)
