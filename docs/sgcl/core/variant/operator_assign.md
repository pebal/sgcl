[sgcl](../../README.md) › [core](../README.md) › [variant](../variant.md)

# sgcl::variant\<Ts...\>::operator=

```cpp
/*(1)*/ variant& operator=(const variant& o)
            noexcept(((std::is_nothrow_copy_constructible_v<Ts>
                       && std::is_nothrow_copy_assignable_v<Ts>) && ...))
            requires ((std::is_copy_constructible_v<Ts> && std::is_copy_assignable_v<Ts>) && ...);
/*(2)*/ variant& operator=(variant&& o)
            noexcept(((std::is_nothrow_move_constructible_v<Ts>
                       && std::is_nothrow_move_assignable_v<Ts>) && ...))
            requires ((std::is_move_constructible_v<Ts> && std::is_move_assignable_v<Ts>) && ...);
/*(3)*/ template<class U>
        requires std::is_constructible_v<T_j, U> && std::is_assignable_v<T_j&, U>
        variant& operator=(U&& u)
            noexcept(std::is_nothrow_constructible_v<T_j, U>
                     && std::is_nothrow_assignable_v<T_j&, U>);
```

Replaces the alternative held, as `std::variant`'s assignments do.

1. When `o` holds the same alternative, it is copy-assigned; when `o` is valueless, the variant becomes valueless;
   otherwise the alternative held is destroyed and a copy of `o`'s constructed in its place. The copy is made in a
   temporary first and moved in when the alternative's copy may throw and its move may not, so that a throwing
   copy leaves the old alternative as it was.
2. The same with moves: move-assigned when the alternatives agree, else destroyed and move-constructed from `o`'s.
3. `T_j` is the alternative the [constructor](variant.md) (4) selects for `u`. When the variant holds it, it is
   assigned `std::forward<U>(u)`; otherwise the old alternative is destroyed and `T_j` constructed from `u`, through
   a temporary when its construction from `u` may throw and its move may not. An lvalue `u` may lie in the
   alternative it replaces (`v = get<failure>(v).message`): when an alternative's destructor does something, a
   tracked word nulled, `T_j` is made from `u` into a temporary first and moved in, so it reads `u` before the old
   alternative is destroyed. Takes part only when `U` is not the variant and the selection finds exactly one
   alternative.

A pointer alternative that is replaced leaves null in its word: its object is unreferenced from then on.

## Parameters

| Parameter | Description |
|---|---|
| `o` | the variant to copy or to move from |
| `u` | the value to assign |

## Return value

`*this`.

## Complexity

Constant, plus the assignment, or the destruction and the construction, of the alternatives.

## Exceptions

- (1) What the copy constructor or the copy assignment of an alternative throws; none when every alternative's are
  noexcept.
- (2) What the move constructor or the move assignment of an alternative throws; none when every alternative's are
  noexcept.
- (3) What the construction of `T_j` from `u` or the assignment throws; none when both are noexcept.

An assignment that throws leaves the alternative as its own assignment left it. A construction that throws after
the old alternative was destroyed leaves the variant valueless ([valueless_by_exception](valueless_by_exception.md)).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int value;
};

int main() {
    variant<int, tracked_ptr<Node>, string> v = 1;
    v = make_tracked<Node>(2);  // the int destroyed, the pointer in its word
    println("{} {}", v.index(), get<1>(v)->value);

    v = "text";  // the pointer destroyed: the Node is unreferenced
    println("{} {}", v.index(), get<2>(v));

    variant<int, tracked_ptr<Node>, string> w = 5;
    v = w;
    println("{} {}", v.index(), get<0>(v));
}
```

Output:

```text
1 2
2 text
0 5
```

## See also

- [emplace](emplace.md): constructs an alternative in place
- [(constructor)](variant.md): the selection of an alternative for a value
- [sgcl::variant\<Ts...\>](../variant.md)
