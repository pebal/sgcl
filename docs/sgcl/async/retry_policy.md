[sgcl](../README.md) › [async](README.md)

# sgcl::async::retry_policy

```cpp
#include "sgcl/async/retry.h"   // or "sgcl/async.h"

namespace sgcl::async {
    struct retry_policy {
        size_t attempts = 5;
        duration max_elapsed = duration::zero();
        duration initial = 100ms;
        duration max_delay = 10s;
        double multiplier = 2;
        async::jitter jitter = jitter::full;
    };
}
```

`sgcl::async::retry_policy` is how a [retry](retry.md) goes on: how many attempts, for how long, and how long it waits
between two. The *n*-th wait is drawn below min(`max_delay`, `initial` × `multiplier`^(*n* − 1)) by the
[jitter](jitter.md). A plain value with no tracked word, so a policy may be a constant of the program; filled by
designated initializers: `{.attempts = 8, .initial = 50ms}`.

## Member objects

| Member | Description |
|---|---|
| `attempts` | the attempts in all, the first included; 0 has no limit. 5 by default |
| `max_elapsed` | no attempt starts later than this after the first: a wait that would end past it ends the retry at once instead; zero, the default, has no limit |
| `initial` | the first wait before the jitter; zero or less waits nothing. 100 ms by default |
| `max_delay` | the longest wait; zero or less waits nothing. 10 s by default |
| `multiplier` | the growth of the waits from one to the next; below one (or NaN) is one. 2 by default |
| `jitter` | how a wait is drawn: [full](jitter.md), the default |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    async::retry_policy steady{
        .attempts = 4, .initial = 2ms, .multiplier = 1, .jitter = async::jitter::none};
    int calls = 0;
    auto failing = [&]() -> expected<int, int> { return unexpected(++calls); };
    auto start = clock::now();
    auto r = async::retry(failing, steady).wait();
    bool waited = clock::now() - start >= 6ms;  // three waits of 2 ms
    println("{} attempts, last error {}, waited: {}", calls, r.error(), waited);
}
```

Output:

```text
4 attempts, last error 4, waited: true
```

## See also

- [retry](retry.md): what takes it
- [jitter](jitter.md): its last field
