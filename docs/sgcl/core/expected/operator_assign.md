[sgcl](../../README.md) › [core](../README.md) › [expected](../expected.md)

# sgcl::expected\<T, E\>::operator=

```cpp
expected& operator=(const expected& o)                                                         // (1)
    noexcept(std::is_nothrow_copy_constructible_v<T> &&
             std::is_nothrow_copy_assignable_v<T> &&
             std::is_nothrow_copy_constructible_v<E> &&
             std::is_nothrow_copy_assignable_v<E>)
    requires std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T> &&
             std::is_copy_constructible_v<E> && std::is_copy_assignable_v<E> &&
             (std::is_nothrow_move_constructible_v<T> ||
              std::is_nothrow_move_constructible_v<E>);
expected& operator=(expected&& o)                                                              // (2)
    noexcept(std::is_nothrow_move_constructible_v<T> &&
             std::is_nothrow_move_assignable_v<T> &&
             std::is_nothrow_move_constructible_v<E> &&
             std::is_nothrow_move_assignable_v<E>)
    requires std::is_move_constructible_v<T> && std::is_move_assignable_v<T> &&
             std::is_move_constructible_v<E> && std::is_move_assignable_v<E> &&
             (std::is_nothrow_move_constructible_v<T> ||
              std::is_nothrow_move_constructible_v<E>);
template<class U = T>
requires std::is_constructible_v<T, U> && std::is_assignable_v<T&, U>
expected& operator=(U&& v)                                                                     // (3)
    noexcept(std::is_nothrow_constructible_v<T, U> && std::is_nothrow_assignable_v<T&, U>);
template<class G>
requires std::is_constructible_v<E, const G&> && std::is_assignable_v<E&, const G&>
expected& operator=(const unexpected<G>& u)                                                    // (4)
    noexcept(std::is_nothrow_constructible_v<E, const G&> &&
             std::is_nothrow_assignable_v<E&, const G&>);
template<class G>
requires std::is_constructible_v<E, G> && std::is_assignable_v<E&, G>
expected& operator=(unexpected<G>&& u)                                                         // (5)
    noexcept(std::is_nothrow_constructible_v<E, G> && std::is_nothrow_assignable_v<E&, G>);
```

Replaces the value or the error, as `std::expected`'s assignments do: the one held is assigned when the new one is
of the same kind, and replaced when it is of the other.

1. The value or the error of `o`, copied.
2. The value or the error of `o`, moved.
3. The value `std::forward<U>(v)`. Takes part only when `U` is not the `expected` or an `unexpected`, and the
   replacement below can be made safe: `T` constructed from `U` without throwing, or `T` or `E` moved without
   throwing.
4. The error `u.error()`.
5. The error, moved from `u`.

- (4–5) Take part only when the replacement can be made safe: `E` constructed from the argument without throwing,
  or `T` or `E` moved without throwing.

A replacement never leaves the `expected` with neither: the new one is constructed in place when that cannot throw;
in a temporary moved in when it moves without throwing; otherwise the old one is moved out first and put back if
the construction throws. An lvalue argument may lie in the one it replaces (`e = e.error().message`): when the old
one's destructor does something, a tracked word nulled, the new one is made from it into a temporary first and moved
in, so it reads the old one before it is destroyed. The one replaced is destroyed: a pointer value's object is
unreferenced from then on.

`expected<void, E>` has (1), (2), (4) and (5), each without the condition on the moves: a success is always there to
fall back on, so an error whose construction throws leaves a success behind.

## Parameters

| Parameter | Description |
|---|---|
| `o` | the `expected` to copy or to move from |
| `v` | the new value |
| `u` | the new error, wrapped in an `unexpected` |

## Return value

`*this`.

## Complexity

Constant, plus the assignment, or the construction and the destruction, of the value or the error.

## Exceptions

What the construction or the assignment of `T` or of `E` throws; none when the ones of the overload are noexcept.

If an exception is thrown, the `expected` holds a value or an error: the one assigned, as its assignment left it,
or the old one, put back.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int value;
};

int main() {
    expected<tracked_ptr<Node>, string> e = make_tracked<Node>(1);
    e = unexpected("gone");  // the pointer destroyed: the Node is unreferenced
    println("{} {}", e.has_value(), e.error());

    e = make_tracked<Node>(2);
    println("{} {}", e.has_value(), (*e)->value);

    expected<tracked_ptr<Node>, string> other = unexpected("other");
    e = other;
    println("{} {}", e.has_value(), e.error());
}
```

Output:

```text
false gone
true 2
false other
```

## See also

- [emplace](emplace.md): constructs a value in place
- [(constructor)](expected.md): constructs the `expected`
- [sgcl::expected\<T, E\>](../expected.md)
