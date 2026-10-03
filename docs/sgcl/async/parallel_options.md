[sgcl](../README.md) › [async](README.md)

# sgcl::async::parallel_options

```cpp
#include "sgcl/async/parallel.h"   // or "sgcl/async.h"

namespace sgcl::async {
    struct parallel_options {
        unsigned lanes = 0;
        size_t grain = 0;
    };
}
```

`async::parallel_options` is how [parallel_for](parallel_for.md), [parallel_for_each](parallel_for_each.md) and
[parallel_reduce](parallel_reduce.md) spread a loop: over as many lanes as the scheduler has workers, each lane
claiming about an eighth of its share of the range at a time, by default. A designated initializer names what
differs: `async::parallel_for(n, f, {.lanes = 4})`, `{.grain = 1}`.

## Member objects

| Member | Description |
|---|---|
| `lanes` | the lanes that run the loop: the caller and the tasks it starts on the workers, each calling the function one index at a time; the lane numbers a function is given are `0` to `lanes - 1`. `0`, the default, is [scheduler::workers()](scheduler/workers.md). `1` runs the loop on the caller alone. More than the workers is allowed, the lanes past them running as workers come free; the lanes that run are at most the chunks of the range |
| `grain` | the indices (or elements) a lane claims at a time, consecutive: a chunk. `0`, the default, is the length of the range divided by eight times the lanes, at least `1`. `1` claims an index at a time; a grain of the range's length or more makes one chunk, which the caller runs alone |

## Example

One lane is the caller alone, as lane `0`; two lanes give lane numbers below two:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

#include <atomic>

using namespace sgcl;

int main() {
    std::atomic<bool> beyond = false;
    async::parallel_for(1000, [&](int, unsigned lane) { beyond = beyond || lane > 0; }, {.lanes = 1});
    println("{}", beyond.load());
    async::parallel_for(1000, [&](int, unsigned lane) { beyond = beyond || lane > 1; },
                        {.lanes = 2, .grain = 1});
    println("{}", beyond.load());
}
```

Output:

```text
false
false
```

## See also

- [parallel_for](parallel_for.md), [parallel_for_each](parallel_for_each.md), [parallel_reduce](parallel_reduce.md):
  the loops that take the options
- [scheduler::set_workers](scheduler/set_workers.md): the number of workers, and so of lanes by default
