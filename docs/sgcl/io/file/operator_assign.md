[sgcl](../../README.md) › [io](../README.md) › [file](../file.md)

# sgcl::io::file::operator=

```cpp
/*(1)*/ file& operator=(const file& other) noexcept;   // implicitly declared
/*(2)*/ file& operator=(file&& other) noexcept;        // implicitly declared
```

Makes this handle one of the file `other` holds, or one that holds none when `other` holds none.

1. `other` keeps the file: the two handles share it.
2. `other` is left holding no file.

The file this handle held before is not closed: other handles may still use it, and a file nothing holds any more
is closed by the collector ([close](close.md)).

## Parameters

| Parameter | Description |
|---|---|
| `other` | the handle whose file this one takes |

## Return value

`*this`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::write_file("first.txt", "first");
    io::write_file("second.txt", "second");

    io::file current;
    println("{}", static_cast<bool>(current));
    current = io::open("first.txt");
    println("{}", *current.read_all_text());

    io::file kept = current;
    current = io::open("second.txt");
    println("{} {}", *current.read_all_text(), kept.path());
}
```

Output:

```text
false
first
second first.txt
```

## See also

- [(constructor)](file.md): a handle made empty or as a copy
- [operator==](operator_cmp.md): whether two handles are the same file
- [sgcl::io::file](../file.md)
