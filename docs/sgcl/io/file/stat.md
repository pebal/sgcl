[sgcl](../../README.md) › [io](../README.md) › [file](README.md)

# sgcl::io::file::stat

```cpp
expected<file_info, error> stat() const noexcept;
```

What is known of the open file: the `fstat(2)` of the descriptor, as a [file_info](../file_info/README.md). The file is the
one the descriptor holds, even when its path names another file by now or none.

## Parameters

None.

## Return value

The [file_info](../file_info/README.md): its `name` the base of the file's [path](path.md), its `size`, `type`, `mode`
and `modified`. Or the [error](../error/README.md), its operation `stat` and its path the file's: `errc::closed` for a
closed file, otherwise the `errno` of `fstat(2)`.

## Complexity

Constant: one system call.

## Exceptions

None.

## Notes

It has no `async_` form. [io::stat](../stat.md) of a path, which waits as a read does on a slow or network disk,
has one.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::write_file("report.txt", "twelve bytes");
    io::file f = io::open("report.txt");
    io::file_info info = f.stat().value();
    println("{} {} {}", info.name, info.size, info.is_regular());

    io::remove("report.txt");
    println("{}", f.stat()->size);

    auto [in, out] = io::pipe().value();
    println("{}", in.stat()->type == io::file_type::fifo);
}
```

Output:

```text
report.txt 12 true
12
true
```

## See also

- [file_info](../file_info/README.md): what it returns
- [stat](../stat.md): the same of a path
- [sgcl::io::file](README.md)
