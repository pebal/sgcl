[sgcl](../../README.md) › [compress](../README.md) › [flate](../flate/README.md) › [reader](README.md)

# sgcl::compress::flate::reader::last_error

```cpp
const optional<error>& last_error() const noexcept;
```

Returns the error of the data, kept: the [compress::error](../error/README.md) whose code, offset and detail a read gave as
an `io::error` of the compress category. The read that reached it and every read after give it; a
[reset](reset.md) clears it. A failure of `in` itself is not kept here: the read returns `in`'s error as it came.

## Parameters

None.

## Return value

The error of the data, or `nullopt` while there was none.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto packed = compress::flate::compress("hello, hello, hello");
    packed[packed.size() / 2] ^= byte(0x55);  // damaged on the way

    compress::flate::reader r{io::buffer(packed)};
    auto text = r.read_all_text();
    if (!text) {
        println("{}", text.error().message());
        println("{}", r.last_error()->message());
    }
}
```

Output:

```text
read flate: corrupt data
offset 7: distance before the start of the output
```

## See also

- [compress::error](../error/README.md)
- [sgcl::compress::flate::reader](README.md)
