[sgcl](../../README.md) › [core](../README.md) › [expected](README.md)

# sgcl::expected\<T, E\>::swap

```cpp
void swap(expected& o)
    noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_swappable_v<T> &&
             std::is_nothrow_move_constructible_v<E> && std::is_nothrow_swappable_v<E>)
    requires std::is_swappable_v<T> && std::is_swappable_v<E> &&
             std::is_move_constructible_v<T> && std::is_move_constructible_v<E> &&
             (std::is_nothrow_move_constructible_v<T> || std::is_nothrow_move_constructible_v<E>);
```

Swaps the contents of `*this` and `o`, as `std::expected::swap` does. Two values are swapped with `swap`, and so
are two errors; a value and an error change places through a temporary of the one that moves without throwing,
and when a move throws, the other side is put back as it was.

`expected<void, E>` has `swap` with the conditions on `E` alone: two errors are swapped, an error and a success
change places by a move of the error.

## Parameters

| Parameter | Description |
|---|---|
| `o` | the `expected` to swap with |

## Return value

None.

## Complexity

Constant, plus the swap or the moves of the values and the errors.

## Exceptions

What the move constructor or the swap of `T` or of `E` throws; none when they are noexcept.

If an exception is thrown while a value and an error change places, the one moved into the temporary is put back where
it was, and each `expected` holds a value or an error.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    expected<int, string> a = 1;
    expected<int, string> b = unexpected("no");
    a.swap(b);
    println("{} {}", a.error(), *b);
}
```

Output:

```text
no 1
```

## See also

- [swap](swap2.md): the same as a free function
- [sgcl::expected\<T, E\>](README.md)
