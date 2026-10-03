[sgcl](../../README.md) › [encoding](../README.md) › [base32](../base32.md)

# sgcl::encoding::base32::base32

```cpp
constexpr base32(const char (&alphabet)[33], optional<char> padding = '=');
```

A codec of an alphabet of one's own: Go's `base32.NewEncoding(alphabet)`, and with `nullopt` for the padding its
`WithPadding(base32.NoPadding)`. The alphabet is 32 different characters, none of them `'\0'`, `'\r'`, `'\n'` or
the padding, in an array of 33 whose last is the terminator: a literal of 32 characters. The length is the
array's, never a search for its end. The codec made is strict, as the [constants](../base32.md#member-objects)
are; [lenient()](lenient.md) makes the lenient one. A codec that reads lower-case letters is one of these, with
the lower-case alphabet.

Anything else is a mistake in the program, not in its input: `invalid_argument`, which in a constant — a
`constexpr` codec — is an error at compile time.

## Parameters

| Parameter | Description |
|---|---|
| `alphabet` | the 32 characters of the values 0 to 31, in their order |
| `padding` | the character that pads the last group to eight, or `nullopt` for none |

## Complexity

Constant: a table of 256 filled from the 32 characters.

## Exceptions

`invalid_argument` when the alphabet has a repeated character, a `'\0'` before its end, a line ending or the
padding, or when the padding is `'\r'` or `'\n'`.

## Example

The lower-case alphabet without padding, as the names of some systems write base32:

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    static constexpr encoding::base32 lower("abcdefghijklmnopqrstuvwxyz234567", nullopt);
    println(lower.encode("foobar"));
    println(string(lower.decode("mzxw6ytboi").value()));
    println(encoding::base32::standard.decode("mzxw6ytboi").error().message());
}
```

Output:

```text
mzxw6ytboi
foobar
offset 0: invalid character 'm'
```

## See also

- [without_padding](without_padding.md), [lenient](lenient.md): the same codec with another choice
- [sgcl::encoding::base32](../base32.md)
