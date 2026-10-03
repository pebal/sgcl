[sgcl](../../README.md) › [io](../README.md) › [file](README.md)

# sgcl::io::file::operator bool

```cpp
explicit operator bool() const noexcept;
```

Checks whether the handle holds a file. A handle made by the default constructor holds none. One moved from still
holds the file, the same as the handle moved to: a move of the word is a copy, as a `tracked_ptr`'s is. A closed file
is still held ([is_closed](is_closed.md) tells it apart).

## Parameters

None.

## Return value

`true` when the handle holds a file, `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::file f;
    println("{}", static_cast<bool>(f));
    f = io::create("held.txt");
    f.close();
    println("{}", static_cast<bool>(f));
}
```

Output:

```text
false
true
```

## See also

- [(constructor)](file.md): a handle with no file
- [is_closed](is_closed.md): whether the file was closed
- [sgcl::io::file](README.md)
