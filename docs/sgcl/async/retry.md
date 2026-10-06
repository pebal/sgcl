[sgcl](../README.md) › [async](README.md)

# sgcl::async::retry

```cpp
#include "sgcl/async/retry.h"   // or "sgcl/async.h"

namespace sgcl::async {
    template<class F>
    auto retry(F f, const retry_policy& policy = {});                                   // (1)
    template<class F>
    auto retry(F f, const retry_policy& policy, const stop_token& stop);                // (2)
    template<class F, class P>
    auto retry(F f, const retry_policy& policy, P retry_if);                            // (3)
    template<class F, class P>
    auto retry(F f, const retry_policy& policy, P retry_if, const stop_token& stop);    // (4)
}
```

Calls `f` until it gives a value or the [policy](retry_policy.md) gives up, waiting between two attempts with
exponential backoff and jitter, and gives `f`'s last result. `f` returns an `expected<T, E>`, the way the library
fails, or is a coroutine function whose [task](task/README.md) gives one; an error is what is retried, unless a
predicate decides. What `f` throws is not retried and comes out of the retry as it is. The call does nothing yet; it returns an object of its own
type, nodiscard as an [operation](operation/README.md) is and carried out the same two ways
([README: Waiting operations](README.md#waiting-operations)):

- `co_await async::retry(f)` in a task: the task suspends for the waits, holding no thread.
- `async::retry(f).wait()` on a thread: the thread sleeps them.

The waits are those of the policy's [jitter](jitter.md), full by default: the *n*-th drawn below
min(`max_delay`, `initial` × `multiplier`^(*n* − 1)), which spreads the callers that one failure made synchronous.
The first attempt always runs.

1. The attempts and the waits of `policy`, the defaults by default: five attempts, 100 ms doubled, full jitter.
2. The same, ended also by the stop of `stop`: a stop during a wait ends the retry with the last error; a token
   stopped before the call lets the first attempt run and nothing after it.
3. An attempt tried again when `retry_if(result)` is `true`: the predicate takes the whole result of the attempt, so
   a value is retried too (an HTTP response of 503 is a value of `client.get`'s expected), and `f` may return any
   type.
4. Both.

This is the retry of an operation that may fail for a while, a connection or a request; not the backoff of a
lock-free loop.

## Parameters

| Parameter | Description |
|---|---|
| `f` | the function, or the coroutine function, that makes an attempt and returns an `expected<T, E>` |
| `policy` | the attempts, the time and the waits |
| `retry_if` | called with the result of each attempt; `false` ends the retry with it |
| `stop` | the token whose stop ends the waits |

## Return value

The object of the retry. Carried out, it gives `f`'s last result: its value, or the result of the last attempt when
the policy gave up or the stop came, or the first result `retry_if` did not want tried again.

## Complexity

The attempts made, each a call of `f`, and between two of them a timer, and a select between it and the token's stop
when there is a token. An attempt that succeeds at once costs no frame and no timer.

## Exceptions

The call: what the copy of `f` and of `retry_if` throws. Carried out: what `f` throws, at the attempt that throws,
and what a timer's wait may throw, `std::system_error` when a thread of the scheduler or the timers cannot be
started.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

struct busy {
    int code;
};

int main() {
    int calls = 0;
    auto connect = [&]() -> expected<string, busy> {
        if (++calls < 3) {
            return unexpected(busy{503});
        }
        return string("connected");
    };
    auto r = async::retry(connect, {.initial = 1ms}).wait();
    println("{} after {} attempts", *r, calls);

    calls = 0;
    auto missing = [&]() -> expected<string, busy> {
        ++calls;
        return unexpected(busy{404});
    };
    auto server_error = [](const expected<string, busy>& r) { return !r && r.error().code >= 500; };
    auto no = async::retry(missing, {}, server_error).wait();
    println("{} after {} attempt", no.error().code, calls);
}
```

Output:

```text
connected after 3 attempts
404 after 1 attempt
```

## See also

- [retry_policy](retry_policy.md): the policy
- [jitter](jitter.md): how a wait is drawn
- [stop_token](stop_token/README.md): what ends the waits
- [rate_limiter](rate_limiter/README.md): the pace of the calls that succeed
