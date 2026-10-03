[sgcl](../../README.md) › [immutable](../README.md) › [list](../list.md)

# sgcl::immutable::list\<T\>::begin, cbegin

```cpp
const_iterator begin() const noexcept;     // (1)
const_iterator cbegin() const noexcept;    // (2)
```

Returns an iterator to the first element; on an empty list it is equal to [end()](end.md).

- (1–2) The same iterator: every iterator of the list is a `const_iterator`.

The iterator is a forward iterator, a pointer to a cell: `++` follows the link to the next cell, so a walk goes
from the front, as the list is read.

## Parameters

None.

## Return value

An iterator to the first element.

## Complexity

Constant.

## Exceptions

None.

## Notes

The iterator keeps nothing alive: it is valid while some list holds the chain through its cell, this list or any
other that shares the cell. A walk of a million cells built one version at a time costs 1.5 ns per cell
([Benchmarks](../benchmarks.md#against-std)).

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"
#include <algorithm>

using namespace sgcl;

int main() {
    immutable::list<int> l = {3, 1, 4, 1, 5};
    int sum = 0;
    for (int x : l) {
        sum += x;
    }
    auto four = std::ranges::find(l, 4);
    println("sum {}, after 4: {}", sum, *++four);
    println("{}", *l.cbegin());
}
```

Output:

```text
sum 14, after 4: 1
3
```

## See also

- [end, cend](end.md): an iterator to the end
- [front](front.md): the first element
- [sgcl::immutable::list\<T\>](../list.md)
