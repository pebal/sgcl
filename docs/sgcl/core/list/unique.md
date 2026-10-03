[sgcl](../../README.md) › [core](../README.md) › [list](../list.md)

# sgcl::list\<T\>::unique

```cpp
/*(1)*/ size_type unique() noexcept(/* see below */);
/*(2)*/ template<class BinaryPredicate>
        size_type unique(BinaryPredicate pred)
            noexcept(std::is_nothrow_invocable_v<BinaryPredicate&, T&, T&>);
```

Erases every element equal to the one before it, keeping the first of each run of equal elements.

1. Compares by `==`.
2. Compares by `pred(previous, current)`, where `previous` is the element kept last.

## Parameters

| Parameter | Description |
|---|---|
| `pred` | the equality of two elements, `bool pred(T& previous, T& current)` |

## Return value

The number of erased elements.

## Complexity

Linear in `size()`: one comparison per element after the first.

## Exceptions

- (1) What `==` of the elements throws; none when it is noexcept, as for `int`.
- (2) What `pred` throws; none when its call is noexcept.

If an exception is thrown, the list stays valid: the elements erased before it are gone, the others are in their
places.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    list l = {1, 1, 2, 2, 2, 3, 1};
    size_t dropped = l.unique();
    println("{} erased: {}", dropped, l);

    list m = {1, 2, 4, 7, 8, 20};
    // an element close to the one kept before it is erased
    dropped = m.unique([](int previous, int x) { return x - previous < 3; });
    println("{} erased: {}", dropped, m);
}
```

Output:

```text
3 erased: [1, 2, 3, 1]
2 erased: [1, 4, 7, 20]
```

## See also

- [remove, remove_if](remove.md): erase the elements equal to a value, or satisfying a predicate
- [sort](sort.md): brings equal elements together
- [sgcl::list\<T\>](../list.md)
