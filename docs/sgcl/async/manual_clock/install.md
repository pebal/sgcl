[sgcl](../../README.md) › [async](../README.md) › [manual_clock](README.md)

# sgcl::async::manual_clock::install

```cpp
void install() noexcept;
```

Makes this clock the module's time. The manual time starts at the steady clock's now, so a point computed before
the install is still meaningful after it; from here on it stands still, `sgcl::clock::now()` returns it, and it
moves only by [advance](advance.md) and [advance_to](advance_to.md). The wall time of the `time` module stands
still with it. The timer thread, when it runs, is told: it reads the time again, and from then on sleeps until an
advance wakes it rather than until a point of the steady clock.

One manual clock is installed at a time: a second install, of this clock or another, is an error (an assertion in
debug builds). The destructor uninstalls.

## Parameters

None.

## Return value

None.

## Complexity

Constant, and a handshake with the timer thread when it runs.

## Exceptions

None. The calls of the timer thread's `std::mutex` and `std::condition_variable` fail only on a lock the caller
holds already, which the module never takes twice.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    async::manual_clock clock;
    clock.install();
    time_point before = sgcl::clock::now();
    this_thread::sleep_for(5ms);  // the real time moves; the module's does not
    println("{}", sgcl::clock::now() == before);
    clock.advance(5ms);
    println("{}", sgcl::clock::now() - before == 5ms);
}
```

Output:

```text
true
true
```

## See also

- [uninstall](uninstall.md): the steady clock again
- [installed](installed.md): whether this clock is the module's time
- [sgcl::async::manual_clock](README.md)
