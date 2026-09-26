# txt::encoding

```cpp
#include "sgcl/txt/encoding.h"
```

Getting text in and out of the shapes other people keep it in: the two other encodings of Unicode itself — UTF-16 at the edge of a Windows system call, UTF-32 where a code point is an integer — and the **27 single byte encodings the Encoding Standard of the WHATWG lists** — what a browser must understand, which is what a file or a web page may still arrive in. The library keeps text in UTF-8 and nothing here changes that: everything decodes to a [`string`](../core/string.md) and everything encodes from one.

This is the one header of the module whose clearest use is the case where there is **no Unicode yet**: bytes in `iso-8859-2` announced by an HTTP header, a file written by a program from the nineties.

## The names

```cpp
enum class encoding : uint8_t {
    utf8, utf16le, utf16be, utf32le, utf32be,
    ascii, latin1, latin2, windows1250, windows1252,
};

constexpr const char* name_of(encoding) noexcept;                  // the name a header would use
optional<encoding> encoding_from_name(const string& name) noexcept;

struct byte_order_mark { optional<encoding> says; size_t size; explicit operator bool() const; };
byte_order_mark detect_bom(slice<const byte>) noexcept;

string decode(const slice<const byte>& bytes, encoding from);                              // a byte that means nothing: U+FFFD
expected<string, decode_error> decode(const slice<const byte>& bytes, encoding from, strict_t);   // decode(b, e, txt::strict): or the first such byte
class decode_error { size_t offset() const; encoding from() const; string message() const; };   // "not utf-8"
vector<byte> encode(const string& text, encoding to);

vector<char16_t> to_utf16(const string&);    string from_utf16(slice<const char16_t>);
vector<char32_t> to_utf32(const string&);    string from_utf32(slice<const char32_t>);
```

An **encoding is a value, not a tag**, and this is the one place in the module where that is so. Elsewhere — the four normalization forms — the choice is made when the program is written, so a tag costs nothing and saves a table; here the name comes out of a header while the program runs, and nothing can be chosen beforehand.

`encoding_from_name` reads a name as a header writes it: any case, with the dashes or without, under the aliases IANA lists and the ones that turn up in the wild (`utf8`, `ISO_8859-2`, `cp1250`, `iso8859_2`). A name nobody knows is `nullopt` — an ordinary answer, not a failure, so no `expected`.

`detect_bom` says what a byte order mark at the front announces and how many bytes it takes. Nothing else in the header looks at one: a caller who wants it honoured skips those bytes itself, which keeps that decision where it belongs.

## What is not here

The multi byte legacy encodings of the same list — **Shift_JIS, EUC-JP, GB18030, Big5 and EUC-KR** — are not implemented, and that is a decision rather than an oversight. Their tables come to some three or four hundred kilobytes against the six hundred the whole module holds today; a linker drops what a program never asks about, so nobody's binary would carry them, but the repository and every build of it would. They are worth the room the day something here has to read the older files of that part of the world. Until then, a text in one of them is turned into UTF-8 before it reaches this library — `iconv` and every platform's own converter do it.

`encoding_from_name` answers `nullopt` for their names, as it does for any name it does not know, so a program that meets one in a header can say so rather than guess.

## Nothing is refused

A byte that means nothing in its encoding decodes to one `U+FFFD`, exactly as an invalid byte of UTF-8 does. A character an encoding cannot write is encoded as `'?'` — what every library that does this has always done, and what a reader can at least see. Neither throws, and neither stops the rest of the text from coming through.

```cpp
decode(one_byte(0x81), encoding::windows1250);   // U+FFFD: that byte is nothing there
decode(one_byte(0x81), encoding::latin1);        // U+0081: in Latin-1 every byte is a code point
decode(one_byte(0x81), encoding::windows1250, strict);   // decode_error at byte 0, for a program that must not store a changed text
encode(string("日"), encoding::iso8859_2);          // "?"
encode(string("Ł"), encoding::iso8859_2);           // one byte, 0xA3
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace sgcl;

// A page arrives with a header saying what it is in, and a byte order
// mark that may say something else
int main() {
    string text = "Zażółć gęślą jaźń — 日";
    for (auto name : {"utf-8", "utf-16le", "iso-8859-2", "windows-1250", "us-ascii"}) {
        auto e = txt::encoding_from_name(string(name));
        auto bytes = txt::encode(text, *e);
        std::cout << txt::name_of(*e) << ": " << bytes.size() << " bajtów -> "
                  << txt::decode(bytes.as_slice(), *e) << '\n';
    }

    byte page[] = {byte(0xEF), byte(0xBB), byte(0xBF),
                        byte('c'), byte('z'), byte(0xC5), byte(0x82)};
    auto bom = txt::detect_bom(slice<const byte>(page));
    std::cout << "znacznik mówi " << (bom ? txt::name_of(*bom.says) : "nic")
              << ", treść: " << txt::decode(slice<const byte>(page).subslice(bom.size), txt::encoding::utf8) << '\n';
    return 0;
}
```

The output:

```
utf-8: 34 bajtów -> Zażółć gęślą jaźń — 日
utf-16le: 42 bajtów -> Zażółć gęślą jaźń — 日
iso-8859-2: 21 bajtów -> Zażółć gęślą jaźń ? ?
windows-1250: 21 bajtów -> Zażółć gęślą jaźń — ?
us-ascii: 21 bajtów -> Za???? g??l? ja?? ? ?
znacznik mówi utf-8, treść: czł
```

## What it is held to

Python's own codecs, which reach the same answer by another road: the tables here are read from the `MAPPINGS` files of unicode.org, and Python's are built into the interpreter. Every one of the **256 bytes of every single byte encoding**, and **90 texts** encoded in each of the ten encodings and decoded back — the question mark for what an encoding cannot write included, since Python's `errors="replace"` writes the same one.

The tables are 32.6 KB, about 1.2 KB an encoding: 256 code points one way and the way back sorted by code point, which is eight steps of a binary search over the couple of hundred entries that are not ASCII. ISO-8859-1 has no table — its byte is its code point — and neither has ASCII.

The tables come from the `MAPPINGS` files of unicode.org rather than from the Encoding Standard's own indexes. The set is the standard's, because that is the list of what still turns up; the tables are not, because the standard's indexes are a browser's compatibility rules and this is a library. The two differ in a handful of places, all of them positions no correct text uses. The enum, the preferred names, the aliases and the tables are written by one list in the generator, so none of them can drift from the others.

Apple's two files (Mac OS Roman and Mac OS Cyrillic) begin at 0x20 and say in prose that the bytes below it are the ASCII controls; the generator fills those in and asserts that every byte below 0x80 of every encoding of the set maps to itself.
