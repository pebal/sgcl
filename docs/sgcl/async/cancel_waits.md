[sgcl](../README.md) › [async](README.md)

# sgcl::async::cancel_waits

```cpp
#include "sgcl/async/reactor.h"   // or "sgcl/async.h"

namespace sgcl::async {
    void cancel_waits(int fd);
}
```

Ends the waits registered on `fd` by [readable](readable.md) and [writable](writable.md) with nothing: their events
are set, and the tasks and threads that wait on them are woken to look at the descriptor again. A program calls it
before it closes a descriptor it waits on this way. The kernel drops the registration of a closed descriptor
silently, so nothing would signal the waits, and the reactor's record of them would take the waits of the next
descriptor given the same number; an event the kernel gives for such a registration meanwhile is dropped.

## Parameters

| Parameter | Description |
|---|---|
| `fd` | the descriptor about to be closed |

## Return value

None.

## Complexity

Linear in the number of waits registered on `fd`.

## Exceptions

`std::system_error` when a wake has to start the scheduler's workers and a thread cannot be started.

## Notes

A wait woken this way finds its event set as a ready one does: the operation that follows, a read that answers
`EAGAIN` on a descriptor in non-blocking mode, says there was nothing. The descriptors of the `io` and `net` modules
release their slot on the reactor themselves when they are closed.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

using namespace sgcl;

int main() {
    int fd[2];
    if (::pipe(fd) != 0) {
        return 1;
    }
    ::fcntl(fd[0], F_SETFL, O_NONBLOCK);
    async::event ready = async::readable(fd[0]);  // nobody writes: it would wait for good
    async::cancel_waits(fd[0]);
    ready.wait();
    char c;
    bool nothing = ::read(fd[0], &c, 1) < 0 && errno == EAGAIN;
    println("woken: {}, nothing to read: {}", ready.is_set(), nothing);
    ::close(fd[0]);
    ::close(fd[1]);
}
```

Output:

```text
woken: true, nothing to read: true
```

## See also

- [readable](readable.md), [writable](writable.md): the waits it ends
- [event](event/README.md): what a wait is
