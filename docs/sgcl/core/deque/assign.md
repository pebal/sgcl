[sgcl](../../README.md) › [core](../README.md) › [deque](../deque.md)

# sgcl::deque\<T\>::assign

```cpp
void assign(size_type count, const T& value);    // (1)
template<std::input_iterator InputIt>
void assign(InputIt first, InputIt last);        // (2)
void assign(std::initializer_list<T> ilist);     // (3)
```

Replaces the elements of the deque.

1. With `count` copies of `value`.
2. With the elements of the range `[first, last)`. `first` and `last` may not be iterators into this deque.
3. With the elements of `ilist`.

The elements already there are assigned the new values, the surplus is popped from the back, the missing ones are
pushed to the back.

## Parameters

| Parameter | Description |
|---|---|
| `count` | the number of elements |
| `value` | the value the elements are copied from |
| `first`, `last` | the range the elements are copied from |
| `ilist` | the list the elements are copied from |

## Return value

None.

## Complexity

Linear in the number of new elements, plus linear in the old size for the elements destroyed.

## Exceptions

- (1) `length_error` when `count > max_size()`, before anything changes.
- What the copy constructor and the copy assignment of `T` throw.

The deque stays consistent and every element is destroyed exactly once, but the values already assigned have
changed and the elements appended before the throw stay.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <list>

using namespace sgcl;

int main() {
    deque d = {1, 2, 3};
    d.assign(2, 9);
    println("{}", d);

    std::list<int> more = {7, 8, 9, 10};
    d.assign(more.begin(), more.end());
    println("{}", d);

    d.assign({5, 6});
    println("{}", d);
}
```

Output:

```text
[9, 9]
[7, 8, 9, 10]
[5, 6]
```

## See also

- [operator=](operator_assign.md): assigns another deque or a list
- [(constructor)](deque.md): constructs the deque
- [sgcl::deque\<T\>](../deque.md)
