[sgcl](../../README.md) › [core](../README.md) › [stack](README.md)

# sgcl::operator==, operator\<=\> (sgcl::stack)

```cpp
friend bool operator==(const stack& lhs, const stack& rhs)     // (1)
    requires std::equality_comparable<Container>;
friend auto operator<=>(const stack& lhs, const stack& rhs)    // (2)
    requires std::three_way_comparable<Container>;
```

Compares two stacks by their containers, the elements from the bottom to the top. Hidden friends of the stack,
found by the arguments' type; `!=`, `<`, `<=`, `>` and `>=` follow from them.

1. `lhs.c == rhs.c`: `true` when the stacks hold equal elements in the same order.
2. `lhs.c <=> rhs.c`: the lexicographical comparison of the containers, a stack that is a prefix of the other
   being less. The result is that of the container's `<=>`.

Each takes part only when the container's operator is there, which for the containers of the library means an
element type that compares: otherwise `std::equality_comparable<stack<T>>` or `std::three_way_comparable<stack<T>>`
is `false`.

## Parameters

| Parameter | Description |
|---|---|
| `lhs`, `rhs` | the stacks to compare |

## Return value

- (1) `true` when the containers are equal, `false` otherwise.
- (2) The ordering of the containers.

## Complexity

Linear in the size of the smaller stack, up to the first pair of elements that differ.

## Exceptions

What the comparison of the containers throws: for the containers of the library, what the comparison of the
elements throws.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    deque d = {1, 2};
    stack<int> a(d);
    stack<int> b(d);
    println("{}", a == b);

    b.push(3);
    println("{} {} {}", a < b, a != b, (b <=> a) > 0);  // a is a prefix of b
}
```

Output:

```text
true
true true true
```

## See also

- [top](top.md): the top element
- [sgcl::stack\<T, Container\>](README.md)
