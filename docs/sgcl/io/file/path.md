[sgcl](../../README.md) › [io](../README.md) › [file](../file.md)

# sgcl::io::file::path

```cpp
const string& path() const noexcept;
```

The path the file was opened with, as it was given; the name given to [from_fd](../from_fd.md); `"pipe"` for the
ends of a [pipe](../pipe.md). It names the file in the errors of its operations (`read log.txt: ...`).

## Parameters

None.

## Return value

The path or the name, empty for a file made by `from_fd` without one.

## Complexity

Constant.

## Exceptions

None.

## Notes

The path is what the file was opened by, not where it is now: a file renamed or removed while open keeps its path.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::file f = io::create("notes.txt");
    println("{}", f.path());
    io::rename("notes.txt", "kept.txt");
    println("{}", f.path());

    auto [in, out] = io::pipe().value();
    println("{} {}", in.path(), out.path());
}
```

Output:

```text
notes.txt
notes.txt
pipe pipe
```

## See also

- [stat](stat.md): the file's `name`, the base of its path
- [from_fd](../from_fd.md): a file named by the caller
- [sgcl::io::file](../file.md)
