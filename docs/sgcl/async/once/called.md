[sgcl](../../README.md) › [async](../README.md) › [once](README.md)

# sgcl::async::once::called

```cpp
bool called() const noexcept;
```

Checks whether the call is done: whether the first caller's function has returned or thrown. While it runs,
`called()` is still `false`; a function that threw has been called, as a promise set with an exception is set.

## Parameters

None.

## Return value

`true` once the function has returned or thrown, `false` before.

## Complexity

Constant: one atomic load.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::once init;
    println("{}", init.called());
    try {
        init.call([] { throw runtime_error("no config"); }).wait();
    } catch (const runtime_error& e) {
        println("{}", e.what());
    }
    println("{}", init.called());
}
```

Output:

```text
false
no config
true
```

## See also

- [call](call.md): runs the function once
- [sgcl::async::once](README.md)
