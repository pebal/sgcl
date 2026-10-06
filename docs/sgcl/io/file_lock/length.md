[sgcl](../../README.md) › [io](../README.md) › [file_lock](README.md)

# sgcl::io::file_lock::length

```cpp
uint64_t length() const noexcept;
```

What the guard holds: the bytes of the range locked; 0 for the whole file, or for a range to the end of the file and past it. It says what was locked, after an unlock too.

## Parameters

None.

## Return value

The bytes of the range locked; 0 for the whole file, or for a range to the end of the file and past it.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::file_lock whole = io::lock_file("l.lock").value();
    io::file_lock tail = io::lock_file("l2.lock", {.offset = 4096}).value();
    println("{} {} {}", whole.length(), tail.offset(), tail.length());
}
```

Output:

```text
0 4096 0
```

## See also

- [lock_options](../lock_options.md)
- [sgcl::io::file_lock](README.md)
