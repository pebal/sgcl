[sgcl](../../../README.md) › [io](../../README.md) › [mixin](../README.md) › [seeker](../seeker.md)

# sgcl::io::mixin::seeker\<Derived\>::size

```cpp
expected<uint64_t, error> size() noexcept(/* see below */);
```

Returns the size of this stream, the position kept: three seeks, to the position (`seek(0, seek_from::current)`), to
the end (`seek(0, seek_from::end)`) and back. A file's size is also in its [stat](../../file/stat.md), which asks the
system without moving anything. Declared noexcept when `Derived`'s `seek` is.

`io::buffer` has a `size()` of its own, the number of bytes it holds, which hides this one.

## Parameters

None.

## Return value

The size, the position of the end, or the error of the first seek that failed, as the stream gave it; after a
failure the position may have moved.

## Complexity

Three seeks.

## Exceptions

What `Derived`'s `seek` throws; none when it is noexcept, as a file's is.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::write_file("size.txt", "0123456789");
    io::file digits = *io::open("size.txt");
    digits.seek(4);
    println("{} {}", *digits.size(), *digits.tell());
}
```

Output:

```text
10 4
```

## See also

- [tell](tell.md): the position
- [file::stat](../../file/stat.md): the size and the rest of what the system knows of a file
- [sgcl::io::mixin::seeker\<Derived\>](../seeker.md)
