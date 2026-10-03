[sgcl](../../README.md) › [async](../README.md) › [promise](README.md)

# sgcl::async::promise\<T\>::set_value

```cpp
void set_value(const T& v) const;    // (1)
void set_value(T&& v) const;         // (2)
void set_value() const;              // (3), promise<void>
```

Sets the promise: the value goes into the promise's state and every waiter is woken, a task made ready on the
scheduler, a thread unparked, a select case served. The first `set_value` or `set_exception` claims the promise with a
compare-exchange; a later one is an error, asserted in a debug build and ignored in a release build, where the first
value stands. Any thread may set the promise, a callback of a platform's own thread included, at any time, before or
after the waits begin; the call returns at once.

1. Sets a copy of `v`.
2. Sets `v`, moved.
3. `promise<void>`: sets the completion.

## Parameters

| Parameter | Description |
|---|---|
| `v` | the value to set |

## Return value

None.

## Complexity

Constant, plus the wake of every waiter.

## Exceptions

- `std::system_error` when the set wakes a waiting task and the wake starts the scheduler's workers, one of which
  cannot be started.
- What the copy (1) or the move constructor (2) of `T` throws when the argument is made, before the promise is
  claimed: the promise is then not set.

A move of the value into the state that throws is not let out: the promise is set with that exception, which every
waiter gets in place of the value, since a promise claimed and never set would keep its waiters for good.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<string> greet(async::promise<string> name) {
    string& n = co_await name;  // suspended until the set, no thread held
    co_return "hello, " + n;
}

int main() {
    async::promise<string> name;
    async::task<string> greeting = async::spawn(greet(name));
    thread setter([name] { name.set_value("Ada"); });  // a copy of the handle
    println("{}", greeting.wait());
    setter.join();

    async::promise<> ready;
    ready.set_value();
    println("{}", ready.done());
}
```

Output:

```text
hello, Ada
true
```

## See also

- [set_exception](set_exception.md): sets an exception instead
- [wait, operator co_await](wait.md): what the waiters get
- [sgcl::async::promise\<T\>](README.md)
