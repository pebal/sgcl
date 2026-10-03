[sgcl](../../README.md) › [core](../README.md) › [vector](../vector.md)

# sgcl::vector\<T\>::assign

```cpp
void assign(size_type count, const T& value);    // (1)
template<std::input_iterator InputIt>
void assign(InputIt first, InputIt last);        // (2)
void assign(std::initializer_list<T> ilist);     // (3)
```

Replaces the elements of the vector.

1. With `count` copies of `value`. `value` may be an element of this vector: it is copied first.
2. With the elements of the range `[first, last)`. `first` and `last` may not be iterators into this vector.
3. With the elements of `ilist`.

When the new elements fit in the capacity, the buffer stays: the elements already there are assigned the new
values, the ones past the old size are constructed, the ones past the new size are destroyed. When they do not
fit, the new elements are built in a fresh buffer and the old elements destroyed. A single-pass range (2) is
assigned by [clear](clear.md) and an append of each element.

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

- `length_error` when the number of elements is above `max_size()`.
- What the copy constructor and the copy assignment of `T` throw.

When the vector takes a fresh buffer, it is as it was before the call. Within the capacity it stays consistent
and every element is destroyed exactly once, but the values already assigned have changed; after a single-pass
range (2) it holds the elements appended before the throw.

## Notes

The capacity never shrinks: an assignment of fewer elements keeps the buffer ([shrink_to_fit](shrink_to_fit.md)
releases what is left over). A buffer the vector leaves is collected, not freed at once: a [slice](../slice.md)
taken before still reads the old elements.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <list>

using namespace sgcl;

int main() {
    vector<char> letters;
    letters.assign(3, 'a');
    println("{}", letters);

    std::list<char> word = {'s', 'g', 'c', 'l'};
    letters.assign(word.begin(), word.end());
    println("{}", letters);

    letters.assign({'x', 'y'});
    println("{}", letters);
}
```

Output:

```text
['a', 'a', 'a']
['s', 'g', 'c', 'l']
['x', 'y']
```

## See also

- [operator=](operator_assign.md): assigns another vector or a list
- [(constructor)](vector.md): constructs the vector
- [sgcl::vector\<T\>](../vector.md)
