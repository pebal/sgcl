# txt::idna

```cpp
#include "sgcl/txt/idna.h"
```

The name of a host, written in somebody's own script and written the way the DNS carries it.

This is text work and not network work, which is why it lives in `txt` and not in `net`. What `bücher.de` is called in the DNS follows from case, from normalization and from the bidirectional classes, and all three are in this module already. A socket never sees any of it — it is given the ASCII the name turns into, and hands back the ASCII a lookup answered.

Two things, the second built on the first:

| | |
|---|---|
| `punycode` | one label between Unicode and ASCII ([RFC 3492](https://www.rfc-editor.org/rfc/rfc3492)) |
| `idna` | a whole domain name, both ways ([UTS #46](https://www.unicode.org/reports/tr46/), [RFC 5890](https://www.rfc-editor.org/rfc/rfc5890)–5894) |

The other half of the text of a URL — the bytes of a path, escaped as `%XX` — is [`percent`](percent.md), which shares nothing with this one: a different specification, no tables, and nobody looking for it would open a file called `idna`.

## Where this header fails, and the rest of the module does not

Everywhere else in `txt`, a text that is wrong is repaired: an invalid byte is `U+FFFD`, a code point no encoding can carry is `'?'`. Here an operation can really fail, and a failure has to be visible. A name may hold a character no domain name may hold, a label may be longer than the 63 bytes the DNS carries, a label that says `xn--` may be nonsense. A name that is wrong must not be looked up, so the answer is an [`expected`](../core/expected.md) and a caller cannot walk past it.

## Names

```cpp
namespace punycode {
    optional<string> encode(const string& label);   // no "xn--" prefix either way
    optional<string> decode(const string& label);
}

namespace idna {
    enum class error : uint8_t { none, disallowed, not_normalized, hyphen, label_prefix,
                                 label_separator, leading_combining, joiner, bidi, std3,
                                 punycode, empty_label, label_too_long, name_too_long };
    const char* message_of(error) noexcept;

    struct failure {
        static constexpr size_t whole_name = size_t(-1);
        error rule;         // which criterion it broke
        size_t label;       // which label, counting from zero
        size_t at, size;    // that label's bytes in the text that was given
        string message() const;   // the rule, in words: message_of(rule)
    };

    struct options {
        bool transitional = false;              // deprecated by UTS #46
        bool use_std3_ascii_rules = false;
        bool check_hyphens = true;
        bool check_bidi = true;
        bool check_joiners = true;
        bool verify_dns_length = true;          // to_ascii only
        bool ignore_invalid_punycode = false;
        static constexpr options standard() noexcept;
        static constexpr options whatwg() noexcept;
    };

    struct outcome { string text; failure reason; explicit operator bool() const; };

    expected<string, failure> to_ascii(const string& name, options = {});
    expected<string, failure> to_unicode(const string& name, options = {});
    outcome ascii_form(const string& name, options = {});     // the text even when it is wrong
    outcome unicode_form(const string& name, options = {});
}
```

```cpp
using namespace sgcl;

auto host = txt::idna::to_ascii(string("bücher.example.de"));
if (host) {
    connect(*host);                                   // "xn--bcher-kva.example.de"
} else {
    auto why = host.error();
    log(why.message(), why.label);
}

// and back, for showing somebody
txt::idna::to_unicode(string("xn--bcher-kva.example.de"));   // "bücher.example.de"
```

`to_ascii` and `to_unicode` are the two a program calls. `ascii_form` and `unicode_form` answer the same question and hand back the text as well as what was wrong with it: UTS #46 converts as far as it can even when it fails, and that text is worth having — a browser shows the user the name it would not look up, with the bad label marked.

## A failure names the label, not the name

A name is refused for something **one of its labels** did, and a caller who has to show the name needs to know which: a browser underlines the label, it does not grey out the address bar. So `failure` carries the label — which one it is, counting from zero — and the bytes it took up **in the text that was handed in**, which are not the bytes of the converted text and are what a caller can point at.

```cpp
auto why = txt::idna::ascii_form(name).reason;
if (why.rule != txt::idna::error::none && why.label != txt::idna::failure::whole_name) {
    underline(name.view().substr(why.at, why.size));
}
```

The range is of the label **as it arrived**, so it covers what was mapped away and what was ignored: in `a.-b­.com` the label at fault is the five bytes of `-b` and the soft hyphen, even though the soft hyphen leaves no trace in the answer; and in `a。-b.com` the second label begins after the three bytes of the ideographic full stop, not after the one byte of the stop it becomes. `name_too_long` is the one failure that is no label's fault, and it says so with `label == failure::whole_name`.

Finding those bytes costs a second pass over the name, and it is made only once the name has already failed. Writing the positions down on the way through every name cost 150 ns on every name that was fine, which is the wrong trade for an answer wanted once in some thousands; done this way it costs about 5 ns on the fast path and nothing measurable on the slow one.

## Transitional and nontransitional, which is what people trip over

Four characters mean one thing to IDNA2003 and another to IDNA2008: `ß`, the Greek final sigma `ς`, and the two zero width joiners. UTS #46 calls them **deviations** and gives two answers.

| | `faß.de` | `σόλος.gr` |
|---|---|---|
| nontransitional (the default) | `xn--fa-hia.de` | `xn--wxaijb9b.gr` |
| transitional (deprecated) | `fass.de` | the final sigma written as an ordinary one |

Transitional processing was meant to carry the older standard over during a changeover that is long finished. It silently sends two spellings of one name to two different hosts, every browser now uses nontransitional processing, and UTS #46 deprecates it. `options::transitional` exists so that a program that has to reproduce an old answer can, and nothing new should ask for it.

Punycode is never remapped, whichever was asked for: `xn--fa-hia.de` reads back as `faß.de` under both. Something already encoded was encoded by somebody who had decided.

## The profiles

`options::standard()` — the default — checks everything and holds the name to what the DNS will carry. `options::whatwg()` is what a browser's URL parser wants: the hyphens and the lengths are not checked, so names already in the wild keep working, while the bidirectional and joiner rules still are.

`use_std3_ascii_rules` is off by default, as the browsers have it. With it off, UTS #46 lets a great deal of ASCII through that IDNA2008 itself would not — an underscore, and more surprisingly `a♥b.com`, since the mapping table marks such characters valid for UTS #46 and only notes (`NV8`) that IDNA2008 would refuse them. With it on, an ASCII character must be a lowercase letter, a digit or a hyphen. Refusing more than that is the caller's business.

## What the checks are

Each label of a name is held to the validity criteria of UTS #46 section 4.1: it must be in normalization form C, must not have `--` in its third and fourth places or a hyphen at either end (`check_hyphens`), must not begin with a combining mark, must hold only code points the mapping table calls valid, must satisfy the **ContextJ** rules of RFC 5892 for the zero width joiners (`check_joiners`), and, in a name that has anything right to left in it anywhere, must satisfy all six conditions of **RFC 5893** (`check_bidi`). The bidirectional classes come from [`bidi`](bidi.md) rather than from a table of this header's own.

With `verify_dns_length` (the default), an empty label is an error unless it is the last one and something came before it: `example.com.` is a fully qualified name whose last label is the root of the DNS, `a..c` is not a name. Without it, as in `options::whatwg()`, an empty label is no error, as UTS #46 has it since Unicode 15.1 and as the WHATWG URL Standard needs (`x..ß` is the host `x..xn--zca`). With `verify_dns_length`, `to_ascii` also holds each label to 63 bytes and the whole name, the root label and its dot not counted, to 253.

## The limit: CONTEXTO is not checked

`check_joiners` is **ContextJ**, which is appendices A.1 and A.2 of RFC 5892 — the two zero width joiners — and that is all of the contextual rules this header applies. **CONTEXTO**, appendices A.3 to A.9, is not implemented: the middle dot between two l's in Catalan, the Greek lower numeral sign, the Hebrew geresh and gershayim, the katakana middle dot, and the rule that a label may not mix the two families of Arabic-Indic digits.

That is a decision, and here is where it stops being the right one. Those rules exist to stop a name from being **registered**, and this library is at the other end of the wire: a client looks a name up, and the registry it is looking it up in is what applied them. `IdnaTestV2.txt` says the same in its own header — it calls the CONTEXTO tests optional for client software and carries none of them — and a client that enforced them would refuse names that are registered and resolve today.

**The day this is used to decide whether a name may be taken rather than whether one may be resolved, they are needed.** Writing them wants the [`script`](properties.md) property, which this module already has, and a small table for the digits; it is an afternoon's work and not a design question.

## Punycode on its own

`punycode::encode` and `decode` take and give one label without the `xn--` prefix: knowing whether a label has one is IDNA's business, and punycode itself is only the encoding. Both hand back nothing rather than a wrong answer.

Section 6.4 of RFC 3492 says in as many words that an implementation must detect the overflow of its counters rather than let them wrap, and that is where the published attacks on punycode have been: a decoder whose `i` wraps writes a code point the label did not ask for, at a position it did not ask for. Every addition and every multiplication here is bounded before it is made, the arithmetic is done 64 bits wide against a 32 bit bound, and the code point that comes out is checked to be one — the RFC's own decoder lets `n` run to 2³² and leaves it to the caller to notice. Eight nines in a row is already past the bound.

## What it costs

A name that is already what the DNS carries — lower case letters, digits, hyphens and stops, and no `xn--` label — is checked over its bytes and comes back as the object it went in as, nothing allocated: `to_ascii` of `www0.example0.com` costs 60 ns against the 370 the general road cost. That is most of the traffic there is. A name that does need the work costs about 550 ns for two labels of German, the quick check properties of [`normalize`](normalize.md) settling the normalization without a decomposition in the usual case; reading an encoded name back costs about the same.

The tables are **17.5 KB**: 13.3 for the status of every code point and 4.2 for the joining types the ContextJ rule reads. They are generated by `tools/unicode_tables.py` from `IdnaMappingTable.txt` and `ArabicShaping.txt` of Unicode 16.0.0.

**What a mapped code point becomes is not among them**, and that is where 58.6 KB went. Of the 6352 code points UTS #46 maps, 6347 are exactly what `NFKC_Casefold` gives, and [`identifier`](identifier.md) already works `NFKC_Casefold` out — it is the compatibility decomposition, folded and put back together, out of tables that normalization and the case mappings carry anyway — rather than keeping one. So this header computes it the same way and carries the five that differ in a `switch`. The generator asserts the whole of that against `IdnaMappingTable.txt` before it writes anything, and a test holds the header's own arithmetic against `nfkc_casefold` over every code point the standard maps, so the two cannot drift apart.

The five: the capital sharp s `ẞ`, which folds to `ss` for everybody else and which UTS #46 sends to the small sharp s so that it lands on a deviation character; the two ideographic full stops `。` and `｡`, which are label separators here and ordinary characters everywhere else; and the two zero width joiners, which `NFKC_Casefold` drops for being default ignorable and which this header, dropping nothing, writes out.

It is not slower for it. A name typed in capitals got **faster** — `WWW0.EXAMPLE0.COM` went from 485 ns to 416, because the only ASCII the standard maps is `A`–`Z` and each to the letter 32 above it, which is a line rather than a search. A name of capitals with accents in it costs 6 per cent more (666 ns to 708), and every other path — a lowercase name, an encoded one, the fast path, punycode — is unchanged.

## The oracle

`IdnaTestV2.txt` of the UCD, in full: 6387 of its 6389 cases — the two left out carry an unpaired surrogate, which no UTF-8 string can hold — and every column of every one of them, the exact text as well as whether there was an error, for `toUnicode` and for `toASCII` with and without transitional processing. All the sample strings of RFC 3492 section 7.1 hold the punycode, both ways. `python-idna` was asked as a second opinion and agreed everywhere it answered at all; it refuses most of the file, being IDNA2008 proper rather than UTS #46.
