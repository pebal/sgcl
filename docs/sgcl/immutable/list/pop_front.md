[sgcl](../../README.md) › [immutable](../README.md) › [list](../list.md)

# sgcl::immutable::list\<T\>::pop_front

```cpp
list pop_front() const noexcept;
```

Returns the list without its first element: the rest of the chain, which this list goes on holding too. Nothing
is allocated and nothing is copied. This list is unchanged.

## Parameters

None.

## Return value

The new list, `size() - 1` elements.

## Complexity

Constant.

## Exceptions

None. `pop_front` on an empty list is undefined; debug builds assert.

## Notes

Nothing is destroyed: this list still holds the first element, and the collector destroys it with its cell once no
list reaches the cell. 7 ns per pop over a million `long`s, against 8 ns for `std::forward_list`, which frees a
node each time ([Benchmarks](../benchmarks.md#against-std)).

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::list<int> l = {1, 2, 3};
    auto rest = l.pop_front();
    println("{} {} {}", l, rest, rest.pop_front());
}
```

Output:

```text
[1, 2, 3] [2, 3] [3]
```

## See also

- [push_front](push_front.md): the list with one more element in front
- [front](front.md): the first element
- [sgcl::immutable::list\<T\>](../list.md)
