[sgcl](../../README.md) › [async](../README.md) › [rate_limiter](../rate_limiter/README.md) › [reservation](README.md)

# sgcl::async::rate_limiter::reservation::cancel

```cpp
void cancel() noexcept;
```

Gives the reservation's tokens back to the bucket, for a caller that will not act, and leaves the reservation not
[ok](ok.md): Go's `Cancel`, by Go's rules. The tokens a later reservation has counted on stay taken, since that
reservation's time was reckoned behind them; the rest go back. Nothing goes back once the reservation's time has
come (the tokens were the caller's to use), nor after a change of the limit or the burst. A second cancel does
nothing.

## Parameters

None.

## Return value

None.

## Complexity

Constant: one compare-exchange, retried when another thread changed the word first.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::manual_clock clock;
    clock.install();
    async::rate_limiter lim(10, 1);
    (void)lim.allow();
    auto a = lim.reserve();
    auto b = lim.reserve();
    println("{}", lim.tokens());
    a.cancel();  // b counts on a's place: nothing back
    println("{}", lim.tokens());
    b.cancel();  // the last: back
    println("{}", lim.tokens());
}
```

Output:

```text
-2
-2
-1
```

## See also

- [ok](ok.md): false after it
- [acquire](../rate_limiter/acquire.md): a stop of the wait cancels its reservation
- [sgcl::async::rate_limiter::reservation](README.md)
