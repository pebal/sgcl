[sgcl](../../README.md) › [immutable](../README.md) › [list](../list.md)

# sgcl::immutable::list\<T\>::reverse

```cpp
list reverse() const noexcept(std::is_nothrow_copy_constructible_v<T>);
```

Returns the list of the elements in the reverse order: a new chain of every cell, made by a walk from the front
that puts a copy of each element in front of the ones before it. This list is unchanged, and the two share no
cell.

## Parameters

None.

## Return value

The new list, of the same size.

## Complexity

Linear in `size()`: one allocation and one copy of `T` per element.

## Exceptions

What the copy constructor of `T` throws; none when it is noexcept.

This list is never changed, so an exception leaves it as it was; no new list is made.

## Notes

`reverse` is a member of its own, not the `reverse` of [mixin::sequence](../../core/mixin/sequence.md), which
writes in place: the list returns the reversed list. It is how a list built by `push_front`, newest first, is
read oldest first.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::list<string> log;
    for (const char* event : {"open", "read", "close"}) {
        log = log.push_front(event);  // the newest in front
    }
    println("{} {}", log, log.reverse());
}
```

Output:

```text
["close", "read", "open"] ["open", "read", "close"]
```

## See also

- [push_front](push_front.md): the list with one more element in front
- [sgcl::immutable::list\<T\>](../list.md)
