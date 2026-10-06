[sgcl](../README.md) › [async](README.md)

# sgcl::async::jitter

```cpp
#include "sgcl/async/retry.h"   // or "sgcl/async.h"

namespace sgcl::async {
    enum class jitter : uint8_t {
        none,
        full,
        equal,
        decorrelated
    };
}
```

How a [retry](retry.md) draws a wait, the field `jitter` of its [retry_policy](retry_policy.md): the four of AWS's
"Exponential Backoff And Jitter". With *v* = min(`max_delay`, `initial` × `multiplier`^(*n* − 1)) for the *n*-th
wait, the random part spreads the callers that one failure made synchronous, so that they do not come back all at
once. The draws are a generator of each thread's own, spread and not secret.

| Value | Description |
|---|---|
| `none` | *v*: the plain exponential |
| `full` | uniform in [0, *v*]: the least work for the server, the default |
| `equal` | *v*/2 plus uniform in [0, *v*/2]: never less than half |
| `decorrelated` | uniform in [`initial`, 3 × the previous wait], at most `max_delay`: grows from the wait before, not from the count |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    async::retry_policy p{.attempts = 3, .initial = 1ms, .jitter = async::jitter::equal};
    int calls = 0;
    auto start = clock::now();
    (void)async::retry([&]() -> expected<int, int> { return unexpected(++calls); }, p).wait();
    println("at least 1.5 ms of waits: {}", clock::now() - start >= 1500us);  // 0.5..1 then 1..2
}
```

Output:

```text
at least 1.5 ms of waits: true
```

## See also

- [retry_policy](retry_policy.md): the policy it is a field of
- [retry](retry.md)
