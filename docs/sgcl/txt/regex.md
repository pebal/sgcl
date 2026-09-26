# txt::regex

```cpp
#include "sgcl/txt/regex.h"
```

Patterns in the style of RE2: everything a pattern can ask for here can be answered in **one pass over the text**, and nothing else is offered. The time a match takes is the length of the text times the length of the pattern, whatever the two are — there is no pattern that costs more, and that is what the class is for.

```cpp
class regex {
    regex(const detail::regex_pattern& pattern);                    // a literal, read by the compiler
    static expected<regex, regex_error> compile(const string& pattern);
    static expected<regex, regex_error> compile(const slice<const char>& pattern);

    const string& pattern() const noexcept;
    size_t group_count() const noexcept;
    optional<size_t> group_index(const string& name) const noexcept;
    size_t program_size() const noexcept;

    bool full_match(const string& text) const;                       // the whole text, end to end (Go's MatchString searches: contains)
    bool contains(const string& text) const;
    optional<match> find(const string& text, size_t from = 0) const;
    regex_matches all(const string& text) const;                     // every match, never overlapping
    size_t count(const string& text) const;

    string replace(const string& text, const string& with) const;    // $1, ${name}, $0, $$
    string replace_first(const string& text, const string& with) const;
    vector<slice<const char>> split(const string& text, size_t limit = 0) const;
};

class match {
    slice<const char> text() const noexcept;                         // the whole match
    size_t begin_at() const noexcept;
    size_t end_at() const noexcept;
    bool empty() const noexcept;
    size_t group_count() const noexcept;                             // groups, the whole match not counted
    optional<slice<const char>> group(size_t n) const noexcept;      // 0 is the whole match
    optional<slice<const char>> group(const string& name) const noexcept;
    slice<const char> operator[](size_t n) const noexcept;           // a group that took no part reads as empty
    const slice<const char>& subject() const noexcept;
};

class regex_error {
    string message() const;                                          // the sentence, and where
    size_t offset() const noexcept;                                  // the byte of the pattern
};
```

Every name that takes a `const string&` takes a [`slice<const char>`](../core/slice.md) as well.

## What there is no backtracking for

A **backreference** — `\1` for "whatever group one matched" — and a **lookaround** — `(?=...)` for "and this holds here too" — both need the engine to be able to go back and try again, and an engine that can go back can be made to go back exponentially often. `(a+)+b` over thirty `a` and no `b` is a second of work in Perl and in Python; over forty it is a fortnight. Every one of those is a denial of service waiting for a pattern or a text from outside the program.

So the parser refuses them by name, with the reason:

```
sgcl::txt::regex: a lookahead or a lookbehind: this engine matches in time
linear in the length of the text, in one pass, and a lookaround cannot be
had that way (at byte 3 of the pattern)
```

The same for an atomic group `(?>...)`, a conditional group `(?(1)...)` and a possessive quantifier `a++`, all of which exist to cut a backtracking engine short and have nothing here to cut.

## What a pattern may say

| | |
|---|---|
| literals | any code point, `\n \r \t \f \v \a \e \0`, `\xHH`, `\x{10FFFF}`, `\uHHHH`, `\UHHHHHHHH`, and `\` before any punctuation mark |
| `.` | one code point, a line feed only under `(?s)` |
| classes | `[abc]`, `[a-z]`, `[^a-z]`, `[a-z0-9_]`, and the shorthands inside them |
| shorthands | `\d` `\D` (Nd), `\w` `\W` (a letter, a number or `_`), `\s` `\S` (White_Space) |
| properties | `\p{L}`, `\pL`, `\p{Lu}`, `\P{Nd}`, `\p{Script=Cyrillic}`, `\p{sc=Greek}`, `\p{Old_Italic}` |
| boundaries | `\b`, `\B` |
| anchors | `^` `$` (the edges of the text, or of a line under `(?m)`), `\A`, `\z` |
| groups | `( )`, `(?: )`, `(?<name> )` and Python's `(?P<name> )` |
| alternation | `a|b|c` |
| quantifiers | `* + ? {n} {n,} {n,m}`, each with `?` after it for the lazy form |
| flags | `(?i)` `(?m)` `(?s)`, `(?-i)`, `(?ims)`, `(?i:...)` — set from that point to the end of the group they stand in. **`(?i)` is simple folding, one code point to one**, so `k`, `K` and the Kelvin sign are one letter but `ß` is not `ss`; see below |

There is no flags argument on `compile`: a flag belongs in the pattern, where whoever reads the pattern can see it, and it is spent where the program is built rather than branched on while the text is walked.

**The walk is over code points.** `.` takes one character however many bytes it is written in, `[а-я]` is a range of Cyrillic letters and not of bytes, `\w` is a letter in any script, and no word boundary falls inside a character. The positions a `match` reports are byte offsets, because a slice of a text is bytes of it.

**Without regard to case** (`(?i)`) is simple folding, one code point to one: `k`, `K` and the Kelvin sign are one letter, `σ` and `ς` are one letter. It is not the full folding of [`fold_case`](case.md), under which `ß` is `ss` — one code point cannot become two in a matcher without the position in the text ceasing to mean anything. `(?i)straße` matches `STRASSE` nowhere, and neither does it in RE2 or in Go.

## Where this parts company with Perl and Python

The answers are leftmost-first, as Perl's and Python's are: the first branch of an alternation before the second, a greedy quantifier taking what it can and a lazy one taking what it must. Only the time is different — and two things besides, both in the same place.

A repetition **whose body can match nothing** is where they differ. Perl and Python stop such a loop the moment a turn of it consumes nothing, which is how they keep from looping forever; a machine that carries every alternative at once has no such danger and instead drops the turn that consumed nothing, because another thread has already stood on that instruction at this position. So:

* `(?:a*|b)*` over `"aab"` finds `aab` here and `aa` there;
* `(a*)*` over `"aab"` matches `aa` in both, but group one holds `aa` here and the empty string after it there.

RE2 and Go part company with Perl in the same place, for the same reason. A body that must consume something behaves everywhere alike.

Two smaller differences of definition: `$` is the end of the text, not the place before a last line feed (Python's `$` is both; its `\Z` is what `$` and `\z` are here); and walking the occurrences of a pattern that can match nothing, this engine moves on by a code point where Python tries the same position again demanding a wider match.

## A literal pattern is read by the compiler

A pattern written into the program is checked where the program is compiled, as [`format`](format.md)'s pattern is:

```cpp
txt::regex re("(?<n>\\d+)\\s*(?i:kg)");     // no optional, no error to handle
```

`txt::regex bad("(a)\\1");` does not build. The compiler points at the line in `regex_pattern` that says why, and `regex::compile` gives the same sentence with the place in the pattern for a pattern that only arrives while the program runs.

## What a match holds

A `match` gives its pieces as [`slice<const char>`](../core/slice.md) of the text it was found in, and a slice holds the object its characters live in. So a match outlives the string it came from, and a loop over the matches of a temporary is safe — which a `std::smatch` over a `std::string_view` is not.

A group that **took no part** in the match is nothing, which an empty group is not: in `(a)|(b)` over `"b"` group one took no part, and in `(a?)b` over `"b"` group one matched and is empty. That is why `group()` is an `optional`; `operator[]` is the short way for whoever does not care.

`all()` is a range of the library ([`mixin::enumerable`](../core/mixin/enumerable.md)) like [`words`](segment.md) and `graphemes`: decided as it is walked rather than gathered into a container first. A match of no width moves the search on by one code point, in `all()`, in `split()` and in `replace()`, or the walk would stand still.

## Example

```cpp
#include "sgcl/sgcl.h"
#include "sgcl/txt/regex.h"
#include <iostream>

using namespace sgcl;

int main() {
    txt::regex date("(?<y>\\d{4})-(?<m>\\d{2})-(?<d>\\d{2})");
    string text = "spotkanie 2026-09-23, a potem 2026-12-01";

    if (auto m = date.find(text)) {
        std::cout << m->text() << ": rok " << *m->group("y") << '\n';
    }
    for (const auto& m : date.all(text)) {
        std::cout << m.begin_at() << ": " << m.text() << '\n';
    }
    std::cout << date.replace(text, string("${d}.${m}.${y}")) << '\n';

    for (auto piece : txt::regex("\\s*,\\s*").split(text)) {
        std::cout << '[' << piece << "]\n";
    }

    auto bad = txt::regex::compile(string("(a+)+\\1"));
    std::cout << bad.error().message() << '\n';
    return 0;
}
```

The output:

```
2026-09-23: rok 2026
10: 2026-09-23
30: 2026-12-01
spotkanie 23.09.2026, a potem 01.12.2026
[spotkanie 2026-09-23]
[a potem 2026-12-01]
sgcl::txt::regex: a backreference: this engine matches in time linear in the length of the text, carrying every alternative at once, and nothing in it can be asked to repeat what another part matched (at byte 5 of the pattern)
```

## What it costs

`benchmarks/txt/regex.cpp` is the program these came out of — `bench_regex sgcl <op>` and `bench_regex std <op>` — so the next change to this header has something to measure against.

A pattern that is **not** in a text of four thousand bytes, so the whole of it is walked, against `std::regex` (libc++, ECMAScript, `optimize`) on the same data:

| op | pattern | sgcl ns/byte | std::regex ns/byte |
|---|---|---|---|
| `date` | `(\d{4})-(\d{2})-(\d{2})` | 0.039 | 110.1 |
| `address` | `(\w+)@(\w+)\.(com\|pl)` | 0.039 | 358.1 |
| `upper` | `[QWX]+` | 0.43 | 76.7 |
| `class` | `[0-9]+` | 0.44 | 85.3 |
| `literal` | `zyzykot` | 0.52 | 41.3 |
| `boundary` | `\bzyzykot\b` | 0.52 | 46.6 |
| `dotstar` | `zyz.*kot` | 1.11 | 71.0 |
| `alt` | `kot\|pies\|ryba` | 5.19 | 126.7 |

The spread in the first column is **what the search may skip**. A run of bytes every match must contain is read off the pattern where it is compiled, and where there is one the text is scanned for it — by the same Boyer–Moore–Horspool table [`searcher`](search.md) uses, or by a `memchr` when the run is a single byte — instead of stepping the machine at every position. `(\w+)@(\w+)\.(com|pl)` begins with `\w`, which nearly every byte is, so every position used to be a candidate and it cost 24.6 ns a byte; it must contain an `@`, and looking for that costs 0.039. Where the run is also the *beginning* of every match, as in `zyzykot`, the search jumps straight to the next place it stands. Where the pattern gives no such run at all — an alternation, a class — the fall-back is the set of bytes a match may begin with, one test a byte, which is the 0.43 those rows show. The second column has no spread because a backtracking engine pays for the pattern at every position whatever it starts with.

The rest, over the same four kilobytes:

| op | sgcl | std::regex |
|---|---|---|
| `hit` — a word that is there, found a third of the way in | 354 ns | 6990 ns |
| `line` — the address over one line of sixty bytes | 60 ns | 22280 ns |
| `find` — the address with its three groups | 190 ns | 1427828 ns |
| `all` — every one of the 842 words | 98 µs, 117 ns a match | 434 µs, 516 ns a match |
| `replace` — each of them wrapped | 105 µs | 456 µs |
| `split` — on every word | 93 µs | not the same question |
| `build` — compiling the date pattern | 3196 ns | 670 ns |

Building a `regex` is the one row that goes the other way: this engine spells a `{n,m}` out into instructions where a backtracking one keeps a counter, and that is the price of not having one.

And the case the whole header is about, `(a+)+b` over *n* characters with no `b` in them:

| n | sgcl | std::regex |
|---|---|---|
| 28 | 0.0009 ms | 1.6 ms, and then it gives up |
| 1 000 | 0.047 ms | — |
| 100 000 | 2.9 ms | — |
| 1 000 000 | 28 ms | — |

libc++ does not hang on it: it counts its steps and throws `regex_error` with "the complexity of an attempted match exceeded a pre-set level" — at sixteen characters already. Which is the point. The pattern does not work there at all, and here it is linear.

## What it is held to

Python's `re` is the oracle, over **3000 random patterns** asked for their first match, **800** for every match and **600** for the pieces a split leaves, generated by `tools/unicode_tables.py` beside the module's other test files. Only what the two are defined to answer the same way is compared; where they differ by design the pattern is translated in the generator or the case is left out, and both differences are asserted directly in a test of their own.

The generator's own first run did not return, because a pattern drawn at random from three atoms and a quantifier sends `re` into an exponential search every few hundred draws. Every question put to the oracle is now given a quarter of a second.

Beside that: eight patterns a backtracking engine goes exponential on, each with a **hard limit in milliseconds** in the test rather than an argument about why it should be fast; and a fuzzer of 60000 rounds under the address and undefined-behaviour sanitizers through every entry point, over texts with lone continuation bytes, a truncated sequence, an encoded surrogate, a Tibetan `0F73`, a final sigma, a sharp s and a Kelvin sign.

## Limits

A pattern may nest 200 levels deep, count to 1000 in a `{n,m}`, have 250 capturing groups and spell out to 20000 instructions. None of these is a limit of the algorithm; they are the line past which a pattern is likelier a mistake or an attack than a question. Past one of them `compile` says which.
