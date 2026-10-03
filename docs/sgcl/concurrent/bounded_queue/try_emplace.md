[sgcl](../../README.md) › [concurrent](../README.md) › [bounded_queue](README.md)

# sgcl::concurrent::bounded_queue\<T\>::try_emplace

```cpp
template<class... A>
bool try_emplace(A&&... a) noexcept(std::is_nothrow_constructible_v<T, A...>);
```

Appends an element constructed in place from `a...` when there is room: `T(std::forward<A>(a)...)` in the cell
at the enqueue position, won and published as [try_push](try_push.md) wins and publishes it. When the queue is
full, nothing is constructed.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the arguments the element is constructed from |

## Return value

`true` when the element was appended, `false` when the queue was full.

## Complexity

Constant, plus the retries of a lost compare-exchange when other producers push at once.

## Exceptions

What the constructor of `T` throws; none when it is noexcept. The cell whose position was won is published without an element and passed
over by the consumer that reaches it; the queue is as it was, except that the cell counts in [size](size.md)
until a consumer has passed it.

## Notes

Lock-free, and linearizable at the compare-exchange on the enqueue position, with the backoff after a lost
exchange, as `try_push` is; `try_push` is this function with the value as its argument. The blocking
[push](push.md) has no emplacing form: construct the element and push it, or loop on `try_emplace`.

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
    concurrent::bounded_queue<Job> jobs(2);
    println("{}", jobs.try_emplace("backup", 2));
    println("{}", jobs.try_emplace("report", 1));
    println("{}", jobs.try_emplace("cleanup", 3));

    while (auto job = jobs.try_pop()) {
        println("{} {}", job->name, job->priority);
    }
}
```

Output:

```text
true
true
false
backup 2
report 1
```

## See also

- [try_push](try_push.md): appends a copy or a moved value
- [push](push.md): waits for room
- [sgcl::concurrent::bounded_queue\<T\>](README.md)
