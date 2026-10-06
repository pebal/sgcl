[sgcl](../../README.md) › [io](../README.md) › [file_lock](README.md)

# sgcl::io::file_lock::mode

```cpp
lock_mode mode() const noexcept;
```

What the guard holds: shared or exclusive, as [lock_options](../lock_options.md) asked ([lock_mode](../lock_mode.md)). It says what was locked, after an unlock too.

## Parameters

None.

## Return value

Shared or exclusive, as [lock_options](../lock_options.md) asked ([lock_mode](../lock_mode.md)).

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::file_lock held = io::lock_file("m.lock", {.mode = io::lock_mode::shared}).value();
    println("{}", held.mode() == io::lock_mode::exclusive);
}
```

Output:

```text
false
```

## See also

- [lock_options](../lock_options.md)
- [sgcl::io::file_lock](README.md)
