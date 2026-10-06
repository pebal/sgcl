[sgcl](../../README.md) › [async](../README.md)

# sgcl::async::rate_error

```cpp
#include "sgcl/async/rate_limiter.h"   // or "sgcl/async.h"

namespace sgcl::async {
    class rate_error {
    public:
        enum class reason : uint8_t;
    };
}
```

Why a [rate_limiter](../rate_limiter/README.md)'s [acquire](../rate_limiter/acquire.md) gave up without the tokens: what
it gives in its `expected<void, rate_error>`. A value, not an exception, as every failure of the library is an
[expected](../../core/expected/README.md); it carries its [reason](../rate_error-reason.md) alone, which
[why](why.md) returns and [message](message.md) puts in words. Go's `Wait` returns an error of a sentence for each;
here they are one list, read by a switch.

## Member types

| Type | Definition |
|---|---|
| [reason](../rate_error-reason.md) | why the wait gave up: `stopped`, `deadline`, `burst` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](rate_error.md) | constructs the error of a reason |
| [why](why.md) | the reason |
| [message](message.md) | the text of the error |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator!=](operator_cmp.md) | compares two errors by their reasons |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::rate_limiter lim(10, 2);
    auto r = lim.acquire(5).wait();
    if (!r && r.error().why() == async::rate_error::reason::burst) {
        println("{}", r.error().message());
    }
}
```

Output:

```text
more tokens than the burst
```

## See also

- [rate_limiter](../rate_limiter/README.md): the limiter whose wait gives it
- [stopped](../stopped/README.md), [timed_out](../timed_out/README.md): the errors of the races
