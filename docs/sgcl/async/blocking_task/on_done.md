[sgcl](../../README.md) › [async](../README.md) › [blocking_task](../blocking_task.md)

# sgcl::async::blocking_task\<T\>::on_done

```cpp
template<class F>
auto on_done(F f) noexcept(std::is_nothrow_move_constructible_v<F>);
```

Returns a case of a [select](../select.md), served once the job ran: the select then calls `f()` and gives the
case's index. A select that chooses another case, a [timeout](../timeout.md) for one, leaves the job running; its
result is still in the handle, for [wait](wait.md) or [result](result.md), or dropped with it.

## Parameters

| Parameter | Description |
|---|---|
| `f` | the body of the case, called with no arguments when the case is chosen |

## Return value

The case, for `async::select(...)`: `co_await` of the select in a task, `wait()` of it on a thread.

## Complexity

Constant.

## Exceptions

- The call: what the move constructor of `F` throws; none when it is noexcept.
- Carried out by the select: what `f` throws.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

async::task<string> lookup(int delay_ms) {
    auto job = async::spawn_blocking([delay_ms] {
        this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        return string("found");
    });
    string answer = "timed out";
    co_await async::select(
        job.on_done([&] { answer = job.result(); }),
        async::timeout(100ms, [] {}));
    co_return answer;
}

int main() {
    println("{}", async::spawn(lookup(1)).wait());
    println("{}", async::spawn(lookup(300)).wait());
}
```

Output:

```text
found
timed out
```

## See also

- [select](../select.md), [timeout](../timeout.md): the select and its timeout case
- [wait, operator co_await](wait.md): the plain wait
- [sgcl::async::blocking_task\<T\>](../blocking_task.md)
