[sgcl](../../README.md) › [core](../README.md) › [forward_list](../forward_list.md)

# sgcl::forward_list\<T\>::begin, cbegin

```cpp
iterator begin() noexcept;                 // (1)
const_iterator begin() const noexcept;     // (2)
const_iterator cbegin() const noexcept;    // (3)
```

Returns an iterator to the first element; on an empty list it is equal to [end()](end.md).

The iterators are forward iterators: a raw pointer to a node, stepped and dereferenced by plain loads with no write
barrier, so the `std::ranges` algorithms that need no more than a forward range work on a list. An iterator is
trivially copyable and may be kept anywhere, a `std::vector` too: it keeps nothing alive by itself, the list roots
every node it links. It is invalidated only by the erasure of its own element.

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

using namespace sgcl;

int main() {
    forward_list l = {3, 1, 2};
    auto it = std::ranges::find(l, 1);
    *it = 10;
    println("{}", l);

    l.push_front(0);  // it still names the same element
    println("{} {}", *it, *l.begin());

    auto c = l.cbegin();
    println("{}", std::is_same_v<decltype(c), forward_list<int>::const_iterator>);
}
```

Output:

```text
[3, 10, 2]
10 0
true
```

## See also

- [before_begin](before_begin.md): an iterator before the first element
- [end](end.md): an iterator to the end
- [sgcl::forward_list\<T\>](../forward_list.md)
