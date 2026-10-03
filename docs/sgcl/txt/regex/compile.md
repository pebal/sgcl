[sgcl](../../README.md) › [txt](../README.md) › [regex](../regex.md)

# sgcl::txt::regex::compile

```cpp
static expected<regex, regex_error> compile(const string& pattern) noexcept;                 // (1)
static expected<regex, regex_error> compile(const slice<const char>& pattern);               // (2)
template<size_t N> static expected<regex, regex_error> compile(const char (&pattern)[N]);    // (3)
template<class P> requires std::same_as<P, const char*> || std::same_as<P, char*>
static expected<regex, regex_error> compile(P pattern);                                      // (4)
```

Compiles a pattern the program only has where it runs — read from a file, typed by a user — and gives either the
regex or the reason it is not one. A pattern written into the program is better given to the
[constructor](regex.md), which the compiler checks.

1. The pattern in a string.
2. The bytes of a slice, copied into a string the regex holds.
3. An array of `char`, a literal among them, up to its first NUL or its end, never past it.
4. The characters at a pointer, up to their NUL.

## Parameters

| Parameter | Description |
|---|---|
| `pattern` | the pattern, in the syntax below |

## Return value

The compiled regex, or a [regex_error](../regex_error.md): its [message](../regex_error/message.md) is
`sgcl::txt::regex: <the reason> (at byte <n> of the pattern)` and its [offset](../regex_error/offset.md) the byte
`n`.

## Complexity

Linear in the length of the pattern once a `{n,m}` is spelled out into instructions.

## Exceptions

- (1) None.
- (2–4) `length_error` when the pattern is longer than 4294967295 bytes, the most a [string](../../core/string.md)
  holds: it is copied into one first.

## Notes

### The syntax

| Element | Written |
|---|---|
| Literals | any code point, `\n \r \t \f \v \a \e \0`, `\xHH`, `\x{10FFFF}`, `\uHHHH`, `\UHHHHHHHH`, and `\` before any punctuation mark |
| `.` | one code point; a line feed only under `(?s)` |
| Classes | `[abc]`, `[a-z]`, `[^a-z]`, `[a-z0-9_]`, and the shorthands inside them |
| Shorthands | `\d` `\D` (Nd), `\w` `\W` (a letter, a number or `_`), `\s` `\S` (White_Space) |
| Properties | `\p{L}`, `\pL`, `\p{Lu}`, `\P{Nd}`, `\p{Script=Cyrillic}`, `\p{sc=Greek}`, `\p{Old_Italic}`: a general category or a script |
| Boundaries | `\b`, `\B` |
| Anchors | `^` `$` (the edges of the text, or of a line under `(?m)`), `\A`, `\z` |
| Groups | `( )`, `(?: )`, `(?<name> )` and Python's `(?P<name> )`; a name is a letter followed by letters, digits and underscores |
| Alternation | `a\|b\|c` |
| Quantifiers | `* + ? {n} {n,} {n,m}`, each with `?` after it for the lazy form; a `{` that does not begin a count (`a{,3}`, `a{x`) stands for itself |
| Flags | `(?i)` `(?m)` `(?s)`, `(?-i)`, `(?ims)`, `(?i:...)`: set from that point to the end of the group they stand in |

The walk is over code points: `.` takes one character however many bytes it is written in, `[а-я]` is a range of
Cyrillic letters, `\w` is a letter in any script, and no word boundary falls inside a character.

**Without regard to case**, `(?i)` is simple folding, one code point to one: `k`, `K` and the Kelvin sign are one
letter, `σ` and `ς` are one letter. It is not the full folding of [fold_case](../fold_case.md), under which `ß` is
`ss`: one code point cannot become two in a matcher without the position in the text ceasing to mean anything.
`(?i)straße` matches `STRASSE` nowhere, and neither does it in RE2 or in Go.

### What is refused: no backtracking

A **backreference** — `\1` for "whatever group one matched" — and a **lookaround** — `(?=...)` for "and this holds
here too" — both need the engine to be able to go back and try again, and an engine that can go back can be made to
go back exponentially often. `(a+)+b` over thirty `a` and no `b` is a second of work in Perl and in Python; over
forty it is a fortnight. Every one of those is a denial of service waiting for a pattern or a text from outside the
program. So the parser refuses them by name, with the reason, and the same for the three constructs that exist to
cut a backtracking engine short and have nothing here to cut:

| Construct | Refused as |
|---|---|
| `\1` | a backreference |
| `(?=...)`, `(?!...)`, `(?<=...)`, `(?<!...)` | a lookahead or a lookbehind |
| `(?>...)` | an atomic group |
| `(?(1)...)` | a conditional group, a backreference by another name |
| `a++` | a possessive quantifier |

A malformed pattern is refused with what is wrong with it: a `(`, a `)` or a `[` without its pair, an empty class
(`]` alone is written `\]`), a `\` at the end, an escape this engine does not know, a quantifier with nothing to
repeat or on another quantifier, a count whose bounds are reversed, `m` below `n` (`a{2,1}`), a class range whose end comes
before its start, `\b` inside a class, a `\p` without a name or with one no general category and no script goes by,
an escape whose digits are no code point, a bad or repeated group name, a flag other than `i`, `m` and `s`.

### Where this parts company with Perl and Python

The answers are leftmost-first, as Perl's and Python's are. Only the time is different — and two things besides,
both in the same place.

A repetition **whose body can match nothing** is where they differ. Perl and Python stop such a loop the moment a
turn of it consumes nothing, which is how they keep from looping forever; a machine that carries every alternative
at once has no such danger and instead drops the turn that consumed nothing, because another thread has already
stood on that instruction at this position. So:

- `(?:a*|b)*` over `"aab"` finds `aab` here and `aa` there;
- `(a*)*` over `"aab"` matches `aa` in both, but group one holds `aa` here and the empty string after it there.

RE2 and Go part company with Perl in the same place, for the same reason. A body that must consume something
behaves everywhere alike.

Two smaller differences of definition: `$` is the end of the text, not the place before a last line feed (Python's
`$` is both; its `\Z` is what `$` and `\z` are here); and walking the occurrences of a pattern that can match
nothing, this engine moves on by a code point where Python tries the same position again demanding a wider match.

### Limits

| Limit | Value |
|---|---|
| Nesting of `(` and `[` | 200 levels |
| The `n` and `m` of `{n,m}` | 1000 |
| Capturing groups | 250 |
| Instructions, after `{n,m}` is spelled out | 20000 |

None of these is a limit of the algorithm; they are the line past which a pattern is likelier a mistake or an
attack than a question. Past one of them `compile` says which.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto ok = txt::regex::compile("\\p{Lu}\\w+");
    println("{}", ok->find("powiedział Jan")->text());

    auto bad = txt::regex::compile("(a+)+\\1");
    println("{}", bad.error().message());
}
```

Output:

```text
Jan
sgcl::txt::regex: a backreference: this engine matches in time linear in the length of the text, carrying every alternative at once, and nothing in it can be asked to repeat what another part matched (at byte 5 of the pattern)
```

## See also

- [(constructor)](regex.md): a pattern written into the program, checked by the compiler
- [regex_error](../regex_error.md): the reason and its place
- [sgcl::txt::regex](../regex.md)
