[sgcl](../../README.md) › [core](../README.md) › [deque](../deque.md)

# sgcl::deque\<T\>::pop_front

```cpp
void pop_front() noexcept;
```

Destroys the first element. A block left empty stays as the spare of the front (an older spare at the same end
goes) while the deque holds elements, and every block goes when it holds none. The deque must not be empty.

## Parameters

None.

## Return value

None.

## Complexity

Constant.

## Exceptions

None.

## Notes

On an empty deque the call is undefined behaviour, as with `std::deque`. Only the erased element is invalidated,
and `end()` when the deque becomes empty; the other iterators and references stay valid
([Iterator invalidation](../deque.md#iterator-invalidation)). The spare block is what the next push at either end
takes before it allocates one: a window of elements travelling through the deque, pushed at the back and popped at
the front, allocates no blocks.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    deque<int> window;
    for (int i : range(100000)) {
        window.push_back(i);
        if (window.size() > 4) {
            window.pop_front();  // the emptied block stays as the spare
        }
    }
    println("{}", window);
}
```

Output:

```text
[99996, 99997, 99998, 99999]
```

## See also

- [front](front.md): access the first element
- [push_front](push_front.md): inserts an element at the beginning
- [pop_back](pop_back.md): removes the last element
- [sgcl::deque\<T\>](../deque.md)
