[sgcl](../../README.md) › [core](../README.md) › [queue](../queue.md)

# sgcl::operator==, operator\<=\> (sgcl::queue)

```cpp
friend bool operator==(const queue& lhs, const queue& rhs)     // (1)
    requires std::equality_comparable<Container>;
friend auto operator<=>(const queue& lhs, const queue& rhs)    // (2)
    requires std::three_way_comparable<Container>;
```

Compares two queues by their containers, front to back.

1. `lhs.c == rhs.c`: `true` when the queues have the same elements in the same order.
2. `lhs.c <=> rhs.c`: the elements compared lexicographically, the first that differ deciding, a queue that is
   a prefix of the other less than it.

`!=` follows from (1), and `<`, `<=`, `>` and `>=` from (2). Both are hidden friends, found by the arguments'
type.

## Parameters

| Parameter | Description |
|---|---|
| `lhs`, `rhs` | the queues to compare |

## Return value

- (1) `true` when the queues are equal, `false` otherwise.
- (2) What the containers' `<=>` returns: `std::strong_ordering` for a `deque` of `int`.

## Complexity

Linear in the size of the queues: at most one comparison of elements per position.

## Exceptions

What the comparison of the elements throws.

## Notes

Each operator takes part only when the container has it, as `std::queue`'s `<=>` does: for a `deque` of
elements without `==`, `std::equality_comparable<queue<T>>` is `false`, and without an order
`std::three_way_comparable<queue<T>>` is `false` too.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    deque d = {1, 2};
    queue<int> a(d), b(d);
    println("{} {}", a == b, a != b);

    b.push(3);
    println("{} {}", a < b, a == b);  // a is a prefix of b

    a.push(4);
    println("{}", a > b);
}
```

Output:

```text
true false
true false
true
```

## See also

- [mixin::equatable](../mixin/equatable.md), [mixin::comparable](../mixin/comparable.md): the comparisons of
  the containers
- [sgcl::queue\<T, Container\>](../queue.md)
