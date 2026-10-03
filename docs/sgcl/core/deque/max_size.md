[sgcl](../../README.md) › [core](../README.md) › [deque](../deque.md)

# sgcl::deque\<T\>::max_size

```cpp
size_type max_size() const noexcept;
```

Returns the largest number of elements a deque may hold: `PTRDIFF_MAX`, the largest distance between two
iterators, whatever the type of the elements.

## Parameters

None.

## Return value

The largest number of elements.

## Complexity

Constant.

## Exceptions

None.

## Notes

The bound is that of the type, not of the memory: managed memory runs out far below it, and an allocation the
heap refuses ends the program ([collector](../collector.md#the-memory-limit)).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <limits>

using namespace sgcl;

int main() {
    deque<char> letters;
    deque<double> numbers;
    println("{}", letters.max_size() == size_t(std::numeric_limits<ptrdiff_t>::max()));
    println("{}", letters.max_size() == numbers.max_size());
}
```

Output:

```text
true
true
```

## See also

- [size](size.md): the number of elements
- [sgcl::deque\<T\>](../deque.md)
