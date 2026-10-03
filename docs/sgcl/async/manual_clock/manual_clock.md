[sgcl](../../README.md) › [async](../README.md) › [manual_clock](../manual_clock.md)

# sgcl::async::manual_clock::manual_clock

```cpp
/*(1)*/ manual_clock() = default;
/*(2)*/ manual_clock(const manual_clock&) = delete;
```

1. A clock not yet installed: the module's time is still the steady clock's until [install](install.md).
2. The clock is not copyable, and not movable or assignable either: the state it stands for is the library's, one
   for the process, and the object that installed it is the one that uninstalls it.

## Parameters

None.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include <type_traits>

using namespace sgcl;

int main() {
    async::manual_clock clock;
    println("{}", clock.installed());
    println("{}", std::is_copy_constructible_v<async::manual_clock>);
    println("{}", std::is_move_constructible_v<async::manual_clock>);
}
```

Output:

```text
false
false
false
```

## See also

- [install](install.md): the clock made the module's time
- [sgcl::async::manual_clock](../manual_clock.md)
