[sgcl](../../README.md) › [core](../README.md) › [clock](README.md)

# sgcl::clock::now

```cpp
static time_point now() noexcept;
```

The library's time: the steady clock's, or the manual clock's while one is installed. The difference of two
points is a `std::chrono` duration, which a [duration](../duration/README.md) takes as it is.

## Parameters

None.

## Return value

The current point of the library's time.

## Complexity

Constant: one relaxed load of the manual clock's flag, then `std::chrono::steady_clock::now()`, or a load of the
manual time.

## Exceptions

None.

## Notes

Every wait of the library reads its deadline here, so a test that installs a
[manual_clock](../../async/manual_clock/README.md) stops the time the library sees, and `advance` moves it.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    async::manual_clock test_clock;
    test_clock.install();  // the library's time stands still from here

    time_point start = sgcl::clock::now();
    this_thread::sleep_for(10ms);  // real time passes, the library's does not
    println("{}", sgcl::clock::now() == start);

    test_clock.advance(30s);
    duration moved = sgcl::clock::now() - start;
    println("{}", moved);
}
```

Output:

```text
true
30s
```

## See also

- [manual_clock](../../async/manual_clock/README.md): the clock of a test
- [duration](../duration/README.md): what the difference of two points converts to
- [sgcl::clock](README.md)
