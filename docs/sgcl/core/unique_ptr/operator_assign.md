[sgcl](../../README.md) › [core](../README.md) › [unique_ptr](../unique_ptr.md)

# sgcl::unique_ptr\<T\>::operator=

```cpp
unique_ptr& operator=(unique_ptr&&) noexcept = default;                                                                      // (1)
template<class U, std::enable_if_t<std::is_convertible_v<typename unique_ptr<U>::element_type*, element_type*>, int> = 0>
unique_ptr& operator=(std::unique_ptr<U, deleter_type>&& u) noexcept;                                                        // (2)
unique_ptr& operator=(std::nullptr_t) noexcept;                                                                              // (3)
```

The assignments of `std::unique_ptr`, returning this `unique_ptr`: the current object, if any, is destroyed, at once
and on the calling thread.

1. Takes the object of the source over; the source is empty after.
2. Takes the object of a `unique_ptr<U>` over, `U*` convertible to `T*`: a base class, or `void`. The source is
   empty after.
3. Leaves the owner empty.

## Parameters

| Parameter | Description |
|---|---|
| `u` | the owner the object is taken from |

## Return value

`*this`.

## Complexity

Constant, plus the destructor of the object destroyed.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Number {
    ~Number() { println("{} destroyed", value); }
    int value;
};

int main() {
    unique_ptr number = make_tracked<Number>(1);
    number = make_tracked<Number>(2);
    println("now {}", number->value);
    unique_ptr<Number>& same = (number = nullptr);
    println("empty: {} {}", !number, &same == &number);
}
```

Output:

```text
1 destroyed
now 2
2 destroyed
empty: true true
```

## See also

- [(constructor)](unique_ptr.md): constructs the owner
- [reset](reset.md): destroys the object, or replaces it
- [sgcl::unique_ptr\<T\>](../unique_ptr.md)
