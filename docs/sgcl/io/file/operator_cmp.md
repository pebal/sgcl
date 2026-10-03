[sgcl](../../README.md) › [io](../README.md) › [file](README.md)

# sgcl::io::operator==, operator!= (sgcl::io::file)

```cpp
friend bool operator==(const file& a, const file& b) noexcept;
```

Checks whether `a` and `b` are handles of the same file: copies of one handle, not two files opened on one path.
Two handles that hold no file are equal. `a != b` is `!(a == b)`, rewritten by the compiler.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the handles to compare |

## Return value

`true` when the two hold the same file, or both none; `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::write_file("shared.txt", "text");
    io::file f = io::open("shared.txt");
    io::file copy = f;
    io::file other = io::open("shared.txt");
    println("{} {} {}", f == copy, f == other, f != other);
    println("{}", io::file() == io::file());
}
```

Output:

```text
true false true
true
```

## See also

- [operator=](operator_assign.md): a handle made the same file as another
- [sgcl::io::file](README.md)
