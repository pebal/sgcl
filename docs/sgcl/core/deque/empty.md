[sgcl](../../README.md) › [core](../README.md) › [deque](../deque.md)

# sgcl::deque\<T\>::empty

```cpp
bool empty() const noexcept;
```

Checks whether the deque holds no element: `size() == 0`.

## Parameters

None.

## Return value

`true` when the deque is empty, `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    deque<int> d;
    println("{}", d.empty());

    d.push_front(1);
    println("{}", d.empty());

    d.pop_back();
    println("{}", d.empty());
}
```

Output:

```text
true
false
true
```

## See also

- [size](size.md): the number of elements
- [clear](clear.md): destroys every element
- [sgcl::deque\<T\>](../deque.md)
