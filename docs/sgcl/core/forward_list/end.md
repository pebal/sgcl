[sgcl](../../README.md) › [core](../README.md) › [forward_list](README.md)

# sgcl::forward_list\<T\>::end, cend

```cpp
iterator end() noexcept;                 // (1)
const_iterator end() const noexcept;     // (2)
const_iterator cend() const noexcept;    // (3)
```

Returns the iterator past the last element: a null iterator, the link of the last node. It is not to be
dereferenced. Being null, it is never invalidated, and the `end()` of one list equals that of any other.

## Parameters

None.

## Return value

The iterator past the last element.

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
    forward_list<int> l;
    auto end = l.end();
    l.push_front(1);
    l.push_front(2);

    int sum = 0;
    for (auto it = l.begin(); it != end; ++it) {
        sum += *it;
    }
    println("{} {}", sum, end == l.cend());
}
```

Output:

```text
3 true
```

## See also

- [begin](begin.md): an iterator to the beginning
- [sgcl::forward_list\<T\>](README.md)
