[sgcl](../../README.md) › [encoding](../README.md) › [base64](../base64.md)

# sgcl::encoding::base64::encode_to

```cpp
size_t encode_to(const slice<char>& out, const slice<const byte>& data) const;
```

The text of `data` written into the caller's buffer, nothing allocated: Go's `Encode`. `out` holds at least
[encoded_size](encoded_size.md)`(data.size())` characters; a smaller one is a mistake in the program, refused
before anything is written. The characters past the text are left as they were. `out` does not
overlap `data`.

## Parameters

| Parameter | Description |
|---|---|
| `out` | the buffer the text is written into |
| `data` | the bytes to encode |

## Return value

The number of characters written, `encoded_size(data.size())`.

## Complexity

Linear in the size of `data`.

## Exceptions

`length_error` when `out` is smaller than `encoded_size(data.size())`.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    array<byte, 4> data = {byte('a'), byte('b'), byte('c'), byte('d')};
    array<char, 8> text = {};
    size_t n = encoding::base64::standard.encode_to(text, data);
    println("{} {}", n, string(text.data(), n));

    array<char, 4> small = {};
    try {
        encoding::base64::standard.encode_to(small, data);
    } catch (const length_error& e) {
        println(e.what());
    }
}
```

Output:

```text
8 YWJjZA==
sgcl: the buffer is smaller than the encoding
```

## See also

- [encoded_size](encoded_size.md): the size the buffer needs
- [encode](encode.md): into a string of its own
- [decode_to](decode_to.md): the other way
- [sgcl::encoding::base64](../base64.md)
