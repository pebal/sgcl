[sgcl](../../README.md) › [concurrent](../README.md) › [stack](README.md)

# sgcl::concurrent::stack\<T\>::empty

```cpp
bool empty() const noexcept;
```

Checks whether the stack holds an element: one load of the head.

## Parameters

None.

## Return value

`true` when the head was null, `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Notes

Under concurrent pushes and pops the answer is of the moment of the load and may be stale when it returns: a
consumer takes an element with [try_pop](try_pop.md), whose answer is the element itself, rather than with
`empty` and then a pop.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::stack<int> numbers;
    println("{}", numbers.empty());

    numbers.push(7);
    println("{}", numbers.empty());

    numbers.try_pop();
    println("{}", numbers.empty());
}
```

Output:

```text
true
false
true
```

## See also

- [size](size.md): counts the elements
- [sgcl::concurrent::stack\<T\>](README.md)
