[sgcl](../../README.md) › [async](../README.md) › [manual_clock](README.md)

# sgcl::async::manual_clock::now

```cpp
time_point now() const noexcept;
```

Returns the manual time: what `sgcl::clock::now()` returns while this clock is installed. The clock must be
installed (an assertion in debug builds).

## Parameters

None.

## Return value

The manual time, a `time_point` of `sgcl::clock`: the steady clock's now at the install, moved on by every
advance since.

## Complexity

Constant: one load.

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
    time_point start = clock.now();
    clock.advance(90s);
    println("{}", duration(clock.now() - start));
    println("{}", clock.now() == sgcl::clock::now());
}
```

Output:

```text
1m30s
true
```

## See also

- [advance](advance.md), [advance_to](advance_to.md): the time moved
- [clock](../../core/clock/README.md): `sgcl::clock::now()`, which returns it while the clock is installed
- [sgcl::async::manual_clock](README.md)
