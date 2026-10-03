[sgcl](../README.md) › [txt](README.md)

# Benchmarks: txt

The setup, the machine and how the timers are read are described with
[the benchmarks of the engine](../../garbage_collector/benchmarks.md). The cases of the module are in
`benchmarks/txt/` (`bench_format`, `bench_stencil`, `bench_regex`, `bench_identifier` and the others), Go's in
`benchmarks/go`, run by `benchmarks/compare.sh`. The times are nanoseconds unless a row says otherwise.

## The tables

A question asked about one character in the middle of a loop is answered below U+10000 by a two-stage table — an
index that says which block of values a code point falls in, the blocks themselves, and every block that is the same
as another stored once — which is two reads and no branch. It replaced a binary search over sorted ranges that was a
dozen steps no processor can predict:

| Measure | Sorted ranges | Two-stage table |
|---|---|---|
| a property of a code point, over Latin text | 21.8 ns | 0.54 ns |
| `line_breaks` | 82 ns a byte | 20 ns a byte |
| `words` | 26 ns a byte | 8 ns a byte |
| a collation comparison | 443 ns | 51 ns |

A set of code points is the same thing with a bit to the character. Above U+10000 the sorted ranges stay, since an
index over a million code points would cost more than the search it saves and hardly any text goes there; the
generator picks the block size and the width of the index per table, whichever comes out smallest, and three of the
ten tables came out smaller than the ranges they replaced. The whole change cost 50.1 KB, a tenth of the module.

## Runs of ASCII

Most text is a run of ASCII letters, and for a run of them several of the rules have one answer: two ASCII characters
are always separate graphemes unless they are a carriage return and a line feed (GB3), a run of letters is one word
(WB5) and holds together on a line (LB28), and the case of a Latin letter is one bit with no mapping that changes a
length, depends on its neighbours or spells an i the Turkish way. So the run is walked in one step rather than one
step a character, found eight bytes at a time by a mask over a word — no instruction the compiler would not write by
itself.

| Function, over ASCII | Before | After |
|---|---|---|
| `to_upper_full` | 11.1 ns a byte | 0.11 ns a byte |
| `fold_case` | 9.7 ns a byte | 0.11 ns a byte |
| the grapheme rule | 6.5 ns a byte | 1.8 ns a byte |
| `line_breaks` | 22 ns a byte | 12 ns a byte |
| `words` | 10.3 ns a byte | 7.3 ns a byte |

What is left in the last three is not the rules but the piece of text each boundary hands back: a
[slice](../core/slice.md) holds the object its characters live in, and making one costs about 3.9 ns.

## Percent encoding

| Case | SGCL |
|---|---|
| `percent::encode` of a path of 23 bytes | 121 ns |
| `percent::decode` of it, back | 81 ns |

Over a stream of different values. Nothing is allocated per character: the output is laid out into one buffer and the
string made once from it.

## Properties

The sorted ranges of a property table — what the decimal digits still are, and what every table is above U+10000 —
are split at the end of the Basic Multilingual Plane: a table of 16-bit bounds below U+10000 and one of wide fields
above it. Measured over a paragraph, when the general category was such a table, that was five to ten per cent
faster than one wide table, and a third smaller.


## Normalization

A text that is already in the form comes back as the same object from [normalize](normalize.md), nothing allocated
and nothing copied, and the quick check properties settle most texts without a decomposition. What
[without_marks](without_marks.md) costs is measured with the identifiers, below: a text with nothing to take off comes
back as the same object, 6.1 ns over ASCII, against 197 ns over a Latin name with accents in it
(`bench_identifier marks ascii`).

## Bidirectional text

[mirrored](mirrored.md) with the levels in hand is rule L4 and nothing else; the form that works the paragraph out
runs the whole algorithm first. On a mixed line of fifty-one bytes:

| Case | Time per call |
|---|---|
| `mirrored(text, levels)`, the levels in hand | 199 |
| `mirrored(text)`, the paragraph worked out | 1464 |

## Identifiers

The benchmark is in the tree, so the next change has something to measure against rather than starting over:

```sh
cmake --build <build> --target bench_identifier
<build>/benchmarks/bench_identifier casefold latin     # and: upper, compat, folded
<build>/benchmarks/bench_identifier skeleton ascii
```

It runs over a **stream of a thousand different names**, never the same one twice, because a repeated name hides the
searching. `latin` is Polish, French, German and Czech, two of the six with a capital in them; `upper` is the same
letters all in capitals, so that every name changes and no quick check saves it; `compat` is fullwidth letters, a
ligature, circled digits, Greek, Cyrillic and Japanese; `folded` is `latin` already put through
[nfkc_casefold](nfkc_casefold.md).

| Function | `ascii` | `latin` | `upper` | `compat` | `folded` |
|---|---|---|---|---|---|
| [is_identifier](is_identifier.md) | 13.4 | 17.3 | | 31.1 | 17.2 |
| [is_identifier](is_identifier.md), the profile | 14.4 | 18.1 | | 31.2 | 17.9 |
| [nfkc_casefold](nfkc_casefold.md) | 14.7 | 101 | 296 | 196 | 19.2 |
| [fold_case](fold_case.md), for scale | 15.3 | 105 | 104 | 166 | 105 |
| [is_nfkc_casefolded](is_nfkc_casefolded.md) | 11.2 | 11.1 | | 14.8 | 14.6 |
| [skeleton](skeleton.md) | 212 | 220 | 219 | 242 | 231 |
| [is_confusable](is_confusable.md) | 389 | 458 | 452 | 492 | 460 |
| [is_allowed_identifier](is_allowed_identifier.md) | 8.2 | 10.3 | | 14.3 | 10.2 |
| [restriction_level_of](restriction_level_of.md) | 9.8 | 29.6 | | 35.7 | 29.8 |
| [is_single_script](is_single_script.md) | 19.8 | 19.3 | | 30.1 | 19.4 |
| [without_marks](without_marks.md) | 6.1 | 197 | 198 | 229 | 200 |

The times are nanoseconds per call.

- **The `fold_case` row is the one to read against.** Over `latin` the two are level, 101 against 105, because the
  quick check settles two names in three before any folding happens; over `folded`, where `fold_case` still pays its
  full 105, `nfkc_casefold` costs 19. Over `upper`, where every name changes and the quick check saves nothing, the
  compatibility decomposition and the composition cost about 190 ns on top of the plain fold. That is the shape of
  the trade: the form is dearer than a folding only when it has work to do, and the names a registry sees mostly do
  not. The two properties that answer whether a name is in the form already cost 14.4 ns over a name where the fold
  costs 100.
- **The scripts of a text are counted into eight words of stack** rather than into a container
  ([is_single_script](is_single_script.md)). The container it replaced was the whole cost of the question:
  `is_single_script` used to take longer over an ASCII name (27.0 ns) than over a Polish one (26.0), which is not the
  shape of a walk that decodes a code point and reads a table. It is 19.8 and 19.3 now, and `restriction_level_of`
  over a Polish name fell from 38.0 to 29.6.

## Domain names

| Case | Time per call |
|---|---|
| [to_ascii](idna/to_ascii.md) of `www0.example0.com`, a name already as the DNS carries it | 60 (the general road: 370) |
| [to_ascii](idna/to_ascii.md) of two labels of German, which need the work | about 550 |
| [to_unicode](idna/to_unicode.md) of an encoded name | about the same, 550 |
| [to_ascii](idna/to_ascii.md) of `WWW0.EXAMPLE0.COM`, typed in capitals | 416 (485 before the mapping was computed) |
| [to_ascii](idna/to_ascii.md) of a name of capitals with accents in it | 708 (666 before, 6 per cent more) |

- A name that is already what the DNS carries, lower case letters, digits, hyphens and stops and no `xn--` label, is
  checked over its bytes and comes back as the object it went in as, nothing allocated. That is most of the traffic
  there is. A name that does need the work is settled by the quick check properties of [normalize](normalize.md)
  without a decomposition in the usual case.
- **The mapping is computed, not tabled** ([idna](idna.md)), which saved 58.6 KB and is not slower for it. A name
  typed in capitals got **faster**, because the only ASCII the standard maps is `A`–`Z`, each to the letter 32 above
  it, which is a line rather than a search. A name of capitals with accents in it costs 6 per cent more, and every
  other path (a lowercase name, an encoded one, the fast path, punycode) is unchanged.
- **The bytes of the failing label** ([failure](idna-failure.md)) are found by a second pass over the name, made only
  once the name has already failed. Writing the positions down on the way through every name cost 150 ns on every
  name that was fine, which is the wrong trade for an answer wanted once in some thousands; done this way it costs
  about 5 ns on the fast path and nothing measurable on the slow one.

## Regex

`benchmarks/txt/regex.cpp` is the program these came out of — `bench_regex sgcl <op>` and `bench_regex std <op>` —
so the next change to [regex.h](regex.md) has something to measure against.

A pattern that is **not** in a text of four thousand bytes, so the whole of it is walked, against `std::regex`
(libc++, ECMAScript, `optimize`) on the same data:

| Op | Pattern | SGCL, ns a byte | `std::regex`, ns a byte |
|---|---|---|---|
| `date` | `(\d{4})-(\d{2})-(\d{2})` | 0.039 | 110.1 |
| `address` | `(\w+)@(\w+)\.(com\|pl)` | 0.039 | 358.1 |
| `upper` | `[QWX]+` | 0.43 | 76.7 |
| `class` | `[0-9]+` | 0.44 | 85.3 |
| `literal` | `zyzykot` | 0.52 | 41.3 |
| `boundary` | `\bzyzykot\b` | 0.52 | 46.6 |
| `dotstar` | `zyz.*kot` | 1.11 | 71.0 |
| `alt` | `kot\|pies\|ryba` | 5.19 | 126.7 |

The spread in the first column is **what the search may skip**. A run of bytes every match must contain is read off
the pattern where it is compiled, and where there is one the text is scanned for it — by the same
Boyer–Moore–Horspool table [searcher](searcher.md) uses, or by a `memchr` when the run is a single byte — instead of
stepping the machine at every position. `(\w+)@(\w+)\.(com|pl)` begins with `\w`, which nearly every byte is, so
every position used to be a candidate and it cost 24.6 ns a byte; it must contain an `@`, and looking for that costs
0.039. Where the run is also the *beginning* of every match, as in `zyzykot`, the search jumps straight to the next
place it stands. Where the pattern gives no such run at all — an alternation, a class — the fall-back is the set of
bytes a match may begin with, one test a byte, which is the 0.43 those rows show. The second column has no spread
because a backtracking engine pays for the pattern at every position whatever it starts with.

The rest, over the same four kilobytes:

| Op | SGCL | `std::regex` |
|---|---|---|
| `hit` — a word that is there, found a third of the way in | 354 ns | 6990 ns |
| `line` — the address over one line of sixty bytes | 60 ns | 22280 ns |
| `find` — the address with its three groups | 190 ns | 1427828 ns |
| `all` — every one of the 842 words | 98 µs, 117 ns a match | 434 µs, 516 ns a match |
| `replace` — each of them wrapped | 105 µs | 456 µs |
| `split` — on every word | 93 µs | not the same question |
| `build` — compiling the date pattern | 3196 ns | 670 ns |

Building a `regex` is the one row that goes the other way: this engine spells a `{n,m}` out into instructions where
a backtracking one keeps a counter ([program_size](regex/program_size.md)), and that is the price of not having
one.

And the case the whole header is about, `(a+)+b` over *n* characters with no `b` in them:

| `n` | SGCL | `std::regex` |
|---|---|---|
| 28 | 0.0009 ms | 1.6 ms, and then it gives up |
| 1 000 | 0.047 ms | — |
| 100 000 | 2.9 ms | — |
| 1 000 000 | 28 ms | — |

libc++ does not hang on it: it counts its steps and throws `regex_error` with "the complexity of an attempted match
exceeded a pre-set level" — at sixteen characters already. Which is the point. The pattern does not work there at
all, and here it is linear.

## Search

[find_fold](find_fold.md) and [find_normalized](find_normalized.md) map both sides on every call, so a loop over
the occurrences maps the text again for each of them and is quadratic; the ranges
([fold_matches, normalized_matches](fold_matches.md)) map it once:

| A text of 64 KB, every occurrence found | A call at a time | A range |
|---|---|---|
| `find_fold` / `fold_matches`, 878 of them | 337 ms | **0.41 ms** |
| `find_normalized` / `normalized_matches`, 1747 | 648 ms | **0.41 ms** |

At 256 KB, where the shape shows itself properly, it is 4.65 s against 1.51 ms and 9.46 s against 1.54 ms. Four
times the text costs sixteen times as much one way and four times the other. Over sixty-four kilobytes the mapping
of a text is some three hundred microseconds and a search through the mapped text some twenty
([folded_text](folded_text.md)).

## Collation

A comparison costs about 27 ns for two short words on an ordinary machine in the root order and 51 in a language's,
and a sort key about 64 ns into a buffer of the caller's; sorting a thousand words through the comparator is 380 ns
a word. Measured separately for [key_to](collator/key_to.md): sorting a thousand words through the comparator costs
about 300 ns a word, and through keys written into a buffer on the stack about 165.

A setting that was not asked for costs nothing at all: the collator settles once, where it is built, whether
anything it was given changes what an element weighs or how a weight is read, and where nothing does, the
comparison, the key and the stream that feeds them are the code they were before any setting existed. Measured on
two words that differ in their fourth letter, a comparison is 44.7 ns with the settings in the header and 44.5
without them, a Polish one 182 against 183, and a key 43.2 against 43.4.

The elements are produced one at a time and the first level of both texts is walked side by side, so a comparison
stops at the first letter that differs, which in a sorted list is nearly always the first one — neither text is
taken apart any further than that, and a short word needs no allocation at all. Nor is a text taken apart at all
where it need not be. The algorithm is defined on the decomposed form, but a letter with no mark behind it cannot
be reordered and cannot join anything, so its weights are read where it stands — which is what the table's entries
for the composed letters are for, and the only thing they are for, since a text that is decomposed first never
reaches them. That is the condition UAX #15 calls FCD. A language's own letters are read the same way, the rules
being closed canonically so that a composed ż meets the rule written on the decomposed one; a rule over several
letters is left to the slower road, where the marks are in canonical order and one may stand further off than it is
written.

Weighing the text is the whole cost of a search by collation, so [collator::find](collator/find.md) in a loop
weighs the text once for every occurrence and is quadratic. Over 64 KB of text with 1902 occurrences in it, one pass
costs **1.19 s** that way and **1.10 ms** through [collated_matches](collated_matches.md), of which 0.63 ms is the
weighing — a thousandfold, and the same lesson the folded search learned.

[collated_text::ends_with](collated_text/ends_with.md) finds the one element a match at the end can begin at by a
walk back over the pattern's length; it used to try every position from the last to the first, and over a hundred
kilobytes that was 658 µs against the 6 ns [starts_with](collated_text/starts_with.md) costs for the same question
at the other end.

## Format

The benchmark is in the tree ([benchmarks/txt/format.cpp](../../../benchmarks/txt/format.cpp)), so the next change
has something to measure against rather than starting over:

```sh
cmake --build <build> --target bench_format
<build>/benchmarks/bench_format sgcl padded      # and: std padded
<build>/benchmarks/bench_format sgcl list
```

Both sides write into a buffer the caller lends, so nothing is allocated and the writing is what is measured —
`std::format_to_n` against `txt::format_to`, which is the same contract. Over a **stream of a thousand different
values**, not one repeated, because half of what writing a number costs is that its length cannot be predicted:

| Operation | `txt::format` | `std::format` |
|---|---|---|
| `simple` `"{} left"` | **12.8 ns** | 27.9 |
| `padded` `"{:>12}"` | **14.9** | 40.6 |
| `centred` `"{:*^40}"` | **15.4** | 44.4 |
| `mixed` `"{:>8.3f} {:#x}"` | **50.6** | 87.9 |
| `whole` `"{}"` of a whole `double` | **15.7** | 59.7 |
| `text` `"{}"` of a short text | **8.7** | 20.0 |
| `five` five fields in one pattern | **66.6** | 115.0 |
| `literal` forty-five characters, no field | **6.5** | 86.0 |
| `alloc` into a string that is handed back | **22.0** | 34.2 |
| `runtime` a pattern read where it runs | **16.1** | 28.9 |

The `literal` row is the only one that flatters this side: `format_to_n` of a literal costs the standard 87.8 ns where
the unbounded `format_to` costs 48.5, and over a number the two are the same. Everywhere else the bound is free.

Eight rows of this table and three of the next were measured again on 3 October 2026, both sides, after the
library's copies longer than 32 bytes went out of line and the sink's copy was inlined again; the rows left as they
were measured within 5 per cent of their numbers.

And the ones the standard cannot be asked, C++23 being where it grew them:

| Operation | `txt::format` |
|---|---|
| `list` a list of five numbers | 48.9 ns, ten a number |
| `list100` a hundred of them | 824, eight a number |
| `elements` `"{::>5}"` | 74.5 |
| `listwidth` `"{:>40}"` over the list | 75.0 — the one road that goes through room of its own |
| `words` a list of text, escaped and quoted | 46.1 |
| `pair` | 26.4 |

Benchmarks are built at `-O2` whatever the configuration, which is what the numbers above are; the ones below came
from a harness at `-O3` and are not to be read against these.

Measured against the standard's on the same machine:

| Pattern | `txt::format` | `std::format` |
|---|---|---|
| `"{} left"` with a number | **23.2 ns** | 33.1 |
| the same through `format_to`, nothing allocated | **14.0** | — |
| `"{:>8.3f} {:#x}"` | **59.5** | 104.7 |

A list of five numbers is 50.1 ns through `format_to`, which is ten a number and less than the 75.4 the same five
numbers cost written out as five fields of a pattern — a pattern of more than four fields keeps no steps and is read
where it runs, and a range has one field and one step. A list of a hundred is 791 ns, or 7.9 a number. A width over a
list is the one road that goes through room of its own before it is padded: the same five numbers in `{:>40}` are
75.0 ns, against the 112.6 they cost while the body was written twice. A pair is 21.8 and an `optional` that holds a
number costs the number.

The floor of the work — `to_chars` and a copy — is 12.7 ns, of which a `string` of the library is 9.8; the standard
does not pay that at this length, because it keeps a short string inside itself. Those three are one value repeated,
which flatters the standard: over a stream of different ones the distance is wider, as the first table shows.

### The pattern is not read where the program runs

The same pass that checks the pattern writes down its steps — the run of literal text, the value that follows it,
the specification already made out — so a call walks over as many steps as the pattern has fields and not over its
characters. A pattern keeps four of them, which covers a message; one with more, or with a number too large for the
narrow fields of a step, keeps none and is read the old way, which is correct and only a little slower.

The steps are kept small on purpose. The pattern is a temporary the conversion makes at the call site, so what it
costs to build is the bytes it takes: at 280 of them that was 3.5 ns of every call, and over a literal with nothing to
substitute — where there is nothing else to do — 8.9. Packed into fourteen bytes a step, the whole is 80, and a
literal of forty-five characters went from 33.8 ns when it was read a byte at a time, through 9.5 when the reading was
made wide, to **6.9** now that it is not read at all.

### Numbers

Decimal is written here and not by the standard, and without a branch on how many digits a number has: eight digits
come out at once — as four dividends by a hundred and then by ten, in sixteen-bit lanes — and how many of them matter
is one lookup on the bit width, after which they are slid up inside the register. It costs 1.10 ns for a 32-bit value
against the standard's 1.70, and 2.96 against 4.35 for a 64-bit one, but the number that matters is over a stream of
different values rather than one repeated, because half of what the standard pays there is mispredicting the length:

| Over 1024 different values | Before | After |
|---|---|---|
| `format_to("{}", int)` | 19.0 ns | **15.3** |
| `format_to("{}", int64_t)` | 20.2 | **17.0** |
| `format_to("{} of {}", int, int64_t)` | 45.0 | **31.9** |

Two fields gain thirteen nanoseconds, more than twice what one does, because two streams of values mispredict worse
than one.

**A whole number written as `{}` does not go through the standard either.** Below 2^53 for a `double` and 2^24 for a
`float` — where the significand runs out in each — the interval of values that read back as it is narrower than one,
so its digits without their trailing zeros *are* the shortest representation — no algorithm needed — and when the
last digit is not a zero the plain shape always wins, since the exponential one spends five more characters on the
point and the exponent. The standard is at its slowest on exactly these, its digit-removal loop taking the trailing
digits off one at a time:

| Over 1024 values | Before | After |
|---|---|---|
| whole numbers (`42.0`, `1000.0`) | 26.9 ns | **13.4** |
| powers of ten | 43.9 | **24.1** |
| a third of them whole, the rest not | 40.9 | **32.7** |

For a `float` the saving is larger still, because the standard's `to_chars` for one is not specialized the way it is
for a `double`: a whole `float` costs it 24.7 ns of conversion where a whole `double` costs 12.6, which is backwards,
and this road does either in 1.8.

Everything else — a fraction, a value at or above 2^53, any form with a type or a precision — is the standard's
`to_chars`, and stays that way: the algorithms that find the shortest representation in general (Ryu, Schubfach) are
no faster than what it already does.

The writing is kept apart from the padding for the same reason. A value that asks for no width — which is most of
them — goes out as itself, and only a value in a field wider than itself takes the road that pads it. Written as one
function, the compiler made a frame of 112 bytes and spilled six pairs of registers before anything happened, because
the copying ladder appears in it four times over, and every value paid that whether or not it had a width: writing
one number went from 9.8 ns to 7.1.

### A width over a value made of values

The body of such a field is written once, into room the thread keeps, and padded where it lies. It used to be written
twice, once to be counted and once to be kept, and over a value made of values that stopped being a constant — every
level doubled the level below it. After that it went into 256 bytes of the call's own stack, and a level that did not
fit was written again with every level under it, so a nest whose every level passed 256 bytes still doubled: sixteen
levels of three hundred bytes took 443 ms. The levels of one field share one room now and grow it for one another, and
the same nest is 0.17 ms; a list of 400 numbers in `{:>2000}` is 3.7 µs through `format_to` against 6.7.

## Stencil

Measured on this machine at `-O2`, over a stream of different values
([benchmarks/txt/stencil.cpp](../../../benchmarks/txt/stencil.cpp)), in nanoseconds per render:

| Template | Render | Parse |
|---|---|---|
| a page of literal text, no action | 27.7 | 45.2 |
| one value in a line of text | 52.1 | 135.4 |
| five values | 180.6 | 416.5 |
| one value with a specification | 74.6 | 136.3 |
| an `if` with an `else` | 56.3 | 240.0 |
| a path four names long | 71.2 | 234.3 |
| a name that is not there | 26.4 | 140.9 |
| two functions of the pipeline | 144.7 | 286.7 |
| ten rows of two fields, an `if` in each | 545.0 | 529.7 |
| a hundred such rows | 5296.6 | — |
| the same ten rows into a buffer one lends | 524.4 | — |

Reading a source is two to four times writing from it, which is the argument for the compiled form. Of a single
field's 52.1 ns, about 26 is the [string](../core/string.md) handed back and most of the rest is hashing the name in
the mapping; `render_to` into a buffer one lends allocates nothing at all.

A render allocates for its own bookkeeping **not at all** while the blocks are shallower than four, which is nearly
every template: the parser counts how deep they go and the walk keeps its frames, its dots and its owned values on
the stack. Past that the three move to vectors, in a call of their own, so that the page with no block in it does not
carry them. A walk over a list points straight at the elements and copies none of them.

### A page longer than the kilobyte on the stack

A page that fits that kilobyte is written once. A longer one **grows the room** rather than being written twice: the
walk asks after each step that writes whether that step ran off the end, and when it did it takes twice as much room,
carries over what stood before the step and writes that one step again. Never the page — a page of a hundred
kilobytes is one walk and seven doublings, not two walks.

The asking is what a page that fits pays for a page that does not, and it has to be made to cost nothing. Two things
do that, and both were arrived at by reading the disassembly rather than by guessing: the walk reads the capacity
**once** into a local of its own, because a field of the sink would have to be read back after every call a step
makes, and the test is marked `[[unlikely]]`, because a test whose other side calls is otherwise laid out with the
common case **jumping over** the call. The second was the larger by far — the compare itself is two instructions,
while the branch at every literal run of every row came to fourteen per cent of a page of ten rows
([growing_sink](growing_sink.md)).

This is where a page parts company with a [format](format.md) pattern, which is written twice and stays that way: a
pattern's second pass is `to_chars` and a copy over a handful of fields and costs less than the copying a doubling
buffer does, where a page's second pass is every branch tested again, every row of every loop walked again and every
`upper` and `escape_html` run again. The benchmark keeps both roads so the question can be asked again —
`bench_stencil sgcl` against `bench_stencil twopass`, the second being exactly what `render` did before. Best of
three alternated runs, nanoseconds a page:

| A page | Grown | Written twice |
|---|---|---|
| 244 characters, fits the stack room | 517.9 | 514.4 |
| 1137 characters | 2354.8 | 4559.4 |
| 2359 characters (the hundred rows above) | 5096.1 | 9890.2 |
| 9879 characters | 19946.8 | 40592.3 |
| 98709 characters | 193570.5 | 401917.1 |

Half, at every size, because the second walk was the whole of the cost and the copying is next to none of it: the
seven doublings on the way to a hundred kilobytes carry 127 KB of characters, and one grown walk comes to 193.6 µs
against the 201.0 that half of two walks is. The page that fits pays nothing — the first row is the same number twice,
and against the commit before the room could grow at all, two binaries alternated in one window and best of three,
the whole short end is level as well: `empty` 28.9 against 28.7, `simple` 51.2 against 51.9, `five` 178.5 against
178.1, `branch` 57.1 against 55.9, `rows` 521.8 against 526.2, `render_to` 502.5 against 506.8. These rows were
measured together and are not comparable to the cent with the table above them, which is from an earlier round.

The growth takes a plain array of characters (`new char[n]`) and not a `std::string`, which would write n zeros into
room that is about to be written over — though that is a reason and not a measurement: the two were built and
alternated, and over a page of a hundred kilobytes, where the doublings zero 260 KB between them, they read 236.6 µs
and 235.7. The zeroing is invisible against the walk.

### Against Go's text/template

The same eleven shapes and the same stream of values on both sides
([benchmarks/go/stencil](../../../benchmarks/go/stencil/main.go)), nanoseconds per render, each side writing a fresh
string:

| Template | SGCL | Go | Ratio |
|---|---|---|---|
| a page of literal text, no action | 29.0 | 63.7 | 2.1× |
| one value in a line of text | 53.1 | 198.4 | 3.7× |
| five values | 178.4 | 770.7 | 4.3× |
| one value with a specification | 73.6 | 572.2 | 7.7× |
| an `if` with an `else` | 57.2 | 144.8 | 2.5× |
| two functions of the pipeline | 153.1 | 585.2 | 3.8× |
| a path four names long | 72.3 | 351.1 | 4.8× |
| a name that is not there | 26.7 | 135.8 | 5.0× |
| ten rows of two fields, an `if` in each | 517.4 | 2644.5 | 5.1× |
| a hundred such rows | 5010.7 | 23809.8 | 4.7× |
| ten rows into a buffer the caller keeps | 502.9 | 2654.6 | 5.3× |

And reading a source, which each side does once and then keeps:

| Template | SGCL | Go | Ratio |
|---|---|---|---|
| one value in a line of text | 127.5 | 1516.7 | 11.9× |
| five values | 396.8 | 3214.1 | 8.1× |
| an `if` with an `else` | 234.6 | 2115.9 | 9.0× |
| ten rows of two fields | 504.5 | 3620.2 | 7.2× |

**Where the difference comes from, and where it does not.** Not from allocation: keeping one buffer instead of
building a string helps Go nothing (2654.6 against 2644.5) and helps this nothing either (502.9 against 517.4), so
neither side is bound by the writing. It comes from how a name is looked up. Go walks the data with reflection —
`map[string]any` at every step, an `interface{}` unwrapped, a `reflect.Value` made — where a [value](value.md) here is
a variant of thirty-two bytes and a mapping is an [ordered_map](../core/ordered_map.md) of them. That is also why the
widest gap is the specification: `{{ printf "%8.2f" .d }}` is a function called through reflection over there, and
`{{ d:>8.2f }}` is [format](format.md)'s own writer here, which is the reason this module has a template at all.

Both columns above were measured in one sitting after the room was made to grow, the two binaries alternated under an
empty environment, and they are left as they were rather than half refreshed. One row has moved since: a stage of a
pipeline that takes no arguments no longer builds room for eight of them, so the two functions read about 145 ns here
and not 153.1, which is 4.0× rather than 3.8×. It wants a fresh sitting of both columns and not a number dropped into
one of them. The hundred rows were the row that flattered this side least, 2.4×, while a page over a kilobyte was
still written twice here; it reads 4.7× now, which is where the rest of the table already was. The row that flatters
this side most is reading a source, where the shapes are small and Go's parser builds a tree of nodes on the heap.

Two shapes are not the same question on both sides and are marked as such rather than quietly counted: a name the
data does not carry writes nothing here and `<no value>` in Go, and the specification is a format specification here
and a `printf` call there. The rest are the same source, the same data and the same output — the oracle
([tools/stencil_oracle.go](../../../tools/stencil_oracle.go)) is what keeps them so.
