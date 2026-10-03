[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::idna

```cpp
#include "sgcl/txt/idna.h"   // or "sgcl/txt.h"

namespace sgcl::txt::idna {
    enum class error : uint8_t;
    struct options;
    struct failure;
    struct outcome;

    inline constexpr const char* version = unicode::version;
}
```

`txt::idna` is the name of a host, written in somebody's own script and written the way the DNS carries it: a whole
domain name between Unicode and ASCII, both ways, by [UTS #46](https://www.unicode.org/reports/tr46/) and
[RFC 5890](https://www.rfc-editor.org/rfc/rfc5890)–5894. One label of it at a time is [punycode](punycode.md)
([RFC 3492](https://www.rfc-editor.org/rfc/rfc3492)), which this is built on. [to_ascii](idna/to_ascii.md) and
[to_unicode](idna/to_unicode.md) are the two a program calls.

This is text work and not network work, which is why it lives in `txt` and not in `net`. What `bücher.de` is called
in the DNS follows from case, from normalization and from the bidirectional classes, and all three are in this
module already. A socket never sees any of it: it is given the ASCII the name turns into, and hands back the ASCII a
lookup answered. The other half of the text of a URL, the bytes of a path escaped as `%XX`, is
[percent](percent.md), which shares nothing with this one: a different specification and no tables.

## Rules

- **Here an operation can really fail**, unlike anywhere else in `txt`, where a text that is wrong is repaired (an
  invalid byte is `U+FFFD`, a code point no encoding can carry is `'?'`). A name may hold a character no domain name
  may hold, a label may be longer than the 63 bytes the DNS carries, a label that says `xn--` may be nonsense. A name
  that is wrong must not be looked up, so the answer is an [expected](../core/expected.md) and a caller cannot walk
  past it. [ascii_form](idna/ascii_form.md) and [unicode_form](idna/unicode_form.md) answer the same question and
  hand back the text as well as what was wrong with it: UTS #46 converts as far as it can even when it fails, and that
  text is worth having, since a browser shows the user the name it would not look up, with the bad label marked.
- **A failure names the label, not the name** ([failure](idna-failure.md)): a browser underlines the label, it does
  not grey out the address bar.
- **Transitional and nontransitional**, which is what people trip over. Four characters mean one thing to IDNA2003
  and another to IDNA2008: `ß`, the Greek final sigma `ς`, and the two zero width joiners. UTS #46 calls them
  **deviations** and gives two answers, `faß.de` being `xn--fa-hia.de` by nontransitional processing, the default,
  and `fass.de` by transitional processing, which writes the final sigma of `σόλος.gr` as an ordinary one.
  Transitional processing was meant to carry the older standard over during a changeover that is long finished. It
  silently sends two spellings of one name to two different hosts, every browser now uses nontransitional
  processing, and UTS #46 deprecates it; [options](idna-options.md)`::transitional` exists so that a program that
  has to reproduce an old answer can, and nothing new should ask for it. Punycode is never remapped, whichever was
  asked for: `xn--fa-hia.de` reads back as `faß.de` under both. Something already encoded was encoded by somebody
  who had decided.
- **The checks.** Each label of a name is held to the validity criteria of UTS #46 section 4.1: it must be in
  normalization form C, must not have `--` in its third and fourth places or a hyphen at either end
  (`check_hyphens`), must not begin with a combining mark, must hold only code points the mapping table calls valid,
  must satisfy the **ContextJ** rules of [RFC 5892](https://www.rfc-editor.org/rfc/rfc5892) for the zero width
  joiners (`check_joiners`), and, in a name that has anything right to left in it anywhere, must satisfy all six
  conditions of [RFC 5893](https://www.rfc-editor.org/rfc/rfc5893) (`check_bidi`). The bidirectional classes are
  [bidi](bidi.md)'s rather than a table of this namespace's own. The two profiles and the flags are
  [options](idna-options.md)'.
- **The limit: CONTEXTO is not checked.** `check_joiners` is ContextJ, appendices A.1 and A.2 of RFC 5892, the two
  zero width joiners, and that is all of the contextual rules applied here. **CONTEXTO**, appendices A.3 to A.9, is
  not implemented: the middle dot between two l's in Catalan, the Greek lower numeral sign, the Hebrew geresh and
  gershayim, the katakana middle dot, and the rule that a label may not mix the two families of Arabic-Indic digits.
  That is a decision, and here is where it stops being the right one. Those rules exist to stop a name from being
  **registered**, and this library is at the other end of the wire: a client looks a name up, and the registry it is
  looking it up in is what applied them. `IdnaTestV2.txt` says the same in its own header (it calls the CONTEXTO
  tests optional for client software and carries none of them), and a client that enforced them would refuse names
  that are registered and resolve today. **The day this is used to decide whether a name may be taken rather than
  whether one may be resolved, they are needed.** Writing them wants the [script](script.md) property, which this
  module already has, and a small table for the digits.
- **The oracle** is `IdnaTestV2.txt` of the UCD, in full: 6387 of its 6389 cases (the two left out carry an unpaired
  surrogate, which no UTF-8 string can hold), and every column of every one of them, the exact text as well as
  whether there was an error, for `toUnicode` and for `toASCII` with and without transitional processing.
  `python-idna` was asked as a second opinion and agreed everywhere it answered at all; it refuses most of the file,
  being IDNA2008 proper rather than UTS #46.
- **The tables are 17.5 KB**: 13.3 for the status of every code point and 4.2 for the joining types the ContextJ
  rule reads, generated by `tools/unicode_tables.py` from `IdnaMappingTable.txt` and `ArabicShaping.txt` of
  Unicode 16.0.0. **What a mapped code point becomes is not among them**, and that is where 58.6 KB went. Of the 6352
  code points UTS #46 maps, 6347 are exactly what [nfkc_casefold](nfkc_casefold.md) gives, and the module works
  NFKC_Casefold out of the compatibility decompositions and the full case folding rather than keeping a table of it.
  So the mapping is computed the same way, and the five that differ are a `switch`: the capital sharp s `ẞ`, which
  folds to `ss` for everybody else and which UTS #46 sends to the small sharp s so that it lands on a deviation
  character; the two ideographic full stops `。` and `｡`, which are label separators here and ordinary characters
  everywhere else; and the two zero width joiners, which NFKC_Casefold drops for being default ignorable and which
  the mapping here, dropping nothing, writes out. The generator asserts the whole of that against
  `IdnaMappingTable.txt` before it writes anything, and a test holds the arithmetic against `nfkc_casefold` over
  every code point the standard maps, so the two cannot drift apart.
- A name that is already what the DNS carries, lower case letters, digits, hyphens and stops and no `xn--` label, is
  checked over its bytes and comes back as the object it went in as, nothing allocated; that is most of the traffic
  there is. The costs are on [Benchmarks: Domain names](benchmarks.md#domain-names).

## Member types

| Type | Definition |
|---|---|
| [error](idna-error.md) | which rule a name broke |
| [failure](idna-failure.md) | which rule a name broke, and in which label |
| [options](idna-options.md) | what is checked, and the two profiles |
| [outcome](idna-outcome.md) | the text even when it is wrong, and what was wrong with it |

## Member objects

| Constant | Description |
|---|---|
| `version` | the Unicode version of the tables, `"16.0.0"`, `unicode::version`: what a name maps to is fixed by it, and two programs that disagree about it disagree about where a name points |

## Member functions

| Function | Description |
|---|---|
| [ascii_form](idna/ascii_form.md) | the name as the DNS carries it, and what was wrong with it |
| [message_of](idna/message_of.md) | the rule of an error, in words |
| [to_ascii](idna/to_ascii.md) | the name as the DNS carries it, or the rule it broke |
| [to_unicode](idna/to_unicode.md) | the name as a reader writes it, or the rule it broke |
| [unicode_form](idna/unicode_form.md) | the name as a reader writes it, and what was wrong with it |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto host = txt::idna::to_ascii("bücher.example.de");
    if (host) {
        println("{}", *host);
    } else {
        println("{} (label {})", host.error().message(), host.error().label);
    }
    println("{}", txt::idna::to_unicode("xn--bcher-kva.example.de").value());

    txt::idna::options old;
    old.transitional = true;
    println("{} {}", txt::idna::to_ascii("faß.de").value(),
            txt::idna::to_ascii("faß.de", old).value());
    println("Unicode {}", txt::idna::version);
}
```

Output:

```text
xn--bcher-kva.example.de
bücher.example.de
xn--fa-hia.de fass.de
Unicode 16.0.0
```

## See also

- [punycode](punycode.md): one label between Unicode and ASCII
- [percent](percent.md): the other half of the text of a URL
- [nfkc_casefold](nfkc_casefold.md): what the mapping is worked out from
- [bidi](bidi.md): the classes the rule of RFC 5893 reads
- [net](../net/README.md): which is given the ASCII
