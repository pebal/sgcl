[sgcl](../README.md) › [async](README.md) › [rate_error](rate_error/README.md)

# sgcl::async::rate_error::reason

```cpp
#include "sgcl/async/rate_limiter.h"   // or "sgcl/async.h"

namespace sgcl::async {
    class rate_error {
    public:
        enum class reason : uint8_t {
            stopped,
            deadline,
            burst
        };
    };
}
```

Why a [acquire](rate_limiter/acquire.md) for tokens gave up, as [why](rate_error/why.md) returns it. In every case the
wait took no tokens: those it had reserved went back by the rules of
[cancel](rate_limiter-reservation/cancel.md).

| Value | Description |
|---|---|
| `stopped` | the token was stopped, before the wait or during it |
| `deadline` | the tokens would come after the token's deadline, never (a limit of zero), or further than the limiter counts |
| `burst` | more tokens than the burst, which a finite limit never gives at once |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::rate_limiter lim(0, 1);  // the burst once, never refilled
    (void)lim.allow();
    auto r = lim.acquire().wait();
    println("{}", r.error().why() == async::rate_error::reason::deadline);
}
```

Output:

```text
true
```

## See also

- [rate_error](rate_error/README.md)
