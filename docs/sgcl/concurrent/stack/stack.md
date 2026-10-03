[sgcl](../../README.md) › [concurrent](../README.md) › [stack](../stack.md)

# sgcl::concurrent::stack\<T\>::stack

```cpp
/*(1)*/ stack() noexcept;
/*(2)*/ stack(const stack&) = delete;
```

1. An empty stack: a null head. Nothing is allocated.
2. The stack is not copyable, and not movable: a structure shared by threads has one place.

## Parameters

None.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <type_traits>

using namespace sgcl;

struct Worker {
    concurrent::stack<string> tasks;  // a member of a managed object
};

int main() {
    concurrent::stack<int> numbers;  // on the stack
    tracked_ptr worker = make_tracked<Worker>();

    println("{} {}", numbers.empty(), worker->tasks.empty());
    println("{}", std::is_copy_constructible_v<concurrent::stack<int>>);
}
```

Output:

```text
true true
false
```

## See also

- [push](push.md), [emplace](emplace.md): put an element on the top
- [sgcl::concurrent::stack\<T\>](../stack.md)
