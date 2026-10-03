[sgcl](../../README.md) › [io](../README.md) › [file](README.md)

# sgcl::io::file::file

```cpp
file() noexcept = default;           // (1)
file(const file& other) noexcept;    // (2), implicitly declared
file(file&& other) noexcept;         // (3), implicitly declared
```

1. A handle that holds no file: `!f`. An operation on it is a contract violation (debug builds assert); it is given
   a file by an assignment.
2. A handle of the same file as `other`: one descriptor, one position, shared.
3. The same, `other` left holding no file.

A file with a descriptor is made by [open](../open.md), [create](../create.md), [temp_file](../temp_file.md),
[from_fd](../from_fd.md) and [pipe](../pipe.md).

## Parameters

| Parameter | Description |
|---|---|
| `other` | the handle whose file this one shares |

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::file none;
    println("{}", static_cast<bool>(none));

    io::file f = io::create("log.txt");
    io::file same = f;
    same.write("written through the copy");
    f.close();
    println("{} {}", same.is_closed(), same == f);
    println("{}", *io::read_text("log.txt"));
}
```

Output:

```text
false
true true
written through the copy
```

## See also

- [open](../open.md), [create](../create.md): what makes a file
- [operator=](operator_assign.md): the handle made the same file as another
- [operator bool](operator_bool.md): whether the handle holds a file
- [sgcl::io::file](README.md)
