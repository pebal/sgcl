[sgcl](../../README.md) › [codec](../README.md)

# sgcl::codec::error

```cpp
#include "sgcl/codec/error.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    class error;
}
```

`sgcl::codec::error` is why an image file is not an image, or an image not a file: the [code](code.md) from
the one list of every format ([errc](../errc.md)), the [byte](offset.md) of the input where the failure was
found, the [error of the stream](io_error.md) when the input came from one and that failed, and a
[message](message.md) in the format's own words. It is the one type of error of the whole module.

The decoders, the encoders, [load](../load.md) and [save](../save.md) return an
[expected](../../core/expected/README.md) of their result and a `codec::error`, as Go's `image.Decode` returns an `error`:
a file that is not what it should be is a value to look at, not an exception. A program that takes the result as it
comes, `codec::image photo = codec::png::decode(file);`, gets a
[bad_expected_access](../../core/bad_expected_access/README.md)`<codec::error>` thrown for a file that fails, which carries
the error, its `what()` the error's message.

## Rules

- An error is a value: it is copied (never with a throw), compared and held in an `expected`. It holds a
  [string](../../core/string/README.md) and an [io::error](../../io/error/README.md), so it lives where a string may.
- The codes start at 1, so that a `std::error_code` of 0 is success; an `errc` converts to one of
  [codec_category](../codec_category.md).
- The offset is in bytes from the start of the input, at the header or the field that failed; 0 where the failure
  has no byte of its own: a file that does not open, an extension `save` does not write, a `want` outside the list.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](error.md) | constructs an error of a code, an offset and the words, or of a stream's error |
| [code](code.md) | the kind of failure |
| [offset](offset.md) | the byte of the input where it was found |
| [io_error](io_error.md) | the stream's error, for `errc::io` |
| [message](message.md) | the offset and the words, as one sentence |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](operator_cmp.md) | the same code at the same byte, in the same words |

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image picture(4, 4, codec::pixel_format::rgb8);
    vector<byte> file = codec::png::encode(picture);
    auto cut = codec::png::decode(file.as_slice(0, 40));
    if (!cut) {
        const codec::error& e = cut.error();
        println(e.message());
        println("{} {}", e.offset(), e.code() == codec::errc::unexpected_end);
    }
}
```

Output:

```text
offset 40: png: the data ends in the middle
40 true
```

## See also

- [errc](../errc.md): the codes
- [decode](../decode.md), [load](../load.md), [save](../save.md): what returns it
- [expected](../../core/expected/README.md): the value or the error
- [io::error](../../io/error/README.md): the error of a stream
- [sgcl::codec](../README.md)
