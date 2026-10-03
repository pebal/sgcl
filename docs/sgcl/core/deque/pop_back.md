[sgcl](../../README.md) › [core](../README.md) › [deque](../deque.md)

# sgcl::deque\<T\>::pop_back

```cpp
void pop_back() noexcept;
```

Destroys the last element. A block left empty stays as the spare of the end (an older spare at the same end goes)
while the deque holds elements, and every block goes when it holds none. The deque must not be empty.

## Parameters

None.

## Return value

None.

## Complexity

Constant.

## Exceptions

None.

## Notes

On an empty deque the call is undefined behaviour, as with `std::deque`. The erased element and `end()` are
invalidated; the other iterators and references stay valid
([Iterator invalidation](../deque.md#iterator-invalidation)). The spare block is what the next push at either end
takes before it allocates one, so elements pushed at one end and popped at the other allocate no blocks.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    deque<string> undo = {"type a", "type b", "delete"};
    println("undo {}", undo.back());
    undo.pop_back();
    println("{}", undo);
}
```

Output:

```text
undo delete
["type a", "type b"]
```

## See also

- [back](back.md): access the last element
- [push_back](push_back.md): appends an element at the end
- [pop_front](pop_front.md): removes the first element
- [sgcl::deque\<T\>](../deque.md)
