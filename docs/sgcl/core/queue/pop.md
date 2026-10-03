[sgcl](../../README.md) › [core](../README.md) › [queue](README.md)

# sgcl::queue\<T, Container\>::pop

```cpp
void pop() noexcept(noexcept(c.pop_front()));
```

Removes the first element: `c.pop_front()` destroys it there and then. The queue must not be empty.

## Parameters

None.

## Return value

None. The element is read with [front](front.md) before the pop.

## Complexity

The container's `pop_front`: constant for `deque` and `list`.

## Exceptions

What the container's `pop_front` throws; none for `deque` and `list`.

## Notes

`pop` on an empty queue is undefined behaviour, as with `std::queue`. When the element is a `tracked_ptr`, the
pop destroys the pointer, not the object: the object lives on while something else refers to it, and is the
collector's when nothing does.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    queue<int> q;
    for (int i : range(1, 5)) {
        q.push(i);
    }
    vector<int> taken;
    while (!q.empty()) {
        taken.push_back(q.front());
        q.pop();
    }
    println("{}, {} left", taken, q.size());
}
```

Output:

```text
[1, 2, 3, 4], 0 left
```

## See also

- [front](front.md): access the first element
- [push](push.md): appends an element at the end
- [sgcl::queue\<T, Container\>](README.md)
