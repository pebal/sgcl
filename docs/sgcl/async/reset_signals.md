[sgcl](../README.md) › [async](README.md)

# sgcl::async::reset_signals

```cpp
#include "sgcl/async/signal.h"   // or "sgcl/async.h"

namespace sgcl::async {
    void reset_signals(std::initializer_list<int> numbers = {}) noexcept;
}
```

Gives the signals of `numbers` back the disposition they had before the first [signals](signals.md) registration
for them, the default action or a handler the program had installed, Go's `signal.Reset`; an empty list, the
default, does it for every number registered. The channels registered for the numbers are forgotten, and not
closed, since a channel may serve other numbers still: the registration no longer holds them, and nothing more is
sent on them for these numbers. A number [ignore_signals](ignore_signals.md) set to be ignored gets its
disposition from before as well. A number never registered nor ignored is left as it is.

## Parameters

| Parameter | Description |
|---|---|
| `numbers` | the signals to give back; every one registered, for an empty list |

## Return value

None.

## Complexity

Linear in `numbers`, or in the numbers registered for an empty list: a `sigaction` per number.

## Exceptions

None. The module's `std::mutex` fails only on a lock the caller holds already, which the module never takes twice.

## Notes

At the end of the program the dispositions are given back without a call.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include <csignal>

using namespace sgcl;

volatile std::sig_atomic_t handled = 0;

int main() {
    std::signal(SIGUSR1, [](int) { handled = 1; });  // the program's own handler

    async::channel<int> usr = async::signals({SIGUSR1});
    std::raise(SIGUSR1);
    println("{} {}", *usr.receive().wait() == SIGUSR1, int(handled));

    async::reset_signals({SIGUSR1});
    std::raise(SIGUSR1);
    println("{}", int(handled));
}
```

Output:

```text
true 0
1
```

## See also

- [signals](signals.md): the registration
- [ignore_signals](ignore_signals.md): the signals ignored
