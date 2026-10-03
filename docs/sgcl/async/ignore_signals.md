[sgcl](../README.md) › [async](README.md)

# sgcl::async::ignore_signals

```cpp
#include "sgcl/async/signal.h"   // or "sgcl/async.h"

namespace sgcl::async {
    void ignore_signals(std::initializer_list<int> numbers) noexcept;
}
```

Sets the signals of `numbers` to be ignored by the process, Go's `signal.Ignore`: `ignore_signals({SIGPIPE})`,
say, so that a write to a closed pipe is an error of the write rather than the end of the program. The channels
registered for the numbers by [signals](signals.md) are forgotten, and not closed, since a channel may serve other
numbers still. The disposition the number had before is saved, unless an earlier registration saved it already,
and [reset_signals](reset_signals.md) gives it back.

## Parameters

| Parameter | Description |
|---|---|
| `numbers` | the signals to ignore |

## Return value

None.

## Complexity

Linear in `numbers`: a `sigaction` per number.

## Exceptions

None. The module's `std::mutex` fails only on a lock the caller holds already, which the module never takes twice.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include <csignal>

using namespace sgcl;

int main() {
    async::ignore_signals({SIGUSR2});
    std::raise(SIGUSR2);  // the default action would end the program
    println("still running");
    async::reset_signals({SIGUSR2});
}
```

Output:

```text
still running
```

## See also

- [reset_signals](reset_signals.md): the disposition from before back
- [signals](signals.md): the signals as a channel
