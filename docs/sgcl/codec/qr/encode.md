[sgcl](../../README.md) › [codec](../README.md) › [qr](README.md)

# sgcl::codec::qr::encode

```cpp
static expected<qr, error> encode(const string& text, const options& o = {});               // (1)
static expected<qr, error> encode(const char* text, const options& o = {});                 // (2)
static expected<qr, error> encode(const slice<const byte>& data, const options& o = {});    // (3)
```

Makes the QR code of a text or of bytes.

1. A text, UTF-8: cut into segments of the four modes for the fewest bits, by a dynamic programme over its
   characters (numeric 10 bits a 3 digits, alphanumeric 11 a 2 characters, byte 8 a byte, Kanji 13 a character,
   each segment's mode and count in front), once for each of the three groups of versions whose counts differ in
   length (1 to 9, 10 to 26, 27 to 40). An ECI of UTF-8 goes in front when a byte segment holds what is not ASCII
   and `o.eci` is on. A text that is not UTF-8 is taken as bytes.
2. A literal, as (1): so that `encode("...")` does not also match (3).
3. Bytes as they are, in one byte segment, no ECI.

- (1–3) The version is the smallest of `o.min_version` to `o.max_version` whose capacity at `o.level` holds the
  bits; with `o.boost_level` the level is then raised as far as that version still holds them. The bit stream ends
  with its terminator and the pad codewords 11101100 and 00010001, is cut into blocks with their Reed-Solomon
  codewords and interleaved, placed in the symbol around its function patterns, and masked by `o.mask`, or by the
  one of the eight patterns of least penalty under the four rules of the standard (runs, blocks, finder-like
  patterns, the balance of dark and light).

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8 |
| `data` | the bytes |
| `o` | the level, the versions, the mask, Kanji mode and ECI; the default is level M, versions 1 to 40, the mask chosen, the level boosted, Kanji mode and ECI on |

## Return value

The QR code, or the [error](../error/README.md) `errc::too_large` when a symbol of `o.max_version` does not hold the
data at `o.level`.

## Complexity

Linear in the length of the data and in the modules of the symbol, eight times over for the masks tried.

## Exceptions

`invalid_argument` for options outside their ranges: a level outside the list, `o.min_version` below 1, `o.max_version`
past 40 or below `o.min_version`, `o.mask` outside −1 to 7.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::qr url = codec::qr::encode("https://example.com");
    println("version {}, level {}", url.version(), int(url.correction()));
    codec::qr digits = codec::qr::encode("0123456789012345678901234567890123456789");
    println("40 digits: version {}", digits.version());
    codec::qr japanese = codec::qr::encode("日本語", {.level = codec::qr::level::high});
    println("Kanji mode: version {}", japanese.version());
    vector<byte> bytes(3000, byte(0x55));
    println("{}", codec::qr::encode(bytes).error().message());
}
```

Output:

```text
version 2, level 2
40 digits: version 2
Kanji mode: version 1
offset 0: qr: more data than a symbol of max_version holds at its level
```

## See also

- [options](../qr-options.md): what encode is told
- [to_image](to_image.md), [to_svg](to_svg.md): the symbol drawn
- [sgcl::codec::qr](README.md)
