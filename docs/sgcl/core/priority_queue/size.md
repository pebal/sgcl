[sgcl](../../README.md) › [core](../README.md) › [priority_queue](README.md)

# sgcl::priority_queue\<T, Container, Compare\>::size

```cpp
size_type size() const noexcept(noexcept(c.size()));
```

Returns the number of elements: `c.size()`.

## Parameters

None.

## Return value

The number of elements.

## Complexity

Constant.

## Exceptions

What the container's `size` throws; none for `vector` and `deque`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    priority_queue<string> words;
    for (string w : {"pear", "apple", "fig"}) {
        words.push(w);
    }
    words.pop();
    println("{} left, {} on top", words.size(), words.top());
}
```

Output:

```text
2 left, fig on top
```

## See also

- [empty](empty.md): checks whether the priority queue is empty
- [sgcl::priority_queue\<T, Container, Compare\>](README.md)
