[sgcl](../../README.md) › [async](../README.md)

# sgcl::async::rate_limiter

```cpp
#include "sgcl/async/rate_limiter.h"   // or "sgcl/async.h"

namespace sgcl::async {
    class rate_limiter {
    public:
        class reservation;

        static constexpr double inf = std::numeric_limits<double>::infinity();

        friend bool operator==(const rate_limiter& a, const rate_limiter& b) noexcept;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::async::rate_limiter` is a token bucket, Go's `golang.org/x/time/rate.Limiter`: a bucket of `burst` tokens,
full at the start, refilled at a limit of so many a second, and an event that takes one token, or *n*. Three calls
take them. [allow](allow.md) takes them now or says no, for a request dropped when the rate is passed;
[reserve](reserve.md) takes them for the moment they come, the bucket going into debt, and says when that is, for a
caller that schedules itself; [acquire](acquire.md) waits for them, in a task that holds no thread while it waits or on a
thread, a [stop_token](../stop_token/README.md) ending the wait. The limit and the burst change at run time
([set_limit](set_limit.md), [set_burst](set_burst.md)), the tokens of the moment kept.

What differs from Go: the bucket is one word, not a float behind a mutex. It is the generic cell rate algorithm, the
token bucket's equivalent: the time at which the bucket would be full again, which *n* tokens move on by *n* times
the time of one. A call reads the clock and changes that word with one compare-exchange, so many threads take tokens
at once without a lock and refusals touch nothing. The time is counted in fractions of a nanosecond, so a limit of
150 million bytes a second holds its rate where whole nanoseconds would be 5% off. The time is the module's
[clock](../../core/clock/README.md), which a test moves with a [manual_clock](../manual_clock/README.md); there are
no calls with a time of their own (Go's `AllowN(t, n)`).

## Rules

- A rate limiter is a handle: one word, a tracked word to the bucket, which copies share;
  [operator==](operator_cmp.md) says whether two are the same bucket. A task takes it by value
  ([README: Handles](../README.md#handles)).
- The bucket never holds more than the burst. More tokens than the burst are never given at a finite limit: `allow`
  says no, `reserve` gives a reservation that is not ok, `acquire` fails with [rate_error](../rate_error/README.md)
  `burst`.
- A limit of [inf](rate_limiter.md) allows everything, whatever the burst; a limit of zero, or less, or NaN gives the
  burst once and never refills it, and a wait for more than is left fails at once.
- A reservation that goes into debt is a promise kept by the bucket: the tokens are counted as taken, and the calls
  after it wait behind it until [cancel](../rate_limiter-reservation/cancel.md) gives them back.

## Member types

| Type | Definition |
|---|---|
| [reservation](../rate_limiter-reservation/README.md) | tokens taken ahead: when the caller may act, and their cancel |

## Member objects

| Object | Description |
|---|---|
| `inf` | the limit that allows everything: `std::numeric_limits<double>::infinity()` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](rate_limiter.md) | constructs a full bucket of a limit and a burst, or a handle of the same bucket |
| `(destructor)` | lets go of the handle; the bucket is the collector's once no handle holds it |

#### Tokens

| Function | Description |
|---|---|
| [allow](allow.md) | takes tokens now, or says no |
| [reserve](reserve.md) | takes tokens for the moment they come and says when |
| [acquire](acquire.md) | waits for tokens, in a task or on a thread, until a stop |

#### Settings

| Function | Description |
|---|---|
| [set_limit](set_limit.md) | changes the limit, the tokens of now kept |
| [set_burst](set_burst.md) | changes the burst, the tokens of now kept up to it |

#### Observers

| Function | Description |
|---|---|
| [limit](limit.md) | the tokens a second |
| [burst](burst.md) | the most tokens the bucket holds |
| [tokens](tokens.md) | the tokens there now |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator!=](operator_cmp.md) | checks whether two handles are the same bucket |

## Complexity

`allow` and `reserve` are a read of the clock and one compare-exchange, constant, retried after a pause when another
thread changed the word first. `acquire` is that, and a timer when the tokens are not there. A change of the limit or
the burst makes a new set of constants, one allocation, and the bucket makes one by itself every 26 days of the
clock.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

async::task<int> client(async::rate_limiter lim, async::stop_token stop) {
    int sent = 0;
    while (co_await lim.acquire(1, stop)) {  // a request when the bucket allows
        ++sent;
    }
    co_return sent;
}

int main() {
    async::manual_clock clock;
    clock.install();
    async::rate_limiter lim(10, 3);  // 10 a second, 3 at once
    async::stop_source stop;
    stop.stop_after(950ms);
    auto t = async::spawn(client(lim, stop.token()));
    for (int i : range(10)) {
        clock.advance(100ms);
    }
    println("{} requests", t.wait());  // 3 at once, then one every 100 ms
}
```

Output:

```text
12 requests
```

## See also

- [rate_error](../rate_error/README.md): why a wait for tokens gave up
- [semaphore](../semaphore/README.md): a number of permits, given back by hand
- [tick](../tick.md), [every](../every.md): a signal every period
- [manual_clock](../manual_clock/README.md): the time of a test
