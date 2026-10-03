[sgcl](../../README.md) › [io](../README.md) › [process](../process.md)

# sgcl::io::process::signal

```cpp
expected<void, error> signal(int sig) const noexcept;
```

Sends the signal `sig` to the process, Go's `Process.Signal`, the system's `kill(pid, sig)`. A signal while a wait is
in progress is what ends the wait, when the signal ends the process. Once the process was waited for or released,
the call is refused: its id may be another process's by then.

## Parameters

| Parameter | Description |
|---|---|
| `sig` | the signal: `SIGTERM`, `SIGINT`, `SIGKILL`... |

## Return value

Nothing, or the [error](../error.md): `errc::process_done` after the wait or the release, else the `errno` of
`kill` in the system category; the operation is `signal`.

## Complexity

Constant: one call to the system.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include <csignal>

using namespace sgcl;

int main() {
    io::command sleeper("sleep", "10");
    (void)sleeper.start();
    (void)sleeper.process.signal(SIGTERM);
    auto ended = sleeper.process.wait();
    println("{} {}", ended->signaled(), ended->signal() == SIGTERM);
    println("{}", sleeper.process.signal(SIGTERM).error().message());
}
```

Output:

```text
true true
signal: process already finished
```

## See also

- [kill](kill.md): `SIGKILL`
- [command](../command.md): `stop`, a kill on a stop token; `set_pgid`, a group to signal as one
- [sgcl::io::process](../process.md)
