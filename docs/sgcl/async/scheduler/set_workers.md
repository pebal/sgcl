[sgcl](../../README.md) › [async](../README.md) › [scheduler](README.md)

# sgcl::async::scheduler::set_workers

```cpp
static void set_workers(unsigned n);
```

Sets the number of worker threads from now on, over `SGCL_WORKERS` and `config::workers`; 0 is the hardware
concurrency, and more than 64 is 64. Before the first task it is the number the scheduler starts with. After it,
the workers are stopped as [stop](stop.md) stops them, once the tasks ready at that moment have run, and started
again with the new number; the queues are kept, and the tasks on the rings of workers no longer there go to the
global queue. The same number again does nothing.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the number of workers; 0 for the hardware concurrency |

## Return value

None.

## Complexity

Constant before the scheduler starts. After it, the join of every worker and the start of the new ones.

## Exceptions

`std::system_error` when a worker thread cannot be started or joined.

## Notes

The call joins the workers, so it is never made from a task: debug builds assert. A program changes the number at a
quiet moment. The same from the shell, with no change to the program: `SGCL_WORKERS=4 ./server`.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<int> answer() {
    co_return 42;
}

int main() {
    async::scheduler::set_workers(2);  // before the first task: the scheduler starts with two
    println("{} from {} workers", async::spawn(answer()).wait(), async::scheduler::workers());

    async::scheduler::set_workers(4);  // later: the workers stopped and started again
    println("{} from {} workers", async::spawn(answer()).wait(), async::scheduler::workers());
}
```

Output:

```text
42 from 2 workers
42 from 4 workers
```

## See also

- [workers](workers.md): the number now
- [set_worker_spin](set_worker_spin.md): how long an idle worker looks for work
- [sgcl::async::scheduler](README.md)
