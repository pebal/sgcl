[sgcl](../../README.md) › [immutable](../README.md) › [list](../list.md)

# sgcl::immutable::list\<T\>::end, cend

```cpp
const_iterator end() const noexcept;     // (1)
const_iterator cend() const noexcept;    // (2)
```

Returns an iterator past the last element: an iterator to no cell, equal to a default-constructed
`const_iterator`. It may not be dereferenced.

- (1–2) The same iterator: every iterator of the list is a `const_iterator`.

## Parameters

None.

## Return value

An iterator past the last element.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"
#include <iterator>

using namespace sgcl;

int main() {
    immutable::list<int> l = {1, 2, 3};
    println("{} cells", std::distance(l.begin(), l.end()));
    println("{}", l.pop_front().pop_front().pop_front().begin() == l.cend());
}
```

Output:

```text
3 cells
true
```

## See also

- [begin, cbegin](begin.md): an iterator to the beginning
- [sgcl::immutable::list\<T\>](../list.md)
