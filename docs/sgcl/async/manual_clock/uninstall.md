[sgcl](../../README.md) › [async](../README.md) › [manual_clock](../manual_clock.md)

# sgcl::async::manual_clock::uninstall

```cpp
void uninstall() noexcept;
```

Makes the steady clock the module's time again, when this clock is installed; otherwise does nothing. The
destructor calls it, so a clock installed at the top of a test is uninstalled at its end. The timer thread, when
it runs, reads the time again. A timer armed under the manual clock and not yet due keeps its point, which the
steady clock reaches later, since the manual time started from the steady clock's now at the install and moved
only forward.

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
    time_point frozen = sgcl::clock::now();
    this_thread::sleep_for(5ms);
    clock.uninstall();
    println("{}", clock.installed());
    println("{}", sgcl::clock::now() - frozen >= 5ms);  // the steady clock went on meanwhile
}
```

Output:

```text
false
true
```

## See also

- [install](install.md): the manual time again
- [sgcl::async::manual_clock](../manual_clock.md)
