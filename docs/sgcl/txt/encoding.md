[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::encoding

```cpp
#include "sgcl/txt/encoding.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    enum class encoding : uint8_t {
        utf8, utf16le, utf16be, utf32le, utf32be, ascii, latin1,
        ibm866, iso8859_2, iso8859_3, iso8859_4, iso8859_5, iso8859_6, iso8859_7, iso8859_8,
        iso8859_10, iso8859_13, iso8859_14, iso8859_15, iso8859_16, koi8_r, koi8_u, macintosh,
        windows874, windows1250, windows1251, windows1252, windows1253, windows1254, windows1255,
        windows1256, windows1257, windows1258, x_mac_cyrillic
    };
}
```

An encoding text may arrive in or have to leave in: the two other encodings of Unicode itself — UTF-16 at the edge of
a Windows system call, UTF-32 where a code point is an integer — ASCII and ISO-8859-1, and the 27 single byte
encodings the Encoding Standard of the WHATWG lists, what a browser must understand, which is what a file or a web
page may still arrive in: bytes in `iso-8859-2` announced by an HTTP header, a file written by a program from the
nineties. The library keeps text in UTF-8 and nothing here changes that: everything [decode](decode.md)s to a
[string](../core/string/README.md) and everything [encode](encode.md)s from one.

An encoding is a value, not a tag, and this is the one place in the module where that is so. Elsewhere — the four
normalization forms — the choice is made when the program is written, so a tag costs nothing and saves a table;
here the name comes out of a header while the program runs (`charset=iso-8859-2`,
[encoding_from_name](encoding_from_name.md)), and nothing can be chosen beforehand.

## Rules

- **Nothing is refused.** A byte that means nothing in its encoding decodes to one `U+FFFD`, exactly as an invalid
  byte of UTF-8 does; a character an encoding cannot write is encoded as `'?'`, what every library that does this has
  always done and what a reader can at least see. Neither throws, and neither stops the rest of the text from coming
  through. A program that must not store a changed text decodes with [strict](strict_t.md), which gives the first
  such byte instead ([decode_error](decode_error/README.md)).
- **What is not here.** The multi byte legacy encodings of the same list — Shift_JIS, EUC-JP, GB18030, Big5 and
  EUC-KR — are not implemented, and that is a decision rather than an oversight. Their tables come to some three or
  four hundred kilobytes against the six hundred the whole module holds today; a linker drops what a program never
  asks about, so nobody's binary would carry them, but the repository and every build of it would. They are worth
  the room the day something here has to read the older files of that part of the world. Until then, a text in one
  of them is turned into UTF-8 before it reaches this library — `iconv` and every platform's own converter do it.
  `encoding_from_name` answers `nullopt` for their names, as for any name it does not know, so a program that meets
  one in a header can say so rather than guess.
- **The tables.** They come from the `MAPPINGS` files of unicode.org rather than from the Encoding Standard's own
  indexes. The set is the standard's, because that is the list of what still turns up; the tables are not, because
  the standard's indexes are a browser's compatibility rules and this is a library. The two differ in a handful of
  places, all of them positions no correct text uses. The enumeration, the preferred names, the aliases and the
  tables are written by one list in the generator, so none of them can drift from the others. Apple's two files (Mac
  OS Roman and Mac OS Cyrillic) begin at 0x20 and say in prose that the bytes below it are the ASCII controls; the
  generator fills those in and asserts that every byte below 0x80 of every encoding of the set maps to itself.
- **The size.** The tables are 32.6 KB, about 1.2 KB an encoding: 256 code points one way and the way back sorted by
  code point, which is eight steps of a binary search over the couple of hundred entries that are not ASCII.
  ISO-8859-1 has no table — its byte is its code point — and neither has ASCII.
- **What it is held to.** Python's own codecs, which reach the same answer by another road: the tables here are read
  from the `MAPPINGS` files, and Python's are built into the interpreter. Every one of the 256 bytes of every single
  byte encoding, and 90 texts encoded in each of the ten encodings and decoded back — the question mark for what an
  encoding cannot write included, since Python's `errors="replace"` writes the same one.

| Value | Description |
|---|---|
| `utf8` | UTF-8, `utf-8`: what the library keeps text in |
| `utf16le` | UTF-16, the low byte first, `utf-16le` |
| `utf16be` | UTF-16, the high byte first, `utf-16be` |
| `utf32le` | UTF-32, the low byte first, `utf-32le` |
| `utf32be` | UTF-32, the high byte first, `utf-32be` |
| `ascii` | `us-ascii`: a byte above 127 is no character |
| `latin1` | ISO-8859-1, `iso-8859-1`: every byte its own code point, no table |
| `ibm866` | the Cyrillic code page of DOS, `ibm866` |
| `iso8859_2` | Central European, `iso-8859-2` |
| `iso8859_3` | South European, `iso-8859-3` |
| `iso8859_4` | North European, `iso-8859-4` |
| `iso8859_5` | Cyrillic, `iso-8859-5` |
| `iso8859_6` | Arabic, `iso-8859-6` |
| `iso8859_7` | Greek, `iso-8859-7` |
| `iso8859_8` | Hebrew, `iso-8859-8` |
| `iso8859_10` | Nordic, `iso-8859-10` |
| `iso8859_13` | Baltic, `iso-8859-13` |
| `iso8859_14` | Celtic, `iso-8859-14` |
| `iso8859_15` | Western European with the euro sign, `iso-8859-15` |
| `iso8859_16` | South-Eastern European, `iso-8859-16` |
| `koi8_r` | Russian, `koi8-r` |
| `koi8_u` | Ukrainian, `koi8-u` |
| `macintosh` | Mac OS Roman, `macintosh` |
| `windows874` | Thai, `windows-874` |
| `windows1250` | Central European, `windows-1250` |
| `windows1251` | Cyrillic, `windows-1251` |
| `windows1252` | Western European, `windows-1252` |
| `windows1253` | Greek, `windows-1253` |
| `windows1254` | Turkish, `windows-1254` |
| `windows1255` | Hebrew, `windows-1255` |
| `windows1256` | Arabic, `windows-1256` |
| `windows1257` | Baltic, `windows-1257` |
| `windows1258` | Vietnamese, `windows-1258` |
| `x_mac_cyrillic` | Mac OS Cyrillic, `x-mac-cyrillic` |

## Example

A page arrives with a header saying what it is in:

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string text = "Zażółć gęślą jaźń — 日";
    for (auto name : {"utf-8", "utf-16le", "iso-8859-2", "windows-1250", "us-ascii"}) {
        auto e = txt::encoding_from_name(name);
        auto bytes = txt::encode(text, *e);
        println("{}: {} bytes -> {}", txt::name_of(*e), bytes.size(), txt::decode(bytes, *e));
    }
}
```

Output:

```text
utf-8: 34 bytes -> Zażółć gęślą jaźń — 日
utf-16le: 42 bytes -> Zażółć gęślą jaźń — 日
iso-8859-2: 21 bytes -> Zażółć gęślą jaźń ? ?
windows-1250: 21 bytes -> Zażółć gęślą jaźń — ?
us-ascii: 21 bytes -> Za???? g??l? ja?? ? ?
```

## See also

- [decode](decode.md), [encode](encode.md): the bytes of an encoding and the text
- [encoding_from_name](encoding_from_name.md), [name_of](name_of.md): the names a header uses
- [detect_bom](detect_bom.md): what a byte order mark says
- [to_utf16](to_utf16.md), [to_utf32](to_utf32.md): UTF-16 and UTF-32 as units
- [sgcl::txt](README.md)
