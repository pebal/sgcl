[sgcl](../../README.md) › [core](../README.md) › [tracked_ptr](README.md)

# sgcl::tracked_ptr\<T\>::type

```cpp
const std::type_info& type() const noexcept;
```

Returns the dynamic type of the object, read from the metadata of its page, without virtual functions and through a
`tracked_ptr<void>` as well: the `std::type_info` of the type the object was created with (`make_tracked<U>`),
whatever the pointer's `T` and wherever it points inside the object.

## Parameters

None.

## Return value

The type the object was created with; `typeid(T)` for a null pointer, which still is no `T`: [is](is.md)`<T>()` of
it is `false`.

## Complexity

Constant.

## Exceptions

None.

## Notes

The object has no header: the type is the page's, shared by every object on it.

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
    tracked_ptr<void> any = shape;
    tracked_ptr radius(&static_pointer_cast<Circle>(shape)->r);
    println("{} {} {}", shape.type() == typeid(Circle), any.type() == typeid(Circle),
            radius.type() == typeid(Circle));
    println("{}", tracked_ptr<Shape>().type() == typeid(Shape));
}
```

Output:

```text
true true true
true
```

## See also

- [is](is.md): checks whether the object was created as a given type
- [as](as.md): the pointer to the whole object as a given type
- [sgcl::tracked_ptr\<T\>](README.md)
