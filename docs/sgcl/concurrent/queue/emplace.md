[sgcl](../../README.md) › [concurrent](../README.md) › [queue](../queue.md)

# sgcl::concurrent::queue\<T\>::emplace

```cpp
template<class... A>
void emplace(A&&... a) noexcept(std::is_nothrow_constructible_v<T, A...>);
```

Appends an element constructed in place from `a...`: `T(std::forward<A>(a)...)` in a new node on the managed
heap, linked after the last node as [push](push.md) links it.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the arguments the element is constructed from |

## Return value

None.

## Complexity

Constant, plus the walk from the tail to the last node: a node or two, more when other threads push at once.

## Exceptions

What the constructor of `T` throws; none when it is noexcept.

If an exception is thrown, nothing is linked and the queue is as it was.

## Notes

Lock-free, and linearizable at the compare-exchange that links the node, as `push` is; it wakes a thread waiting
in [pop](pop.md) when there is one.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Job {
    string name;
    int priority;
};

int main() {
    concurrent::queue<Job> jobs;
    jobs.emplace("backup", 2);
    jobs.emplace("report", 1);

    while (auto job = jobs.try_pop()) {
        println("{} {}", job->name, job->priority);
    }
}
```

Output:

```text
backup 2
report 1
```

## See also

- [push](push.md): appends a copy or a moved value
- [push_range](push_range.md): appends the elements of a range with one exchange
- [sgcl::concurrent::queue\<T\>](../queue.md)
