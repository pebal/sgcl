[sgcl](../../README.md) › [core](../README.md) › [list](README.md)

# sgcl::list\<T\>::end, cend

```cpp
iterator end() noexcept;                 // (1)
const_iterator end() const noexcept;     // (2)
const_iterator cend() const noexcept;    // (3)
```

Returns the iterator past the last element: the sentinel. It is not to be dereferenced; `--end()` is the last
element.

A list that has no sentinel yet, one that never held an element or one moved from, has a null `end()`, which means
the same. The first insertion makes the sentinel and invalidates that null iterator, so a fresh `end()` is taken
after it. Used all the same, a null `end()` does no harm: it no longer compares equal to `end()`, but it still
means the end as a position ([insert](insert.md), [emplace](emplace.md), [splice](splice.md)) and as the end of a
range, and a range that starts at it is empty: `erase(stale, end())` erases nothing.

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
    list<int> l;
    auto stale = l.end();  // null: the list has no sentinel yet
    l.push_back(1);
    println("{}", stale == l.end());

    l.insert(stale, 2);  // still the end as a position
    auto next = l.erase(stale, l.end());  // a range that starts at it is empty
    println("{} {}", l, next == l.end());

    println("{}", *--l.end());
}
```

Output:

```text
false
[1, 2] true
2
```

## See also

- [begin](begin.md): an iterator to the beginning
- [rend](rend.md): a reverse iterator to the end
- [sgcl::list\<T\>](README.md)
