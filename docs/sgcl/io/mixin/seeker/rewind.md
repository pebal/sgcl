[sgcl](../../../README.md) › [io](../../README.md) › [mixin](../README.md) › [seeker](README.md)

# sgcl::io::mixin::seeker\<Derived\>::rewind

```cpp
expected<void, error> rewind() noexcept(/* see below */);
```

Moves the position of this stream back to its first byte: `seek(0, seek_from::begin)`, the position it returns
dropped. It is C's `rewind`, which reports what C's does not. Declared noexcept when `Derived`'s `seek` is.

## Parameters

None.

## Return value

Nothing, or the error of the seek, as the stream gave it.

## Complexity

One seek.

## Exceptions

What `Derived`'s `seek` throws; none when it is noexcept, as a file's and a buffer's are.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::write_file("again.txt", "read twice");
    io::file text = *io::open("again.txt");
    println("{}", *text.read_all_text());
    println("'{}'", *text.read_all_text());  // at the end: nothing more

    text.rewind();
    println("{}", *text.read_all_text());
}
```

Output:

```text
read twice
''
read twice
```

## See also

- [tell](tell.md): the position
- [seek_from](../../seek_from.md): where a seek counts from
- [sgcl::io::mixin::seeker\<Derived\>](README.md)
