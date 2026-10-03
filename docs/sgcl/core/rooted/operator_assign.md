[sgcl](../../README.md) › [core](../README.md) › [rooted](../rooted.md)

# sgcl::rooted\<T\>::operator=

```cpp
/*(1)*/ rooted& operator=(const rooted&) noexcept = default;
/*(2)*/ rooted& operator=(rooted&&) noexcept = default;
```

Makes this `rooted` hold the value of another; its cell stays, and the value it held before lives on if anything
else reaches it.

1. Shares the value of the other `rooted`.
2. Takes the value of the other `rooted` over; the source holds nothing after, to be destroyed or assigned to.

## Parameters

| Parameter | Description |
|---|---|
| The right operand | the `rooted` whose value is shared or taken |

## Return value

`*this`.

## Complexity

Constant.

## Exceptions

None.

## Notes

A value is assigned through the constructor that copies or moves one in: `r = value` makes a new `rooted` of it and
takes that over by (2).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    rooted<string> greeting(string("hello"));
    rooted<string> other(string("bye"));
    other = greeting;  // the same string now
    println("{} {}", *other, other.get() == greeting.get());

    greeting = string("hi");  // a new value, in a managed object of its own
    println("{} {}", *greeting, *other);
}
```

Output:

```text
hello true
hi hello
```

## See also

- [(constructor)](rooted.md): makes the value, or shares another `rooted`'s
- [swap](swap.md): exchanges the values of two `rooted`s
- [sgcl::rooted\<T\>](../rooted.md)
