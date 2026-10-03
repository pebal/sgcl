[sgcl](../../README.md) › [async](../README.md) › [promise](../promise.md)

# sgcl::async::promise\<T\>::done

```cpp
bool done() const noexcept;
```

Checks whether the promise is set, with a value or with an exception: whether a wait would return at once. The same
for `promise<void>`.

## Parameters

None.

## Return value

`true` once the value or the exception is in, `false` before.

## Complexity

Constant: one atomic load.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include <exception>
#include <stdexcept>

using namespace sgcl;

int main() {
    async::promise<int> value, failure;
    println("{} {}", value.done(), failure.done());

    value.set_value(1);
    failure.set_exception(std::make_exception_ptr(std::runtime_error("lost")));
    println("{} {}", value.done(), failure.done());
}
```

Output:

```text
false false
true true
```

## See also

- [wait, operator co_await](wait.md), [result](result.md): the value once it is in
- [on_done](on_done.md): the set as a case of a select
- [sgcl::async::promise\<T\>](../promise.md)
