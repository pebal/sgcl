[sgcl](../../README.md) › [io](../README.md) › [file](README.md)

# sgcl::io::file::close

```cpp
expected<void, error> close() const noexcept;
```

Ends the file, from any thread or task: no operation starts after it, and a task or a thread waiting in `read` or
`write` wakes to `errc::closed`. The descriptor is given back to the kernel by whoever lets go of it last,
`close()` itself or the last operation in progress (a count of the operations and a closing bit in one word, as
net's sockets hold theirs), so that no read in progress lands on the number the kernel gives to the next file
opened. A second `close` does nothing and succeeds.

A file that is not closed keeps its descriptor until the collector finds the file dead and its destructor, on the
collector's thread after that sweep, closes it — later than the last use. A file that is done is closed: `close()`
releases the descriptor now and reports what the deferred close could not.

## Parameters

None.

## Return value

Nothing, or the [error](../error/README.md) of `close(2)`, its operation `close` and its path the file's (`EIO`, or on some
file systems the error of a write the kernel had kept).

## Complexity

Constant: one system call, or none when an operation in progress makes it.

## Exceptions

None.

## Notes

A close never waits, so it has no `async_` form: closing a pipe that a task reads is how the task is woken.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<> listen(io::file in) {
    byte b[16];
    auto n = co_await in.async_read(b);  // waits: nothing is written to the pipe
    println("{}", n.error().message());
}

int main() {
    auto [in, out] = io::pipe().value();
    auto waiting = async::spawn(listen(in));
    in.close();
    waiting.wait();

    io::file f = io::create("closed.txt");
    println("{} {}", static_cast<bool>(f.close()), static_cast<bool>(f.close()));
    println("{} {}", f.is_closed(), f.fd());
    println("{}", f.write("late").error().message());
}
```

Output:

```text
read pipe: stream closed
true true
true -1
write closed.txt: stream closed
```

## See also

- [is_closed](is_closed.md): whether the file was closed
- [fd](fd.md): `-1` once closed
- [sgcl::io::file](README.md)
