[sgcl](../../README.md) › [core](../README.md) › [forward_list](../forward_list.md)

# sgcl::forward_list\<T\>::before_begin, cbefore_begin

```cpp
/*(1)*/ iterator before_begin() noexcept;
/*(2)*/ const_iterator before_begin() const noexcept;
/*(3)*/ const_iterator cbefore_begin() const noexcept;
```

Returns the iterator to the sentinel, the position before the first element: what
[insert_after](insert_after.md), [emplace_after](emplace_after.md), [erase_after](erase_after.md) and
[splice_after](splice_after.md) take to work at the front. It is not to be dereferenced; `++before_begin()` is
[begin()](begin.md).

The sentinel is a link inside the list object, so the iterator is valid as long as the list object is, whatever
the list holds.

## Parameters

None.

## Return value

The iterator before the first element.

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
    forward_list l = {3, 1, 2};
    auto before = l.before_begin();
    l.insert_after(before, 0);
    println("{}", l);

    l.erase_after(before);
    l.clear();
    l.insert_after(before, 7);  // the same sentinel
    println("{} {}", l, std::next(l.cbefore_begin()) == l.cbegin());
}
```

Output:

```text
[0, 3, 1, 2]
[7] true
```

## See also

- [begin](begin.md): an iterator to the beginning
- [insert_after](insert_after.md): inserts elements after a position
- [sgcl::forward_list\<T\>](../forward_list.md)
