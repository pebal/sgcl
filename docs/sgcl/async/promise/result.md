[sgcl](../../README.md) › [async](../README.md) › [promise](../promise.md)

# sgcl::async::promise\<T\>::result

```cpp
/*(1)*/ T& result() const;
/*(2)*/ void result() const;  // promise<void>
```

The result of a set promise: the value, or the exception rethrown. On a promise not set yet it waits first, blocking
the calling thread as [wait](wait.md) does; on a set one it returns at once, which makes it the read of the value in
the body of an [on_done](on_done.md) case, or after a wait.

1. Gives the value.
2. `promise<void>`: gives nothing, only rethrows.

## Parameters

None.

## Return value

- (1) A reference to the value in the promise's state: every reader reads the one value, and a lone reader may move
  it out.
- (2) None.

## Complexity

Constant on a set promise; a wait on one not set.

## Exceptions

What [set_exception](set_exception.md) set, rethrown, every time.

## Notes

On a promise not set yet, `result()` blocks the thread: it is not called from a task on a worker, which writes
`co_await p` instead (a debug build asserts). On a set promise it may be called anywhere.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include <exception>
#include <stdexcept>

using namespace sgcl;

int main() {
    async::promise<string> name;
    name.set_value("Ada");
    string& n = name.result();
    println("{} {}", n, &n == &name.result());  // the one value

    async::promise<> failed;
    failed.set_exception(std::make_exception_ptr(std::runtime_error("stopped")));
    try {
        failed.result();
    } catch (const std::runtime_error& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
Ada true
stopped
```

## See also

- [wait, operator co_await](wait.md): waits for the set
- [done](done.md): whether `result()` would wait
- [sgcl::async::promise\<T\>](../promise.md)
