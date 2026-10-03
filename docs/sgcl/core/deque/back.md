[sgcl](../../README.md) › [core](../README.md) › [deque](../deque.md)

# sgcl::deque\<T\>::back

```cpp
reference back() noexcept;                // (1)
const_reference back() const noexcept;    // (2)
```

Returns a reference to the last element. The deque must not be empty.

## Parameters

None.

## Return value

A reference to the last element.

## Complexity

Constant.

## Exceptions

None.

## Notes

On an empty deque the call is undefined behaviour, as with `std::deque`. The reference stays valid across a push
at either end; [pop_back](pop_back.md) destroys the element it refers to.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    deque<int> history;
    for (int i : range(1, 6)) {
        history.push_back(i * 10);
    }
    println("{}", history.back());

    history.back() += 5;
    println("{}", history);
}
```

Output:

```text
50
[10, 20, 30, 40, 55]
```

## See also

- [front](front.md): access the first element
- [push_back](push_back.md), [pop_back](pop_back.md): append, remove the last element
- [sgcl::deque\<T\>](../deque.md)
