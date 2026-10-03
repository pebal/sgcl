[sgcl](../README.md) › [async](README.md)

# sgcl::async::parallel_for

```cpp
#include "sgcl/async/parallel.h"   // or "sgcl/async.h"

namespace sgcl::async {
    /*(1)*/ template<class T, class F>
            void parallel_for(T count, F f, const parallel_options& options = {})
                noexcept(/* see below */);
    /*(2)*/ template<class T, class F>
            void parallel_for(T begin, T end, F f, const parallel_options& options = {})
                noexcept(/* see below */);
    /*(3)*/ template<class T, class F>
            void parallel_for(T begin, T end, T step, F f, const parallel_options& options = {});
}
```

Calls `f` for every index of a range, the calls spread over the workers of the [scheduler](scheduler.md) and the
caller: OpenMP's `parallel for`, `std::for_each(std::execution::par, ...)` over a range of integers, a loop Go
writes by hand with goroutines and a `WaitGroup`. The loop runs inside the call, which returns once `f` has been
called for every index, as SNS-HDR's `parallel_for_` does: `async::parallel_for(n, f);` on a thread and in a task
alike, with nothing to wait for or `co_await` afterwards.

1. The indices `0 .. count - 1`; none for a `count` of zero or less.
2. The indices `begin .. end - 1`; none when `begin` is not below `end`. A `begin` past `end` is an empty range,
   not a walk downwards, as in [range](../core/range.md).
3. The indices `begin`, `begin + step`, `begin + 2 * step`, ... while they are below `end` for a positive `step`,
   above `end` for a negative one; none when `begin` is past `end` in the direction of the step.

`f(i)` is called with the index, or `f(i, lane)` when `f` takes two arguments: the number of the lane that calls
it, `0` to the number of lanes less one, `unsigned`. A lane is one caller at a time: the loop is run by the calling
thread (lane `0`; in a task, the task's worker) and by tasks it starts on the workers, each of them a lane, so
per-lane scratch space indexed by `lane` is touched by one call at a time and needs no lock, SNS-HDR's `threadNum()`
passed as an argument. The number of lanes is the [options](parallel_options.md)' `lanes`, by default
[scheduler::workers()](scheduler/workers.md), which is what a vector of scratch space is sized with.

The lanes take the indices in chunks of `grain` consecutive indices, each chunk claimed by an increment of a
counter they share, so a lane that is slow (a core taken by another program, indices dearer than the rest) takes
fewer chunks and the others make up for it. The default grain cuts the range into about eight chunks per lane; a
grain of `1` claims an index at a time, SNS-HDR's `parallel_for_`.

`T` is an integer type of up to 64 bits other than `bool`, the same for both ends and the step:
`async::parallel_for(size_t(0), v.size(), f)`, not `(0, v.size(), f)`, or (1), `async::parallel_for(v.size(), f)`.
The indices are counted on 64 bits, so a range reaches the ends of its type (`INT64_MIN` to `INT64_MAX`, `SIZE_MAX`)
without an overflow.

## Parameters

| Parameter | Description |
|---|---|
| `count` | the number of indices, from `0` |
| `begin` | the first index |
| `end` | the bound, never an index itself |
| `step` | the distance between two indices, positive or negative, never `0` |
| `f` | called with every index, or with every index and a lane, from several threads at once; called through a `const` reference, so a `mutable` lambda does not compile |
| `options` | the number of lanes and of indices a lane claims at a time ([parallel_options](parallel_options.md)) |

## Return value

None. The call returns once every index has been visited, or once the loop has stopped at an exception.

## Complexity

`f` is called once per index. A chunk costs an atomic increment of the shared counter. A loop of more than one
chunk and lane makes one managed object for what the lanes share and starts a task per lane past the caller's; a
loop of one chunk, or with one lane, runs on the caller alone and makes nothing.

## Exceptions

- (3) `invalid_argument` for a `step` of zero, before `f` is called.
- What `f` throws. The first exception thrown by any lane is rethrown from the call once every call of `f` under way
  has returned; the chunks nobody has claimed yet are not started, while a chunk under way on another lane runs to
  its end; the other exceptions are dropped. The indices visited stay visited.

(1–2) are noexcept when the call of `f` is, `f(i)` or `f(i, lane)` as the loop calls it. A scheduler that cannot
start its workers is not an error here: the caller visits every index itself.

## Notes

The caller computes: a thread, or the worker of a task, runs lane `0` until no chunk is left to claim, and then
waits only for the chunks the other lanes have under way, never for a lane that has not started. Those chunks run on
other workers and wait for nothing of the call, so they end; the caller spins a moment on them, the last chunks being
mostly near their end, then sleeps until the last one ends. A lane that starts once every chunk is taken finds
nothing to claim and ends. So a `parallel_for` inside `f`, called on the worker that runs `f`, does not deadlock:
with every worker busy, its caller visits every index itself.

More lanes than workers is allowed: the lanes past the workers run as workers come free. The lanes that run are at
most the chunks: a loop of three chunks with eight lanes runs on three.

A task that calls the loop holds its worker until the call returns, as any computation in a task does
([README](README.md#the-rules)), and the wait at the end holds it only while other workers finish their chunks; the
other lanes are tasks of their own.

## Example

The squares of the first ten integers, each written by the lane that took its index:

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<int> squares(10);
    async::parallel_for(10, [&](int i) { squares[i] = i * i; });
    println("{}", squares);
}
```

Output:

```text
[0, 1, 4, 9, 16, 25, 36, 49, 64, 81]
```

A sum without a lock: a partial sum per lane, added up at the end.

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<int64_t> sums(async::scheduler::workers());
    async::parallel_for(3000, [&](int i, unsigned lane) { sums[lane] += i; });
    int64_t total = 0;
    for (int64_t s : sums) {
        total += s;
    }
    println("{}", total);
}
```

Output:

```text
4498500
```

A step, in a task: the multiples of seven below a thousand.

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

#include <atomic>

using namespace sgcl;

async::task<int> multiples_of_seven() {
    std::atomic<int> count = 0;
    async::parallel_for(0, 1000, 7, [&](int) { ++count; });
    co_return count.load();
}

int main() {
    println("{}", async::spawn(multiples_of_seven()).wait());
}
```

Output:

```text
143
```

A loop inside a loop: the rows of a table spread over the lanes, and the cells of each row over them again.

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<vector<int>> table(3, vector<int>(4));
    async::parallel_for(3, [&](int row) {
        async::parallel_for(4, [&](int col) { table[row][col] = row * 10 + col; });
    });
    for (auto& row : table) {
        println("{}", row);
    }
}
```

Output:

```text
[0, 1, 2, 3]
[10, 11, 12, 13]
[20, 21, 22, 23]
```

An exception stops the loop and comes out of the call:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    try {
        async::parallel_for(100, [](int i) {
            if (i == 42) {
                throw invalid_argument("index 42");
            }
        });
    } catch (const invalid_argument& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
index 42
```

## See also

- [parallel_for_each](parallel_for_each.md): every element of a range
- [parallel_reduce](parallel_reduce.md): a value of every index, combined in order
- [parallel_options](parallel_options.md): the lanes and the grain
- [task_group](task_group.md), [when_all](when_all.md): tasks of different work, started and waited for together
- [scheduler](scheduler.md): the workers the lanes run on
