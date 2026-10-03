[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::format

```cpp
#include "sgcl/txt/format.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    /*(1)*/ template<class... A>
            string format(const format_pattern<std::type_identity_t<A>...>& pattern,
                          const A&... args);
    /*(2)*/ template<class... A>
            optional<string> format(const runtime_pattern& pattern, const A&... args);
}
```

Text built from a pattern and the values that go in it, `txt::format("{} left", n)`. The pattern is the one of
[std::format](https://en.cppreference.com/w/cpp/utility/format/spec) — a pair of braces for a value, a colon and a
[specification](#the-specification) after it for how to write it — and the result is a
[string](../core/string.md).

1. A pattern written in the program, a literal: it is turned into a [format_pattern](format_pattern.md) on its way in
   and **read where the program is compiled**, not where it runs. A brace left open, a number with no value behind
   it, a precision asked of a whole number, `{:d}` over a name — a pattern that does not fit the values it is given
   is an error of the compiler and not a surprise at the customer's.
2. A pattern the compiler never saw, asked for by name with [runtime](runtime.md): a catalogue of translations read
   from a file, the text of a message chosen by the language of whoever reads it. The pattern is walked once and
   every field is weighed just before it is written; the first that does not fit its value stops the walk, what
   was written by then is thrown away, and the answer is `nullopt` — nothing half built is ever handed out.
   Everything the compiler refuses on the other road is `nullopt` here.

[format_to](format_to.md) writes the same text into memory the caller lends, and [fits](fits.md) asks of a runtime
pattern whether it fits, with nothing written.

## Parameters

| Parameter | Description |
|---|---|
| `pattern` | the pattern: a literal (1), or a [runtime](runtime.md) of a text (2) |
| `args` | the values of the fields, of any type with a [formatter](formatter.md) or a `format_value` beside it |

## Return value

1. The text.
2. The text, or `nullopt` when the pattern does not fit the values.

## Complexity

Linear in the length of the text. (1) reads nothing of the pattern where the program runs when it has at most four
steps (a field and the literal text before it, or the text after the last field): the call walks the steps. A
longer pattern, or one with a number too large for a step, is read where it runs, linear in its length. (2) is
linear in the length of the pattern as well.

## Exceptions

- `out_of_range` when a `{:c}` field is given an integer no `char` holds, on both roads: that is a fault of the
  value, not of the pattern, and (2) does not turn it into `nullopt`.
- `length_error` when the text passes 4 GiB, the most a string holds.
- What a `format_value` or a [formatter](formatter.md) of the program throws.

Nothing is returned when a value throws.

## Notes

### Why txt::format and not format

`format(...)` written bare next to a [string](../core/string.md) of the library **is not this one**:
`basic_string` names `std::char_traits` among its arguments, so `std` is an associated namespace of it and the call
finds `std::format` — it does not announce a clash, it wins, and fails later on a `std::string` that will not
convert. Written as `txt::format`, nothing is ambiguous, and a bare `format` still means the standard's.

### The specification

Everything `std::format` specifies for the types below, in the same order — `[[fill]align][sign][#][0][width][.precision][type]`,
read into a [format_spec](format_spec.md). The output is checked against the standard's own implementation over 449
combinations of specification and value, and agrees on all of them.

| Type | Takes |
|---|---|
| integers | `d` `b` `B` `o` `x` `X` `c`, the signs `+` `-` and space, `#`, `0`, a width |
| floating point | `f` `F` `e` `E` `g` `G` `a` `A`, a precision (six when a form is named and none is given), the signs, `#`, `0`, a width |
| `bool` | `s` (the default: `true`/`false`) and the integer forms |
| `char`, `char32_t` | `c` (the default), `?`, the integer forms; a code point is written as UTF-8 |
| text (`string`, `slice<const char>`, `const char*`, `std::string_view`) | `s`, `?`, a width and a precision **in columns** |
| enumerations | the integer forms over the underlying type; **not** `c` and **not** `s` |
| pointers | `p`, the address in hexadecimal after `0x` |
| ranges, tables, pairs, tuples | `n` (no brackets), `m` over a pair (`k: v`), a width; and a second colon for the elements |
| `optional` | whatever the value it holds takes |
| [duration](../core/duration.md) | `s` (the default: `1h30m0.5s`, as `to_string` writes it), a width |
| the times of [time](../time/README.md#formatting-with-txt) and `<chrono>` | `[[fill]align][width]` and a [pattern of time](#a-pattern-of-time) |

`{{` and `}}` stand for a brace. A field may name its value by number — `{1} {0}` — or leave it out, and then the
values are taken in order. The two may be mixed, which `std::format` refuses: a field without a number takes the
next value in order, counting only the fields without one, so `{0}{}` writes the first value twice.

**The fill is one code point**, as in `std::format`: `{:ż>5}` and `{:…^9}` pad with the whole character, each one
a column of the width however many bytes it takes. Bytes that are not one whole code point of UTF-8 before the
alignment — a byte above ASCII alone, a sequence cut short, two characters — are refused, since filled over a field
they would be broken text: an error of the compiler on the one road, `nullopt` on the other.

**A width or a precision may be given by a value**, as `std::format` has it: `{:>{}}` takes the width from the
value after the field's own, `{:.{2}f}` the precision from the third value. The value is an integer — not a `bool`
and not a character — which is checked with the rest of the pattern: an error of the compiler on the one road,
`nullopt` on the other. A negative width pads nothing and a negative precision is none given, where `std::format`
throws; a width past 65535 is held to 65535, a precision past 32767 to 32767. Only the field of the pattern takes one:
the specification after a second colon, which belongs to the elements, does not.

**A width may go up to 65535**, which is what a step of a compiled pattern holds it in, and past that the pattern is
refused: an error of the compiler on the one road, `nullopt` on the other. A precision may go up to 32767, and the
number of a field may not be bigger than any call could have values for. None of those is a field anybody writes;
the bounds are there so that sixteen characters of a pattern read from a file cannot ask for four gigabytes of
padding.

### The width and the precision of text

**The width and the precision count what the standard counts, not bytes**: the width counts grapheme clusters, as
`std::format` does; for the columns of a terminal there is [columns](columns.md). A cluster is one, or two when
its first code point is East Asian Wide or Fullwidth or in the blocks the standard names ([format.string.std]/13);
a control, a format character and a mark that begins a cluster are one as well, where `columns` gives them none.

Cut by its bytes, `{:.3}` of `żółć` would leave a lead byte with nothing behind it, which is not text any more; cut
by its code points, a letter would lose its accent. The precision stops on a cluster. The output is held to the
standard's own implementation, with three exceptions by name: libc++ counts CR LF as two clusters where UAX #29 (GB3)
makes it one; right after a control it counts the two emoji of a ZWJ sequence, or the two regional indicators of a
flag, as two clusters where UAX #29 (GB11, GB12) makes them one, so `\x02👨‍👩` is two columns wider there; and it
counts the zero before the point of a value below one among the digits of `{:#g}` (`{:#.3g}` of 0.5 is `0.50`
there), where `printf` and this header write `0.500`.

### Numbers

A `float` is written as a `float` and not as the `double` it widens to: the shortest text that reads back as `1.1f`
is `1.1`, where the shortest that reads back as the `double` is `1.100000023841858` — the same number, a different
question. A `long double` is written through `double`. A fraction, a value at or above 2^53, and any form with a
type or a precision are written by the standard's `to_chars`.

`{:c}` of an integer writes the `char` it is, and a number no `char` holds throws `out_of_range`: narrowing it
quietly would write a different character (`{:c}` of 300 would be a comma). An integer wider than `long long`
(`__int128`) is refused where the program is compiled, rather than narrowed to sixty-four bits.

### Text as a program would write it, {:?}

`{}` writes text to be read. `{:?}` writes it to be **told apart**: in quotes (single ones for a character), with
what a terminal cannot show written as an escape.

`\t`, `\n`, `\r`, `\\` and the quote that surrounds it have escapes of their own; everything else that cannot be
shown is `\u{...}`, in lower-case hexadecimal with no leading zeros. Which code points those are is the standard's
list, and the categories are the ones [category_of](category_of.md) already answers: the controls (`Cc`), the format
characters (`Cf`), the surrogates (`Cs`), the private use area (`Co`), the unassigned (`Cn`), the line and paragraph
separators (`Zl`, `Zp`), and every space separator (`Zs`) **but the space itself**. A byte that is part of no
sequence at all is not a character, so it goes out as `\x{ff}` — the byte it was, not the `U+FFFD` it would decode
to.

A width and a precision are over what comes out, escapes and quotes included, and the field is still measured in
columns.

**The elements of a range take this form by default.** This is the point of it, and it is the one rule here that
alters what a program written for an older version prints. Without the quotes `["a, b", "c"]` and a list of three
words are indistinguishable, which is why C++23 does the same (it is `set_debug_format` there). It applies to the
elements of a range, to the members of a pair or tuple, and to what an `optional` holds — so `optional<string>`
holding the word `nullopt` is `"nullopt"` and an empty one is `nullopt`. Only text and characters have a debug form,
so a list of numbers is untouched, and naming any type at all in the element specification — `{::s}` — takes it
back.

### Values made of other values

A list goes in brackets, a table in braces, a pair in parentheses, and a value that may not be there is either
itself or the word `nullopt`. The shapes are the ones C++23 settled on, because they are the ones people already
read.

**Anything with a `begin` and an `end`** is a list: [vector](../core/vector.md), [slice](../core/slice.md),
[array](../core/array.md), [deque](../core/deque.md), the [immutable](../immutable/README.md) containers,
`std::vector`, an `initializer_list`, a range of one's own. **Anything with a `key_type`** is a table and goes in
braces; with a `mapped_type` beside it, its elements are written `k: v`. **Anything structured bindings see** is a
pair or a tuple. Nothing is listed anywhere and nothing is included for it: the questions are asked of the type, so a
container written after this header still answers.

Text is a range of characters and is not taken for one — a [string](../core/string.md), a `slice<const char>`, a
`std::string`, a `const char*` all keep their own road.

`n` drops the brackets (or the parentheses), and `m` writes a pair as `a: b`, which is what one element of a table
is. An `optional` that holds a value writes it with the whole of its own specification, so `{:>6}` lines a column up
whether or not a row has a number in it; one that holds nothing writes `nullopt` in the same field.

### What follows a second colon

Everything after it belongs to the elements, and it goes down as many levels as the type has, one colon read at each.

A colon is still an ordinary character to pad with for every value that holds nothing — `txt::format("{::>6}", 42)`
is `::::42`. Only a value made of other values reads it as theirs, which is the rule C++23 uses and for the same
reason. A **closing brace cannot be the fill character** (`{:}<6}`), because a field ends at the first `}` that
does not close the `{}` of a width or a precision; C++23 forbids it too.

A width applies to the whole, and it is measured in columns like any other field, so a list of Polish or Japanese
words sits in its field as a word does. The padding cannot be known until the whole is written, so the body goes
first into room the thread keeps for the purpose and is padded where it lies; it is written once, the levels of one
field sharing one room. The room is kept between calls up to 64 KB, so only the first large field on a thread is
written a second time, and only its innermost level. A range without a width costs nothing extra.

### A pattern of time

A time reads the rest of its field as a pattern of its own, which is the grammar `std::format` gives the types of
`<chrono>`: `[[fill]align][width]` and then everything from the first `%` to the brace, colons included, so
`{:%H:%M}` is one field and not a nest.

The module [time](../time/README.md#formatting-with-txt) brings the formatters — its own `datetime`, `date` and
`weekday`, and `sys_time`, `local_time`, `year_month_day`, `weekday`, `hh_mm_ss` and `duration` of `<chrono>`,
written as `std::format` writes them — and the pattern is checked where the program is compiled like any other
specification (`{:%Q}` of a datetime is an error). A list hands the pattern on to its elements (`{::%d.%m}`). A
formatter of one's own reads such a field with [takes_layout](formatter/takes_layout.md); a sign, `#`, `0` and a type
are not read for it. A [stencil](stencil.md)'s values are its own kinds (text, numbers, lists, mappings), so a time
goes into one as the text it was written to.

### An enumeration

Neither an `enum class` nor a plain `enum` is an integral type; an enumeration is written as **the number it is**,
with the whole numeric specification over it, taken from the underlying type — which is also what decides whether
there is a sign. [byte](../core/aliases.md) is one too.

The number and not the name, because C++20 has no reflection and a table of names would have to be written by the
program in any case. `s` is deliberately left free: it is the shape one reaches for when writing the names.

**The names are the program's to give**, the same way any other type of its own gives them — a `format_value` beside
the enumeration, which wins over the formatter of the number. Nothing here has to be opened or specialized for that,
and [write_padded](write_padded.md) still pads the name in columns. A [formatter](formatter.md) of the program's own
for the enumeration works too and wins over both, where it wants to say which specifications the names accept.

### A type of one's own

Give it a `format_value` beside it, in its own namespace, and it is found there:

`void format_value(txt::format_sink& out, const T& value, const txt::format_spec& spec);`

It writes into the [format_sink](format_sink.md), usually through [write_padded](write_padded.md), which gives its
text the fill, the alignment and the width of the field. Nothing here has to be opened for that, and a type that says
nothing at all is a message from the compiler — "this type says nothing about how it is written" — rather than a link
error. A `format_value` accepts any specification; a [formatter](formatter.md) is what says which ones a type takes,
so that the compiler can refuse the others. A writing that may throw makes [format_to](format_to.md) potentially
throwing; one declared `noexcept` keeps it `noexcept`.

### A pattern read where the program runs

The answer of (2) is an [optional](../core/aliases.md), not an exception and not a reason: every reason has the same
remedy, which is to fall back on the pattern the program was written with, and that one is a literal the compiler
already checked — `txt::format(txt::runtime(entry), n).value_or(txt::format("{} left", n))` is the whole of what a
caller does. Whoever wants to know earlier asks [fits](fits.md) where the catalogue is loaded, naming the types the
program will pass. Naming a value by number — `{1} {0}` — matters more here than anywhere, because word order is the
first thing a translation changes.

A text longer than 256 bytes is written twice, on both roads: once to be measured, once into the string, so a
`format_value` is called twice for it. That is measured and stays that way — the second pass is `to_chars` and a
copy over a handful of fields, which costs less than the copying of a buffer that grows.

## Example

A pattern and the values that go in it:

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    int total = 10;
    string name = "Ada";
    string s = txt::format("{} left of {}", 3, total);
    string t = txt::format("{:>8.2f} | {:#06x} | {:^10}", 3.14159, 255, name);
    string u = txt::format("[{:>{}}] [{:.{}f}]", name, 6, 3.14159, 3);  // the width, the precision given
    println("{}", s);
    println("{}", t);
    println("{}", u);
    return 0;
}
```

Output:

```text
3 left of 10
    3.14 | 0x00ff |    Ada    
[   Ada] [3.142]
```

A field of text measured in clusters, and a precision that stops on one:

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{}", txt::format("[{:>10}]", "żółć"));  // eight bytes, four clusters
    println("{}", txt::format("[{:>8}]", "日本"));  // two clusters, four wide
    println("{}", txt::format("{:.3}", "żółć"));  // stops on a cluster
    return 0;
}
```

Output:

```text
[      żółć]
[    日本]
żół
```

`{:?}`, text as a program would write it:

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{}", txt::format("{:?}", "a\tb"));
    println("{}", txt::format("{:?}", "a\"b"));
    println("{}", txt::format("{:?}", 'x'));  // a character in single quotes
    println("{}", txt::format("{:?}", "żółć"));  // a letter is shown, not escaped
    println("{}", txt::format("{:?}", "\u00a0"));  // a no-break space is not
    return 0;
}
```

Output:

```text
"a\tb"
"a\"b"
'x'
"żółć"
"\u{a0}"
```

The elements of a range take the debug form by default:

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    vector<string> words = {"a, b", "c"};
    println("{}", txt::format("{}", words));
    // the same eight characters as a list of three words
    println("{}", txt::format("{::s}", words));
    return 0;
}
```

Output:

```text
["a, b", "c"]
[a, b, c]
```

A list:

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    vector<int> v = {1, 42, 255};
    println("{}", txt::format("{}", v));
    println("{}", txt::format("{:n}", v));  // 'n' drops the brackets
    println("{}", txt::format("{::>5}", v));
    println("{}", txt::format("{::#x}", v));
    return 0;
}
```

Output:

```text
[1, 42, 255]
1, 42, 255
[    1,    42,   255]
[0x1, 0x2a, 0xff]
```

A table, a pair and a value that may not be there:

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    sorted_map<int, string> m = {{1, "one"}, {2, "two"}};
    println("{}", txt::format("{}", m));
    println("{}", txt::format("{}", sorted_set<int>{1, 3}));

    println("{}", txt::format("{}", pair<int, int>(1, 2)));
    println("{}", txt::format("{:m}", pair<int, int>(1, 2)));
    println("{}", txt::format("{}", optional<int>(42)));
    println("{}", txt::format("{}", optional<int>()));
    return 0;
}
```

Output:

```text
{1: "one", 2: "two"}
{1, 3}
(1, 2)
1: 2
42
nullopt
```

What follows a second colon, and a width over the whole:

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    vector<int> v = {1, 42, 255};
    vector<vector<int>> deep = {{1, 2}, {30}};
    println("{}", txt::format("{::>5}", v));  // the elements padded to five
    println("{}", txt::format("{:n:#06x}", v));  // no brackets, each element 0x00ff
    println("{}", txt::format("{:::>4}", deep));  // a list of lists, the numbers padded
    println("{}", txt::format("{::>6}", 42));  // a number holds nothing: the colon pads
    println("{}", txt::format("[{:>16}]", v));  // a width applies to the whole
    return 0;
}
```

Output:

```text
[    1,    42,   255]
0x0001, 0x002a, 0x00ff
[[   1,    2], [  30]]
::::42
[    [1, 42, 255]]
```

A pattern of time, with the formatters the module [time](../time/README.md#formatting-with-txt) brings:

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"
#include "sgcl/txt.h"
#include <chrono>

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    std::chrono::sys_seconds t = std::chrono::sys_days{2026y / 9 / 24} + 12h + 41min;
    vector<std::chrono::year_month_day> dates = {2026y / 1 / 2, 2026y / 3 / 4};
    println("{}", txt::format("{:%H:%M}", t));
    println("{}", txt::format("[{:>12%F}]", std::chrono::year_month_day{2026y / 9 / 24}));
    println("{}", txt::format("{::%d.%m}", dates));  // a list hands the pattern on
    println("{}", txt::format("{} {:%T}", t, 90s));
    return 0;
}
```

Output:

```text
12:41
[  2026-09-24]
[02.01, 04.03]
2026-09-24 12:41:00 00:01:30
```

An enumeration, written as the number it is:

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

enum class colour : uint8_t { red = 0, green = 7, blue = 255 };

int main() {
    println("{}", txt::format("{}", colour::blue));
    println("{}", txt::format("{:#04x}", colour::blue));
    println("{}", txt::format("[{:>6}]", colour::green));
    println("{}", txt::format("{:02x}", byte{10}));  // byte is one too
    return 0;
}
```

Output:

```text
255
0xff
[     7]
0a
```

And its names, given by a `format_value` beside it:

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

namespace cards {
    enum class suit { hearts, spades, diamonds, clubs };

    void format_value(txt::format_sink& out, suit s, const txt::format_spec& spec) {
        static const char* names[] = {"hearts", "spades", "diamonds", "clubs"};
        txt::write_padded(out, names[int(s)], spec);
    }
}

int main() {
    println("{}", txt::format("[{:>8}]", cards::suit::spades));
    return 0;
}
```

Output:

```text
[  spades]
```

A type of one's own:

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"
#include <cstdio>

using namespace sgcl;

namespace geometry {
    struct point { int x, y; };

    void format_value(txt::format_sink& out, const point& p, const txt::format_spec& spec) {
        char room[32];
        int n = std::snprintf(room, sizeof room, "(%d, %d)", p.x, p.y);
        txt::write_padded(out, {room, size_t(n)}, spec);
    }
}

int main() {
    println("{}", txt::format("{}", geometry::point{1, 2}));
    println("{}", txt::format("[{:>10}]", geometry::point{3, 4}));
    return 0;
}
```

Output:

```text
(1, 2)
[    (3, 4)]
```

A pattern out of a catalogue of translations, and the built-in one where it does not fit:

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    map<string, string> catalogue = {{"items-left", "pozostało: {}"},
                                     {"broken", "pozostało: {1}"}};
    int n = 3;
    for (auto key : {"items-left", "broken"}) {
        string entry = catalogue.at(key);
        // the built-in pattern if it does not fit
        string s = txt::format(txt::runtime(entry), n).value_or(txt::format("{} left", n));
        println("{}", s);
    }
    return 0;
}
```

Output:

```text
pozostało: 3
3 left
```

## See also

- [format_to](format_to.md): the same into memory the caller lends
- [runtime](runtime.md), [fits](fits.md): a pattern read where the program runs, and whether it fits
- [format_pattern](format_pattern.md), [runtime_pattern](runtime_pattern.md): the two kinds of pattern
- [formatter](formatter.md), [format_spec](format_spec.md), [format_sink](format_sink.md),
  [write_padded](write_padded.md): what a type of one's own is written with
- [stencil](stencil.md): a template whose fields are written by this header
- [columns](columns.md): the cells of a terminal
- [print](../io/print.md): writes through `format`
- [time](../time/README.md#formatting-with-txt): the formatters of time
