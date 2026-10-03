[sgcl](../../README.md) › [async](../README.md) › [task](README.md)

# sgcl::async::task\<T\>::done

```cpp
bool done() const noexcept;
```

Checks whether the coroutine has ended, with a value or with an exception; an empty task is done, as nothing of it
is left to run. It may be called from any thread, and it never waits: a thread that needs the value calls
[wait](wait.md), which blocks without spinning.

## Parameters

None.

## Return value

`true` when the coroutine has ended or the task is empty, `false` otherwise.

## Complexity

Constant: one load.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include <coroutine>
#include <stdexcept>

using namespace sgcl;

async::task<int> fails() {
    co_await std::suspend_always{};
    throw std::runtime_error("no value");
}

int main() {
    async::task<int> empty;
    println("{}", empty.done());

    async::task<int> t = fails();
    println("{}", t.done());
    t.resume();
    println("{}", t.done());
    t.resume();
    println("{}", t.done());  // ended, by the exception
    try {
        t.result();
    } catch (const std::exception& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
true
false
false
true
no value
```

## See also

- [wait, operator co_await](wait.md): waits for the end
- [result](result.md): the value, or what the coroutine threw
- [sgcl::async::task\<T\>](README.md)
