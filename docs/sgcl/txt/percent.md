# txt::percent

```cpp
#include "sgcl/txt/percent.h"
```

The bytes a URL may not carry as they stand, written as `%XX`, and back ([RFC 3986](https://www.rfc-editor.org/rfc/rfc3986) section 2.1).

This header stands on its own. It has no tables, it knows nothing about Unicode and nothing about domain names, and it is a header of its own rather than a corner of [`idna`](idna.md) because nobody looking for percent encoding would think to open a file called `idna`. It is not a URL parser either — taking a URL apart belongs to `net`, and this is only the escaping that the parts of a URL need once they have been taken apart.

## Names

```cpp
class percent_set {
    constexpr percent_set() noexcept;
    constexpr explicit percent_set(const char* chars) noexcept;   // the characters themselves
    constexpr bool holds(char c) const noexcept;
    constexpr percent_set operator|(const percent_set&) const noexcept;
};

namespace percent {
    string encode(const string& text, percent_set keep = unreserved);
    optional<string> decode(const string& text);

    // RFC 3986
    percent_set unreserved, sub_delims, segment, path, query, fragment, userinfo;

    // the WHATWG URL Standard, which escapes less
    namespace whatwg {
        percent_set c0, fragment, query, special_query, path, userinfo,
                    component, form_urlencoded;
    }
}
```

```cpp
using namespace sgcl;

txt::percent::encode(string("/a b/c"), txt::percent::path);   // "/a%20b/c"
txt::percent::encode(string("/a b/c"));                       // "%2Fa%20b%2Fc"
txt::percent::decode(string("%C3%BC"));                       // the two bytes of "ü"
txt::percent::decode(string("a%"));                           // nothing
```

## The set is an argument, not a default buried in the function

RFC 3986 gives a different set for every part of a URL, and the differences are the whole point. A slash is data inside a path segment and a separator between segments; a question mark is a delimiter in a path and data in a query. A program that uses one set everywhere writes a separator where it meant data, and the bug shows up as a file name with a slash in it opening the wrong file.

| | what it leaves alone |
|---|---|
| `unreserved` | the letters, the digits, `-` `.` `_` `~`, and nothing else (section 2.3) |
| `sub_delims` | `!` `$` `&` `'` `(` `)` `*` `+` `,` `;` `=` (section 2.2) |
| `segment` | a `pchar`: unreserved, sub-delims, `:` and `@` — one segment of a path, where a slash is data |
| `path` | `segment` and the slash |
| `query`, `fragment` | `path` and the question mark (sections 3.4, 3.5) |
| `userinfo` | unreserved, sub-delims and the colon (section 3.2.1) |

`percent_set` is a 128 bit mask over ASCII, built from the characters themselves the way the RFC writes them — `percent_set{"!$&'()*+,;="}` is its sub-delims — and the sets compose with `|`, so a program with a set of its own says so in one line:

```cpp
inline constexpr auto mine = txt::percent::unreserved | txt::percent_set{"/:"};
```

## Two families, and which one to reach for

The [WHATWG URL Standard](https://url.spec.whatwg.org/) does not follow RFC 3986 here, and a browser follows the WHATWG. It escapes a good deal less, so that names and paths already in the wild keep working, and it writes its sets the other way round — as what **is** to be escaped, built up from "C0 controls and everything above `~`". Both families are here, each written the way its own specification writes it, and they are not reconciled.

| | what it escapes, of printable ASCII |
|---|---|
| `whatwg::c0` | nothing — not even the space |
| `whatwg::fragment` | `` space " < > ` `` |
| `whatwg::query` | `space " # < >` |
| `whatwg::special_query` | `space " # ' < >` |
| `whatwg::path` | `` space " # < > ? ^ ` { } `` |
| `whatwg::userinfo` | `` space " # / : ; < = > ? @ [ \ ] ^ ` { \| } `` |
| `whatwg::component` | the userinfo set and `$ % & + ,` |
| `whatwg::form_urlencoded` | everything but the letters, the digits and `* - . _` |

**Reach for the WHATWG sets** when you are handling a URL the way a browser does — parsing one, rebuilding one, or handing a path to something that will be read by a browser. **Reach for the RFC 3986 sets** when a specification says RFC 3986, which is most protocol documents outside the web.

Two of these are worth knowing by name. `whatwg::component` is JavaScript's `encodeURIComponent`, character for character; it and `form_urlencoded` are the only sets in either family that escape the per cent sign, which is to say the only two whose output reads back as what went in. And `form_urlencoded` is stated by the standard as a complement — everything except the letters, the digits and `* - . _` — which is what the test holds it to.

One thing `form_urlencoded` does not do: a form encoder writes a space as `+`, and `encode` writes `%20`. That substitution belongs to the form encoding and not to the escaping, and it is one line at the caller.

## Encoding cannot fail; decoding can

Every byte has a spelling, so `encode` always answers. The digits go out upper case, which section 6.2.2.1 says a producer should use, and a byte above ASCII is always escaped, one escape a byte: a URL carries bytes, and nothing in a URL says what encoding they were.

`decode` gives back nothing when a `%` is not followed by two hexadecimal digits. That is the only way it fails and it is worth failing on: a truncated escape is not a text with a stray per cent sign in it, it is a text that was cut, and letting it through is how something gets past a check that ran before the decoding. Either case of the digits is read, as section 6.2.2.1 says a consumer must.

What comes back is **bytes and not text**. An escape can spell a byte no UTF-8 has. A caller who knows they are UTF-8 has them; one who does not turns them into text with [`txt::decode`](encoding.md).

`+` is not a space here. That is `application/x-www-form-urlencoded`, which is a form encoding and not RFC 3986; a form parser does that substitution itself, before or after.

## What it costs

121 ns to escape a path of 23 bytes and 81 to read it back, over a stream of different values. Nothing is allocated per character: the output is laid out into one buffer and the string made once from it.
