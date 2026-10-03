[sgcl](../../README.md) › [encoding](../README.md) › [base64](README.md)

# sgcl::encoding::base64::base64

```cpp
constexpr base64(const char (&alphabet)[65], optional<char> padding = '=');
```

A codec of an alphabet of one's own: Go's `base64.NewEncoding(alphabet)`, and with `nullopt` for the padding its
`WithPadding(base64.NoPadding)`. The alphabet is 64 different characters, none of them `'\0'`, `'\r'`, `'\n'` or
the padding, in an array of 65 whose last is the terminator: a literal of 64 characters. The length is the
array's, never a search for its end, and the characters past it are never read. The codec made is strict, as the
[constants](README.md#member-objects) are; [lenient()](lenient.md) makes the lenient one.

Anything else is a mistake in the program, not in its input: `invalid_argument`, which in a constant — a
`constexpr` codec — is an error at compile time.

## Parameters

| Parameter | Description |
|---|---|
| `alphabet` | the 64 characters of the values 0 to 63, in their order |
| `padding` | the character that pads the last group to four, or `nullopt` for none |

## Complexity

Constant: a table of 256 filled from the 64 characters.

## Exceptions

`invalid_argument` when the alphabet has a repeated character, a `'\0'` before its end, a line ending or the
padding, or when the padding is `'\r'` or `'\n'`.

## Example

`crypt(3)`'s alphabet, which puts `./` first and has no padding:

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    static constexpr encoding::base64 crypt(
        "./ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", nullopt);
    println(crypt.encode("hello, world"));
    println(crypt.padded());

    try {
        encoding::base64 twice("AACDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/");
    } catch (const invalid_argument& e) {
        println(e.what());
    }
}
```

Output:

```text
YETqZE6qGFbtakvi
false
sgcl: an alphabet with a repeated, padding or line-ending character
```

## See also

- [without_padding](without_padding.md), [lenient](lenient.md): the same codec with another choice
- [sgcl::encoding::base64](README.md)
