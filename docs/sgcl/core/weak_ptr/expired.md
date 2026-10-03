[sgcl](../../README.md) › [core](../README.md) › [weak_ptr](README.md)

# sgcl::weak_ptr\<T\>::expired

```cpp
bool expired() const noexcept;
```

Checks whether the cell has been cleared or there is none: whether [lock](lock.md) would return null.

## Parameters

None.

## Return value

`true` when the cell has been cleared or there is none, `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Notes

The other way round is not guaranteed: an object found unreachable stays in the cell until the cycle clears it, so
`expired()` may be `false` for an object nothing reaches any more. For a decision that needs the object, `lock()`
and test the result.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    weak_ptr<int> weak;
    {
        tracked_ptr number = make_tracked<int>(1);
        weak = number;
        println("{}", weak.expired());
    }
    collector::force_collect(true);  // optional, for the demonstration: the next cycle clears it anyway
    println("{} {}", weak.expired(), weak.lock() == nullptr);
    println("{}", weak_ptr<int>().expired());
}
```

Output:

```text
false
true true
true
```

## See also

- [lock](lock.md): the object as a `tracked_ptr`
- [collector](../collector/README.md): `force_collect`
- [sgcl::weak_ptr\<T\>](README.md)
