[sgcl](../../README.md) › [core](../README.md) › [list](README.md)

# sgcl::list\<T\>::begin, cbegin

```cpp
iterator begin() noexcept;                 // (1)
const_iterator begin() const noexcept;     // (2)
const_iterator cbegin() const noexcept;    // (3)
```

Returns an iterator to the first element; on an empty list it is equal to [end()](end.md).

The iterators are bidirectional: a raw pointer to a node, stepped and dereferenced by plain loads with no write
barrier, so the `std::ranges` algorithms that need no random access work on a list. An iterator is trivially
copyable and may be kept anywhere, a `std::vector` too: it keeps nothing alive by itself, the list roots every node
it links. It is invalidated only by the erasure of its own element.

## Parameters

None.

## Return value

An iterator to the first element.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <algorithm>
#include <vector>

using namespace sgcl;

int main() {
    list l = {3, 1, 2};
    std::vector<list<int>::iterator> kept;  // iterators may live anywhere
    for (auto it = l.begin(); it != l.end(); ++it) {
        kept.push_back(it);
    }

    l.erase(std::ranges::find(l, 1));  // only the iterator to 1 is invalid now
    l.push_front(0);
    println("{} {} {}", l, *kept[0], *kept[2]);

    auto c = l.cbegin();
    println("{}", std::is_same_v<decltype(c), list<int>::const_iterator>);
}
```

Output:

```text
[0, 3, 2] 3 2
true
```

## See also

- [end](end.md): an iterator to the end
- [rbegin](rbegin.md): a reverse iterator to the beginning
- [sgcl::list\<T\>](README.md)
