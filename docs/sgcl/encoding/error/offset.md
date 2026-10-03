[sgcl](../../README.md) › [encoding](../README.md) › [error](README.md)

# sgcl::encoding::error::offset

```cpp
uint64_t offset() const noexcept;
```

The byte of the input where the input stops being the start of something valid, from the input's start: the
character outside the alphabet, the padding where the data cannot end, the first character after the padding, the
end of the input when it is cut. `QQ=x` fails at the `x`, 3, and `QUJ` at its end, 3. A strict base64 or base32
refuses bits past the data at the character that carries them. For a stream, the offset counts every byte the
stream gave. An error that did not come from an input text (of `stringify`, `from` or `as` of [json](../json/README.md)
and [xml](../xml/README.md), a mistake of the calls to an [xml::writer](../xml-writer/README.md), a file of `load` or `save` that
does not open, read or write) has an offset of 0, which its [message](message.md) does not show.

## Parameters

None.

## Return value

The offset, in bytes.

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
    for (const char* text : {"QQ=x", "QUJ", "QUJ*", "QUI=", "QUJ="}) {
        auto r = encoding::base64::standard.decode(text);
        if (r) {
            println("{}: {} bytes", text, r->size());
        } else {
            println("{}: {} at {}", text, r.error().message(), r.error().offset());
        }
    }
}
```

Output:

```text
QQ=x: offset 3: expected padding, found 'x' at 3
QUJ: offset 3: the last group is not padded at 3
QUJ*: offset 3: invalid character '*' at 3
QUI=: 2 bytes
QUJ=: offset 2: bits past the data in the last character at 2
```

## See also

- [line](line.md), [column](column.md): the place a person reads
- [sgcl::encoding::error](README.md)
