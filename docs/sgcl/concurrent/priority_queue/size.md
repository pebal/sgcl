[sgcl](../../README.md) › [concurrent](../README.md) › [priority_queue](../priority_queue.md)

# sgcl::concurrent::priority_queue\<T, Compare\>::size

```cpp
size_type size() const noexcept;
```

The number of elements: a load of the count kept beside the heap, without the lock.

## Parameters

None.

## Return value

The number of elements as of the last push or pop completed.

## Complexity

Constant.

## Exceptions

None.

## Notes

Under concurrent pushes and pops the count is a snapshot of no particular moment, as Java's `size` is; it is exact
once the other threads are quiet. Reading it takes no lock, so a monitoring thread that polls the size never slows
the producers and consumers down.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::priority_queue<int> numbers;
    vector<thread> producers;
    for (int p : range(4)) {
        producers.emplace_back([&numbers, p] {
            for (int i : range(250)) {
                numbers.push(p * 250 + i);
            }
        });
    }
    for (auto& t : producers) {
        t.join();
    }
    println("{}", numbers.size());
}
```

Output:

```text
1000
```

## See also

- [empty](empty.md): checks whether the queue holds an element
- [sgcl::concurrent::priority_queue\<T, Compare\>](../priority_queue.md)
