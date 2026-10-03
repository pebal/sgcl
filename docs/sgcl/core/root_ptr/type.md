[sgcl](../../README.md) › [core](../README.md) › [root_ptr](../root_ptr.md)

# sgcl::root_ptr\<T\>::type

```cpp
const std::type_info& type() const noexcept;
```

Returns the dynamic type of the object, as `tracked_ptr` has it ([type](../tracked_ptr/type.md)): the
`std::type_info` of the type the object was created with, read from the metadata of its page, without virtual
functions.

## Parameters

None.

## Return value

The type the object was created with; `typeid(T)` for a null root, which still holds no `T`: [is](is.md)`<T>()` of
it is `false`.

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
    root_ptr<void> any = make_tracked<int>(1);
    println("{} {}", shape.type() == typeid(Circle), any.type() == typeid(int));
    println("{}", root_ptr<Shape>().type() == typeid(Shape));
}
```

Output:

```text
true true
true
```

## See also

- [is](is.md): checks whether the object was created as a given type
- [as](as.md): the pointer to the whole object as a given type
- [sgcl::root_ptr\<T\>](../root_ptr.md)
