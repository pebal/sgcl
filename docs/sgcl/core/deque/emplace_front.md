[sgcl](../../README.md) › [core](../README.md) › [deque](../deque.md)

# sgcl::deque\<T\>::emplace_front

```cpp
template<class... A>
reference emplace_front(A&&... a) noexcept(std::is_nothrow_constructible_v<T, A&&...>);
```

Constructs an element from `a` in place at the beginning and returns a reference to it. The path is that of
[push_front](push_front.md): in the common case a load of the map, a load of the block, the construction and two
stores; the map at its beginning or a missing block goes the slow way. The elements already there never move, so
an argument may refer to an element of this deque.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the arguments of the constructor of `T` |

## Return value

A reference to the new element.

## Complexity

Constant.

## Exceptions

What the constructor of `T` from `a` throws; none when it is noexcept.

If an exception is thrown, the deque is as it was before the call.

## Notes

The references to the other elements stay valid, the iterators do not
([Iterator invalidation](../deque.md#iterator-invalidation)).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    deque<string> log = {"started"};
    string& header = log.emplace_front("log of today");
    println("{}", header);

    log.emplace_front(3, '=');
    println("{}", log);
}
```

Output:

```text
log of today
["===", "log of today", "started"]
```

## See also

- [push_front](push_front.md): inserts a copy or a moved value at the beginning
- [emplace_back](emplace_back.md): constructs an element in place at the end
- [emplace](emplace.md): constructs an element in place at any position
- [sgcl::deque\<T\>](../deque.md)
