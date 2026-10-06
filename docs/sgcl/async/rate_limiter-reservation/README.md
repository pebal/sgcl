[sgcl](../../README.md) › [async](../README.md) › [rate_limiter](../rate_limiter/README.md)

# sgcl::async::rate_limiter::reservation

```cpp
#include "sgcl/async/rate_limiter.h"   // or "sgcl/async.h"

namespace sgcl::async {
    class rate_limiter {
    public:
        class reservation;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::async::rate_limiter::reservation` is what [reserve](../rate_limiter/reserve.md) gives: tokens taken for the
moment they come, Go's `rate.Reservation`. It says whether the tokens were taken ([ok](ok.md)), when the caller may
act ([time](time.md), [delay](delay.md)), and gives the tokens back when the caller will not act
([cancel](cancel.md)). The bucket counts the tokens as taken from the moment of the reservation: the calls after it
come after it, whether its caller acts or not, until it is cancelled.

## Rules

- Move-only, so that its tokens are given back once. A default-constructed, a moved-from and a cancelled
  reservation is not ok.
- A reservation holds the constants of the bucket it was made in, not the bucket: a change of the limit or the
  burst after it leaves its time as it was, and its cancel gives nothing back.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](rate_limiter-reservation.md) | constructs a reservation that is not ok, or takes one over |
| `(destructor)` | lets go of the reservation; its tokens stay taken |
| [operator=](operator_assign.md) | takes another reservation over |

#### Observers

| Function | Description |
|---|---|
| [ok](ok.md) | checks whether the tokens were taken |
| [time](time.md) | the point of the clock the caller may act at |
| [delay](delay.md) | the time from now until then |

#### Modifiers

| Function | Description |
|---|---|
| [cancel](cancel.md) | gives the tokens back |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    async::manual_clock clock;
    clock.install();
    async::rate_limiter lim(2, 1);
    (void)lim.allow();
    auto r = lim.reserve();
    if (r.delay() > 100ms) {  // too long to wait: not now
        r.cancel();
    }
    println("{} {}", r.ok(), lim.tokens());
}
```

Output:

```text
false 0
```

## See also

- [reserve](../rate_limiter/reserve.md): what makes one
- [acquire](../rate_limiter/acquire.md): a reservation waited for
- [rate_limiter](../rate_limiter/README.md)
