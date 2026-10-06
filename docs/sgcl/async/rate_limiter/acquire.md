[sgcl](../../README.md) › [async](../README.md) › [rate_limiter](README.md)

# sgcl::async::rate_limiter::acquire

```cpp
auto acquire(size_t n = 1, const stop_token& stop = {}) const noexcept;
```

Waits for `n` tokens and takes them: Go's `Wait` and `WaitN`, under the name [semaphore](../semaphore/README.md)
gives its wait for a permit. The call does nothing yet; it returns an [operation](../operation/README.md), carried
out in one of two ways
([README: Waiting operations](../README.md#waiting-operations)):

- `co_await lim.acquire()` in a task: the task suspends until the tokens come, holding no thread.
- `lim.acquire().wait()` on a thread: blocks the thread until then.

The tokens are reserved when the operation is carried out, as [reserve](reserve.md) reserves them, and the wait is
for the reservation's time: tokens there now end it at once, with no timer and no frame. A stop of `stop` ends the
wait and gives the tokens back by the rules of [cancel](../rate_limiter-reservation/cancel.md). A wait that cannot
succeed fails at once, taking nothing: a token stopped already, more tokens than the burst, tokens that would come
after the deadline of `stop` (a [stop_after](../stop_source/stop_after.md) or a [stop_at](../stop_source/stop_at.md)
of its source or of a source above it), as Go's `Wait` fails on a context whose deadline would pass first.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the tokens to wait for |
| `stop` | the token whose stop ends the wait; an empty one by default, which never stops |

## Return value

An [operation](../operation/README.md). Carried out, it gives `expected<void, rate_error>`: nothing, the tokens
taken, or a [rate_error](../rate_error/README.md) whose [why](../rate_error/why.md) is `stopped` (the token was
stopped before or during the wait), `deadline` (the tokens would come after the token's deadline, or never, at a
limit of zero) or `burst` (more than the burst).

## Complexity

Constant when the tokens are there or the wait fails: a reservation. Otherwise a timer, and a select between it and
the token's stop when there is a token.

## Exceptions

The call: none. Carried out: what a timer's wait and a select may throw, `std::system_error` when a thread of the
scheduler or the timers cannot be started.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

async::task<> send_all(async::rate_limiter lim) {
    for (int i : range(3)) {
        co_await lim.acquire();
        println("message {}", i);
    }
}

int main() {
    async::manual_clock clock;
    clock.install();
    async::rate_limiter lim(1s, 1);  // one a second
    auto t = async::spawn(send_all(lim));
    clock.advance(2s);
    t.wait();

    async::stop_source soon;
    soon.stop_after(500ms);
    auto r = lim.acquire(1, soon.token()).wait();  // the token comes in a second
    println("{}", r.error().message());
}
```

Output:

```text
message 0
message 1
message 2
the tokens would come after the deadline
```

## See also

- [reserve](reserve.md): the reservation without the wait
- [allow](allow.md): the tokens now or nothing
- [rate_error](../rate_error/README.md): why a wait gave up
- [sgcl::async::rate_limiter](README.md)
