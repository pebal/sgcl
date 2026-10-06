[sgcl](../../README.md) › [async](../README.md) › [rate_limiter](README.md)

# sgcl::async::rate_limiter::allow

```cpp
bool allow(size_t n = 1) noexcept;
```

Takes `n` tokens now when the bucket has them, and returns `true`; otherwise takes nothing and returns `false`. Go's
`Allow` and `AllowN`: the call for an event dropped when the rate is passed, a request answered with 429. It never
waits and never goes into debt: a bucket in debt from a [reservation](reserve.md) allows not even zero tokens, as
Go's does.

The bucket is one word, changed with one compare-exchange; a refusal reads it and changes nothing, so many threads
ask at once without a lock.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the tokens to take; more than the burst is never allowed at a finite limit |

## Return value

`true` when the tokens were taken, `false` when they are not there (nothing taken).

## Complexity

Constant: a read of the clock and one compare-exchange, retried after a pause when another thread changed the word
first.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    async::manual_clock clock;
    clock.install();
    async::rate_limiter lim(10, 2);
    println("{} {} {}", lim.allow(), lim.allow(), lim.allow());
    clock.advance(100ms);  // one token back
    println("{} {}", lim.allow(), lim.allow());
    println("{}", lim.allow(3));  // more than the burst: never
}
```

Output:

```text
true true false
true false
false
```

## See also

- [reserve](reserve.md): the tokens for the moment they come
- [acquire](acquire.md): the wait for them
- [sgcl::async::rate_limiter](README.md)
