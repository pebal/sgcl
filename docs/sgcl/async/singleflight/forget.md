[sgcl](../../README.md) › [async](../README.md) › [singleflight](README.md)

# sgcl::async::singleflight::forget

```cpp
void forget(const string& key) const noexcept;
```

Lets go of the call in flight for `key`: Go's `Forget`. The next caller of the key runs the function anew rather
than wait for the call, for a value known to be stale already; the callers that joined the call before still get its
result, and its end leaves the new call of the key in place. A key with no call in flight is left as it is.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to let go of |

## Return value

None.

## Complexity

A lookup and an erasure in the table.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

async::task<int> slow(int v) {
    co_await async::sleep(1s);
    co_return v;
}

async::task<int> ask(async::singleflight flights, int v) {
    co_return co_await flights.run("config", [v] { return slow(v); });
}

int main() {
    async::manual_clock clock;
    clock.install();
    async::singleflight flights;
    auto old_call = async::spawn(ask(flights, 1));
    clock.advance(500ms);
    flights.forget("config");  // the config changed: a new read
    auto new_call = async::spawn(ask(flights, 2));
    clock.advance(1s);
    println("{} {}", old_call.wait(), new_call.wait());
}
```

Output:

```text
1 2
```

## See also

- [run](run.md): the call
- [sgcl::async::singleflight](README.md)
