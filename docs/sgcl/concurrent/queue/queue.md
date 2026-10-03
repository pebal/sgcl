[sgcl](../../README.md) › [concurrent](../README.md) › [queue](../queue.md)

# sgcl::concurrent::queue\<T\>::queue

```cpp
/*(1)*/ queue() noexcept;
/*(2)*/ queue(const queue&) = delete;
```

1. An empty queue: one node on the managed heap whose element is taken, addressed by the head and the tail.
2. The queue is not copyable, and not movable: a structure shared by threads has one place.

## Parameters

None.

## Complexity

Constant: one allocation.

## Exceptions

None.

## Notes

The first node is the one the head and the tail stand on until the first push links another after it. It is a
managed object like every node of the list: once the head has passed it, it links to itself and is the
collector's.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <type_traits>

using namespace sgcl;

struct Pipeline {
    concurrent::queue<string> lines;  // a member of a managed object
};

int main() {
    concurrent::queue<int> numbers;  // on the stack
    tracked_ptr pipeline = make_tracked<Pipeline>();

    println("{} {}", numbers.empty(), pipeline->lines.empty());
    println("{}", std::is_copy_constructible_v<concurrent::queue<int>>);
}
```

Output:

```text
true true
false
```

## See also

- [push](push.md), [emplace](emplace.md): append an element
- [sgcl::concurrent::queue\<T\>](../queue.md)
