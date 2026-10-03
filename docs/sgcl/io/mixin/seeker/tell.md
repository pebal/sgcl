[sgcl](../../../README.md) › [io](../../README.md) › [mixin](../README.md) › [seeker](README.md)

# sgcl::io::mixin::seeker\<Derived\>::tell

```cpp
expected<uint64_t, error> tell() noexcept(/* see below */);
```

Returns the position of this stream, counted from its first byte: `seek(0, seek_from::current)`, which moves nothing.
It is Go's `Seek(0, io.SeekCurrent)` and `std::istream::tellg`. Declared noexcept when `Derived`'s `seek` is.

## Parameters

None.

## Return value

The position, or the error of the seek, as the stream gave it (`errc::closed` for a closed file).

## Complexity

One seek.

## Exceptions

What `Derived`'s `seek` throws; none when it is noexcept, as a file's and a buffer's are.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::write_file("tell.txt", "0123456789");
    io::file digits = *io::open("tell.txt");
    println("{}", *digits.tell());

    vector<byte> four(4);
    digits.read_full(four);
    println("{}", *digits.tell());

    digits.close();
    println("{}", digits.tell().error().message());
}
```

Output:

```text
0
4
seek tell.txt: stream closed
```

## See also

- [size](size.md): the size, the position kept
- [rewind](rewind.md): back to the first byte
- [sgcl::io::mixin::seeker\<Derived\>](README.md)
