[sgcl](../../README.md) › [core](../README.md) › [deque](../deque.md)

# sgcl::deque\<T\>::end, cend

```cpp
iterator end() noexcept;                 // (1)
const_iterator end() const noexcept;     // (2)
const_iterator cend() const noexcept;    // (3)
```

Returns an iterator past the last element. It is not dereferenced; on an empty deque it equals
[begin](begin.md).

- (1) An `iterator`.
- (2–3) A `const_iterator`.

## Parameters

None.

## Return value

An iterator past the last element.

## Complexity

Constant.

## Exceptions

None.

## Notes

`end()` is invalidated by every insertion and by an erasure at the end, `pop_back` included
([Iterator invalidation](../deque.md#iterator-invalidation)): a loop that erases takes the iterator
[erase](erase.md) returns and reads `end()` again.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    deque d = {1, 2, 3, 4, 5, 6};
    for (auto it = d.begin(); it != d.end();) {
        if (*it % 2 == 0) {
            it = d.erase(it);
        } else {
            ++it;
        }
    }
    println("{} {}", d, d.cend() - d.cbegin());
}
```

Output:

```text
[1, 3, 5] 3
```

## See also

- [begin, cbegin](begin.md): an iterator to the beginning
- [rend, crend](rend.md): a reverse iterator to the end
- [sgcl::deque\<T\>](../deque.md)
