[sgcl](../../README.md) › [io](../README.md) › [file_lock](README.md)

# sgcl::io::file_lock::offset

```cpp
uint64_t offset() const noexcept;
```

What the guard holds: the first byte of the range locked; 0 for the whole file. It says what was locked, after an unlock too.

## Parameters

None.

## Return value

The first byte of the range locked; 0 for the whole file.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::file_lock held = io::lock_file("o.lock", {.offset = 100, .length = 10}).value();
    println("{} {}", held.offset(), held.length());
}
```

Output:

```text
100 10
```

## See also

- [lock_options](../lock_options.md)
- [sgcl::io::file_lock](README.md)
