[sgcl](../../README.md) › [concurrent](../README.md) › [queue](README.md)

# sgcl::concurrent::queue\<T\>::push_range

```cpp
template<class It>
void push_range(It first, It last);
```

Appends the elements of `[first, last)` together, in their order, with one compare-exchange on the list (Java's
`addAll`). The nodes are made and linked to one another first, where no other thread sees them, and the chain is
then linked after the last node as one node is. The pushes of other threads land before the chain or after it,
never inside it.

## Parameters

| Parameter | Description |
|---|---|
| `first`, `last` | the range of the elements to append, each constructed from `*first` |

## Return value

None.

## Complexity

Linear in the distance between `first` and `last`, plus the walk from the tail to the last node; one
compare-exchange on the list, however many the elements.

## Exceptions

What the constructor of `T` from `*first` throws.

If an exception is thrown, nothing is linked and the queue is as it was.

## Notes

A producer pushing *n* elements at once pays for the contended link once instead of *n* times, and a consumer
never sees a part of the chain without the elements before it. The push is lock-free, linearizable at its one
compare-exchange, and wakes a thread waiting in [pop](pop.md) when there is one.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::queue<int> numbers;
    numbers.push(1);

    vector batch = {2, 3, 4};
    numbers.push_range(batch.begin(), batch.end());
    numbers.push(5);

    vector<int> taken;
    while (auto n = numbers.try_pop()) {
        taken.push_back(*n);
    }
    println("{}", taken);
}
```

Output:

```text
[1, 2, 3, 4, 5]
```

## See also

- [push](push.md), [emplace](emplace.md): append one element
- [sgcl::concurrent::queue\<T\>](README.md)
