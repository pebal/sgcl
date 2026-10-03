[sgcl](../../README.md) › [async](../README.md) › [promise](README.md)

# sgcl::async::promise\<T\>::set_exception

```cpp
void set_exception(std::exception_ptr e) const;
```

Sets the promise with an exception in place of a value: every waiter is woken, and `co_await p`, [wait](wait.md) and
[result](result.md) rethrow `e`, every time they are asked. The first `set_value` or `set_exception` claims the
promise; a later one is an error, asserted in a debug build and ignored in a release build. `e` is never null:
a null one is an error too, asserted in a debug build, since the promise would hold neither a value nor an
exception. The same for `promise<void>`.

## Parameters

| Parameter | Description |
|---|---|
| `e` | the exception the waiters get, from `std::current_exception()` or `std::make_exception_ptr`; not null |

## Return value

None.

## Complexity

Constant, plus the wake of every waiter.

## Exceptions

`std::system_error` when the set wakes a waiting task and the wake starts the scheduler's workers, one of which
cannot be started.

## Notes

The exception object is kept by the runtime, outside the managed heap: it carries its message as a `std::string`
and a value with tracked pointers in a [rooted](../../core/rooted/README.md) member
([The rules](../../core/README.md#the-rules), 1).

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include <exception>
#include <stdexcept>

using namespace sgcl;

async::task<> report(async::promise<int> result) {
    try {
        println("{}", co_await result);
    } catch (const std::runtime_error& e) {
        println("failed: {}", e.what());
    }
}

int main() {
    async::promise<int> result;
    async::task<> reporting = async::spawn(report(result));
    result.set_exception(std::make_exception_ptr(std::runtime_error("no connection")));
    reporting.wait();
}
```

Output:

```text
failed: no connection
```

## See also

- [set_value](set_value.md): sets the value
- [sgcl::async::promise\<T\>](README.md)
