[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::percent

```cpp
#include "sgcl/txt/percent.h"   // or "sgcl/txt.h"

namespace sgcl::txt::percent {
    inline constexpr percent_set unreserved, sub_delims, segment, path, query, fragment, userinfo;

    namespace whatwg {
        inline constexpr percent_set c0, fragment, query, special_query, path, userinfo, component,
                                     form_urlencoded;
    }

    string encode(const string& text, percent_set keep = unreserved);
    optional<string> decode(const string& text) noexcept;
}
```

`txt::percent` is the escaping of the bytes a URL may not carry as they stand, written as `%XX`, and back
([RFC 3986](https://www.rfc-editor.org/rfc/rfc3986) section 2.1): [encode](percent/encode.md) and
[decode](percent/decode.md), with the sets of characters each part of a URL leaves alone, as constants of a
[percent_set](percent_set.md).

The header stands on its own. It has no tables, it knows nothing about Unicode and nothing about domain names, and
it is a header of its own rather than a corner of [idna](idna.md) because nobody looking for percent encoding would
think to open a file called `idna`. It is not a URL parser either — taking a URL apart belongs to
[net::url](../net/url.md), and this is only the escaping the parts of a URL need once they have been taken apart.

## Rules

- **The set is an argument, not a default buried in the function.** RFC 3986 gives a different set for every part of
  a URL, and the differences are the whole point. A slash is data inside a path segment and a separator between
  segments; a question mark is a delimiter in a path and data in a query. A program that uses one set everywhere
  writes a separator where it meant data, and the bug shows up as a file name with a slash in it opening the wrong
  file. A program with a set of its own says so in one line: the sets compose with `|` and `-`.
- **Two families.** The [WHATWG URL Standard](https://url.spec.whatwg.org/) does not follow RFC 3986 here, and a
  browser follows the WHATWG. It escapes a good deal less, so that names and paths already in the wild keep working,
  and it writes its sets the other way round — as what is to be escaped, built up from "C0 controls and everything
  above `~`". Both families are here, each written the way its own specification writes it, and they are not
  reconciled. The WHATWG sets leave the sub-delims alone in most places, so `a+b` and `a,b` go through a path
  untouched, where RFC 3986's `path` lets them through too and its `unreserved` does not.
- **Which to reach for.** The WHATWG sets when a URL is handled the way a browser does — parsing one, rebuilding one,
  or handing a path to something a browser will read. The RFC 3986 sets when a specification says RFC 3986, which is
  most protocol documents outside the web.
- **Encoding cannot fail; decoding can.** Every byte has a spelling, so `encode` always answers; a byte above ASCII
  is always escaped, one escape a byte, since a URL carries bytes and nothing in it says what encoding they were.
  `decode` gives back nothing when a `%` is not followed by two hexadecimal digits: a truncated escape is a text that
  was cut, and letting it through is how something gets past a check that ran before the decoding. What comes back
  is bytes and not text.
- **`+` is not a space.** That is `application/x-www-form-urlencoded`, a form encoding and not RFC 3986: a form
  encoder writes a space as `+` where `encode` writes `%20`, and a form parser does the substitution itself, before
  or after. It is one line at the caller.
- Nothing is allocated per character: the output is laid out once and the string made once from it; a text with
  nothing to escape or unescape comes back as itself.

## Member objects

The sets of RFC 3986, built up from nothing, by what they leave alone:

| Constant | Description |
|---|---|
| `unreserved` | the letters, the digits, `-` `.` `_` `~`, and nothing else (section 2.3): what never has to be escaped, wherever it stands |
| `sub_delims` | `!` `$` `&` `'` `(` `)` `*` `+` `,` `;` `=` (section 2.2): the characters a scheme may give a meaning of its own inside a component |
| `segment` | a `pchar`: unreserved, sub-delims, `:` and `@` (section 3.3) — one segment of a path, where a slash is data and is escaped |
| `path` | `segment` and the slash |
| `query` | `path` and the question mark (section 3.4) |
| `fragment` | the same as `query` (section 3.5) |
| `userinfo` | unreserved, sub-delims and the colon (section 3.2.1) |

The sets of the WHATWG URL Standard, built down from everything, by what they escape of printable ASCII (every set
escapes the controls and everything above `~`):

| Constant | Description |
|---|---|
| `whatwg::c0` | nothing: not even the space. The complement of the C0 control percent-encode set |
| `whatwg::fragment` | `` space " < > ` `` |
| `whatwg::query` | `space " # < >`: not the fragment set less anything, the standard leaves the backquote out of it |
| `whatwg::special_query` | `space " # ' < >` |
| `whatwg::path` | `` space " # < > ? ^ ` { } `` |
| `whatwg::userinfo` | `` space " # / : ; < = > ? @ [ \ ] ^ ` { \| } `` |
| `whatwg::component` | the userinfo set and `$ % & + ,`: JavaScript's `encodeURIComponent`, character for character |
| `whatwg::form_urlencoded` | everything but the letters, the digits and `* - . _`: `application/x-www-form-urlencoded`, which the standard states as that complement and the test holds it to |

`whatwg::component` and `whatwg::form_urlencoded` are the only two of their family that escape the per cent sign,
which is to say the only two whose output reads back as what went in.

## Member functions

| Function | Description |
|---|---|
| [encode](percent/encode.md) | the text with everything outside a set written as `%XX` |
| [decode](percent/decode.md) | the bytes of `%XX` escapes, nothing for a `%` that was cut |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string name = "a b/c?d";
    println("{}", txt::percent::encode(name, txt::percent::segment));
    println("{}", txt::percent::encode(name, txt::percent::path));
    println("{}", txt::percent::encode(name, txt::percent::whatwg::path));
    println("{}", txt::percent::encode(name, txt::percent::whatwg::component));
    println("{}", txt::percent::decode("a%20b%2Fc").value());
}
```

Output:

```text
a%20b%2Fc%3Fd
a%20b/c%3Fd
a%20b/c%3Fd
a%20b%2Fc%3Fd
a b/c
```

## See also

- [percent_set](percent_set.md): the sets
- [idna](idna.md): the name of a host, the other half of the text of a URL
- [decode](decode.md): the bytes a decoding gives back, as text of an encoding
- [net::url](../net/url.md): a URL taken apart, which escapes with these sets
- `tests/txt/percent.cpp`: the sets of both families, held to their specifications
- [sgcl::txt](README.md)
