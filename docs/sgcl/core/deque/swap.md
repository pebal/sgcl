[sgcl](../../README.md) › [core](../README.md) › [deque](README.md)

# sgcl::deque\<T\>::swap

```cpp
void swap(deque& other) noexcept;
```

Exchanges the contents of the deque with those of `other`: the maps and the counts. No element is touched.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the deque to exchange the contents with |

## Return value

None.

## Complexity

Constant.

## Exceptions

None.

## Notes

The iterators and references stay valid and refer to the same elements, now in the other deque.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    deque a = {1, 2, 3};
    deque b = {9};
    int& first = a.front();

    a.swap(b);
    println("{} {} {}", a, b, first);
}
```

Output:

```text
[9] [1, 2, 3] 1
```

## See also

- [swap](swap2.md): the non-member swap
- [operator=](operator_assign.md): assigns another deque
- [sgcl::deque\<T\>](README.md)
