[sgcl](../../README.md) › [encoding](../README.md) › [ascii85](../ascii85.md)

# sgcl::encoding::ascii85::encoder_to

```cpp
static encoder encoder_to(const io::writer& out) noexcept;
```

A writer that writes the Ascii85 text of what it is given to `out`: Go's `ascii85.NewEncoder`. Whole groups of
four bytes go out at once, the bytes short of a group wait for the next write, and
[close()](../ascii85-encoder/close.md) writes the last group, n + 1 characters for n bytes, and leaves `out` open.
The [encoder](../ascii85-encoder.md) is a handle of one word, made with its state: a managed object holding an
8 KB block and `out`.

## Parameters

| Parameter | Description |
|---|---|
| `out` | the writer the text goes to: any stream of io, a handle of the library, a stream of one's own |

## Return value

The encoder.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer out;
    out.write("<~");
    encoding::ascii85::encoder a85 = encoding::ascii85::encoder_to(out);
    a85.write("Hel");
    a85.write("lo");
    a85.close();
    out.write("~>");
    println(out.text());
}
```

Output:

```text
<~87cURDZ~>
```

## See also

- [ascii85::encoder](../ascii85-encoder.md): the stream
- [decoder_from](decoder_from.md): the other way
- [encode](encode.md): the text at once
- [sgcl::encoding::ascii85](../ascii85.md)
