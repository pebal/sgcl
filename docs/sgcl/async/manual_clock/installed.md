[sgcl](../../README.md) › [async](../README.md) › [manual_clock](../manual_clock.md)

# sgcl::async::manual_clock::installed

```cpp
bool installed() const noexcept;
```

Checks whether this clock is the module's time: installed by [install](install.md) and not yet uninstalled.

## Parameters

None.

## Return value

`true` when this clock is installed, `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::manual_clock clock;
    println("{}", clock.installed());
    clock.install();
    println("{}", clock.installed());
    clock.uninstall();
    println("{}", clock.installed());
}
```

Output:

```text
false
true
false
```

## See also

- [install](install.md), [uninstall](uninstall.md)
- [sgcl::async::manual_clock](../manual_clock.md)
