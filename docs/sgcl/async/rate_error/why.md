[sgcl](../../README.md) › [async](../README.md) › [rate_error](README.md)

# sgcl::async::rate_error::why

```cpp
constexpr reason why() const noexcept;
```

Returns why the wait gave up, for a switch: a stop ends the caller's work, a deadline may be retried later with more
time, a burst is a request the limiter can never serve.

## Parameters

None.

## Return value

The [reason](../rate_error-reason.md).

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::stop_source s;
    s.request_stop();
    async::rate_limiter lim(10, 1);
    auto r = lim.acquire(1, s.token()).wait();
    switch (r.error().why()) {
        case async::rate_error::reason::stopped: println("stopped"); break;
        case async::rate_error::reason::deadline: println("too late"); break;
        case async::rate_error::reason::burst: println("too many"); break;
    }
}
```

Output:

```text
stopped
```

## See also

- [message](message.md): the reason in words
- [sgcl::async::rate_error](README.md)
