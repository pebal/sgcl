[sgcl](../../README.md) › [core](../README.md) › [deque](../deque.md)

# sgcl::deque\<T\>::rbegin, crbegin

```cpp
reverse_iterator rbegin() noexcept;                 // (1)
const_reverse_iterator rbegin() const noexcept;     // (2)
const_reverse_iterator crbegin() const noexcept;    // (3)
```

Returns a reverse iterator to the last element, the first of the walk from the back: `reverse_iterator(end())`.
On an empty deque it equals [rend](rend.md).

- (1) A `reverse_iterator`.
- (2–3) A `const_reverse_iterator`.

## Parameters

None.

## Return value

A reverse iterator to the last element.

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
    deque d = {1, 2, 3};
    for (auto it = d.rbegin(); it != d.rend(); ++it) {
        *it *= 10;
    }
    println("{} {}", d, *d.crbegin());

    deque<int> reversed(d.rbegin(), d.rend());
    println("{}", reversed);
}
```

Output:

```text
[10, 20, 30] 30
[30, 20, 10]
```

## See also

- [rend, crend](rend.md): a reverse iterator to the end
- [begin, cbegin](begin.md): an iterator to the beginning
- [sgcl::deque\<T\>](../deque.md)
