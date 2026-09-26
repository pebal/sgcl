# txt::format

```cpp
#include "sgcl/txt/format.h"
```

Text built from a pattern and the values that go in it. The pattern is the one of [`std::format`](https://en.cppreference.com/w/cpp/utility/format/spec) — a pair of braces for a value, a colon and a specification after it for how to write it — and it is **read where the program is compiled**, not where it runs: a pattern that does not fit the values it is given is an error of the compiler and not a surprise at the customer's.

```cpp
using namespace sgcl;

string s = txt::format("{} left of {}", 3, total);
string t = txt::format("{:>8.2f} | {:#06x} | {:^10}", 3.14159, 255, name);
```

## The names

```cpp
template<class... A> string format(pattern, const A&... args);
template<class... A> size_t format_to(const slice<char>& buffer, pattern, const A&... args);

struct format_spec { char fill; char align; char sign; bool alternate; bool zero;
                     unsigned width; int precision; char type; };
class format_sink { void put(char); void put(std::string_view); void fill(char, size_t);
                    size_t size() const; };
class growing_sink { format_sink& out(); size_t capacity() const;
                     size_t take_room(size_t want, size_t mark);
                     size_t size() const; string text() const; };
template<class T, class = void> struct formatter;
```

`format` returns a [`string`](../core/string.md). `format_to` writes into memory the caller lends and returns **what the whole text takes**, whether or not it fitted — so a buffer can be sized from a first call with an empty one, and a caller who knows its text is short pays no allocation at all:

```cpp
char room[64];
size_t needed = txt::format_to(slice<char>(room, room + sizeof room), "{} left", n);
if (needed > sizeof room) { /* the text was cut; ask for that much */ }
```

`growing_sink` is the other half of that, for whoever writes a long text in steps rather than a message in one: it starts over room the caller lends, and when a step runs off the end `take_room` takes twice as much, carries over what stood before the step and hands back the new capacity, so that **one step** can be written again. Nothing here uses it — `format` writes a text that did not fit a second time instead, and that is measured and stays that way, because a pattern's second pass is `to_chars` and `memcpy`. [`stencil.h`](stencil.md) is what it is for, where a second pass is every branch, every row and every `upper()` walked over again.

The capacity is asked for **once** and kept in a variable of the caller's, and the test is marked unlikely. Both matter and both were measured: a field of the sink has to be read back after every call a step makes, and a test whose other side calls is otherwise laid out with the common case jumping over the call, which cost fourteen per cent of a page that fitted and needed no growing at all.

```cpp
char room[1024];
txt::growing_sink page(room, sizeof room);
size_t cap = page.capacity();
for (auto& step : steps) {
    size_t mark = page.size();
    write(page.out(), step);
    if (page.size() > cap) [[unlikely]] {
        cap = page.take_room(page.size(), mark);
        write(page.out(), step);        // there is room for it now
    }
}
return page.text();
```

## Why this is in txt and not in core

Two reasons, and the second is the real one.

An unqualified `format(...)` next to a [`string`](../core/string.md) of ours **is not ours**. `basic_string` names `std::char_traits` among its arguments, so `std` is an associated namespace of it and the call is found there — and it does not announce a clash, it simply wins, and fails later on a `std::string` that will not convert. Written as `txt::format`, nothing is ambiguous and `format` left bare still means the standard's.

And a field of text cannot be measured without the tables that live here. **The width and the precision count columns, not bytes** — the same thing [`columns`](properties.md) counts, which is what the standard means by its estimated field width:

```cpp
txt::format("[{:>10}]", "żółć");    // [      żółć]   eight bytes, four columns
txt::format("[{:>8}]",  "日本");     // [    日本]     two characters, four columns
txt::format("{:.3}",    "żółć");    // żół            stops on a code point
```

Cut by its bytes, the last of those would leave a lead byte with nothing behind it, which is not text any more. A combining mark takes no column of its own, and an East Asian wide character takes two.

## What a specification may say

Everything `std::format` specifies for the types below, in the same order — `[[fill]align][sign][#][0][width][.precision][type]`. The output is checked against the standard's own implementation over 449 combinations of specification and value, and agrees on all of them.

| | takes |
|---|---|
| integers | `d` `b` `B` `o` `x` `X` `c`, the signs `+` `-` and space, `#`, `0`, a width |
| floating point | `f` `F` `e` `E` `g` `G` `a` `A`, a precision (six when a form is named and none is given), the signs, `#`, `0`, a width |
| `bool` | `s` (the default: `true`/`false`) and the integer forms |
| `char`, `char32_t` | `c` (the default), `?`, the integer forms; a code point is written as UTF-8 |
| text (`string`, `slice<const char>`, `const char*`, `std::string_view`) | `s`, `?`, a width and a precision **in columns** |
| enumerations | the integer forms over the underlying type; **not** `c` and **not** `s` |
| pointers | `p` |
| ranges, tables, pairs, tuples | `n` (no brackets), a width; and a second colon for the elements |
| `optional` | whatever the value it holds takes |
| [`duration`](../core/duration.md) | `s` (the default: `1h30m0.5s`, as `to_string` writes it), a width |

`{{` and `}}` stand for a brace. A field may name its value by number — `{1} {0}` — or leave it out, and then the values are taken in order.

**A width may go up to 65535**, which is what a step of a compiled pattern holds it in, and past that the pattern is refused: an error of the compiler on the one road, `nullopt` on the other. A precision may go up to 32767, and the number of a field may not be bigger than any call could have values for. None of those is a field anybody writes; the bounds are there so that sixteen characters of a pattern read from a file cannot ask for four gigabytes of padding.

## `{:?}` — text as a program would write it

`{}` writes text to be read. `{:?}` writes it to be **told apart**: in quotes, with what a terminal cannot show written as an escape.

```cpp
txt::format("{:?}", "a\tb");          // "a\tb"
txt::format("{:?}", "a\"b");          // "a\"b"
txt::format("{:?}", 'x');             // 'x'          — a character in single quotes
txt::format("{:?}", "żółć");          // "żółć"       — a letter is shown, not escaped
txt::format("{:?}", "\u00a0");    // "\u{a0}"     — a no-break space is not
```

`\t`, `\n`, `\r`, `\\` and the quote that surrounds it have escapes of their own; everything else that cannot be shown is `\u{...}`, in lower-case hexadecimal with no leading zeros. Which code points those are is the standard's list, and the categories are the ones [`properties`](properties.md) already answers: the controls (`Cc`), the format characters (`Cf`), the surrogates (`Cs`), the private use area (`Co`), the unassigned (`Cn`), the line and paragraph separators (`Zl`, `Zp`), and every space separator (`Zs`) **but the space itself**. A byte that is part of no sequence at all is not a character, so it goes out as `\x{ff}` — the byte it was, not the `U+FFFD` it would decode to.

A width and a precision are over what comes out, escapes and quotes included, and the field is still measured in columns.

### The elements of a range take this form by default

This is the point of it, and it is the one change here that alters what an already-written program prints:

```cpp
vector<string> words = /* "a, b" and "c" */;
txt::format("{}", words);             // ["a, b", "c"]
txt::format("{::s}", words);          // [a, b, c]        — the same eight characters as
                                      //                    a list of three words
```

Without the quotes those two are indistinguishable, which is why C++23 does the same (it is `set_debug_format` over there). It applies to the elements of a range, to the members of a pair or tuple, and to what an `optional` holds — so `optional<string>` holding the word `nullopt` is `"nullopt"` and an empty one is `nullopt`. Only text and characters have a debug form, so a list of numbers is untouched, and naming any type at all in the element specification — `{::s}` — takes it back.

## Values made of other values

A list goes in brackets, a table in braces, a pair in parentheses, and a value that may not be there is either itself or the word `nullopt`. The shapes are the ones C++23 settled on, because they are the ones people already read:

```cpp
using namespace sgcl;

vector<int> v = {1, 42, 255};
txt::format("{}", v);                          // [1, 42, 255]
txt::format("{:n}", v);                        // 1, 42, 255        — 'n' drops the brackets
txt::format("{::>5}", v);                      // [    1,    42,   255]
txt::format("{::#x}", v);                      // [0x1, 0x2a, 0xff]

sorted_map<int, string> m = /* 1 → one, 2 → two */;
txt::format("{}", m);                          // {1: "one", 2: "two"}
txt::format("{}", sorted_set<int>{1, 3});      // {1, 3}

txt::format("{}", pair<int, int>(1, 2));       // (1, 2)
txt::format("{:m}", pair<int, int>(1, 2));     // 1: 2
txt::format("{}", optional<int>(42));          // 42
txt::format("{}", optional<int>());            // nullopt
```

The elements of all of these are written in the debug form by default — see above.

**Anything with a `begin` and an `end`** is a list: [`vector`](../core/vector.md), [`slice`](../core/slice.md), [`array`](../core/array.md), [`deque`](../core/deque.md), the [`im`](../immutable/README.md) containers, `std::vector`, an `initializer_list`, a range of your own. **Anything with a `key_type`** is a table and goes in braces; with a `mapped_type` beside it, its elements are written `k: v`. **Anything structured bindings see** is a pair. Nothing is listed anywhere and nothing is included for it: the questions are asked of the type, so a container written after this header still answers.

Text is a range of characters and is not taken for one — a [`string`](../core/string.md), a `slice<const char>`, a `std::string`, a `const char*` all keep their own road.

### What follows a second colon

Everything after it belongs to the elements, and it goes down as many levels as the type has, one colon read at each:

```cpp
txt::format("{::>5}", v);                      // the elements padded to five
txt::format("{:n:#06x}", v);                   // no brackets, each element 0x00ff
txt::format("{:::>4}", deep);                  // a list of lists, the numbers padded
```

A colon is still an ordinary character to pad with for every value that holds nothing — `txt::format("{::>6}", 42)` is `::::42`, as it always was. Only a value made of other values reads it as theirs, which is the rule C++23 uses and for the same reason. One small thing did change with it: a **closing brace can no longer be the fill character** (`{:}<6}`), because a field now ends at the first `}`. C++23 forbids it too.

A width applies to the whole:

```cpp
txt::format("[{:>16}]", v);                    // [    [1, 42, 255]]
```

and it is measured in columns like any other field, so a list of Polish or Japanese words sits in its field as a word does. That is the one road here that is not written straight into the caller's sink: the padding cannot be known until the whole is written, so the body goes first into room the thread keeps for the purpose and is padded where it lies. It is written **once**. It used to be written twice, once to be counted and once to be kept, and over a value made of values that stopped being a constant — every level doubled the level below it. After that it went into 256 bytes of the call's own stack, and a level that did not fit was written again with every level under it, so a nest whose every level passed 256 bytes still doubled: sixteen levels of three hundred bytes took 443 ms. The levels of one field share one room now and grow it for one another, and the same nest is 0.17 ms; a list of 400 numbers in `{:>2000}` is 3.7 µs through `format_to` against 6.7. The room is kept between calls up to 64 KB, so only the first large field on a thread is written a second time, and only its innermost level. A range without a width costs nothing extra either way.

### A pattern of time

A time reads the rest of its field as a pattern of its own, which is the grammar `std::format` gives the types of `<chrono>`: `[[fill]align][width]` and then everything from the first `%` to the brace, colons included, so `{:%H:%M}` is one field and not a nest.

```cpp
txt::format("{:%H:%M}", t);                    // 12:41
txt::format("[{:>12%F}]", t.date());           // [  2026-09-24]
txt::format("{::%d.%m}", dates);               // [02.01, 04.03]: a list hands the pattern on
txt::format("{} {:%T}", sys_seconds, 90s);     // the types of <chrono> too
```

The module [`time`](../time/layout.md) brings the formatters — its own `datetime`, `date` and `weekday`, and `sys_time`, `local_time`, `year_month_day`, `weekday`, `hh_mm_ss` and `duration` of `<chrono>`, written as `std::format` writes them — and the pattern is checked where the program is compiled like any other specification (`{:%Q}` of a datetime is an error). A formatter of one's own reads such a field by having a `static constexpr bool takes_layout(std::string_view pattern)` (the pattern, or a view over nothing when none was written) and a `write(format_sink&, const T&, const format_spec&, std::string_view pattern)`; a sign, `#`, `0` and a type are not read for it. A [stencil](stencil.md)'s values are its own kinds (text, numbers, lists, tables), so a time goes into one as the text it was written to.

## An enumeration

Neither an `enum class` nor a plain `enum` is an integral type, so an enumeration used to have no formatter at all and did not compile. It is written as **the number it is**, with the whole numeric specification over it, taken from the underlying type — which is also what decides whether there is a sign:

```cpp
enum class colour : uint8_t { red = 0, green = 7, blue = 255 };

txt::format("{}", colour::blue);        // 255
txt::format("{:#04x}", colour::blue);   // 0xff
txt::format("[{:>6}]", colour::green);  // [     7]
txt::format("{:02x}", byte{10});   // 0a   — byte is one too
```

The number and not the name, because C++20 has no reflection and a table of names would have to be written by you in any case. `s` is deliberately left free: it is the shape you reach for when you do write them.

**The names are yours to give**, the same way any other type of your own gives them — a `format_value` beside the enumeration, which wins over the formatter above:

```cpp
namespace cards {
    enum class suit { hearts, spades, diamonds, clubs };

    void format_value(txt::format_sink& out, suit s, const txt::format_spec& spec) {
        static const char* names[] = {"hearts", "spades", "diamonds", "clubs"};
        txt::detail::write_text(out, names[int(s)], spec);
    }
}

txt::format("[{:>8}]", cards::suit::spades);    // [  spades]
```

Nothing here has to be opened or specialized for that, and the field still pads the name in columns. A `formatter<suit>` of your own works too and wins over both, if you want to say which specifications the names accept.

## A pattern the compiler never saw

A catalogue of translations is read from a file when the program starts, and the text of a message is then chosen by the language of whoever is reading it. No `consteval` can help there, so the asking moves to where the pattern arrives — and because it can now fail, what comes back is an [`optional`](../core/aliases.md):

```cpp
template<class... A> optional<string> format(runtime_pattern, const A&... args);
template<class... A> optional<size_t> format_to(slice<char>, runtime_pattern, const A&... args);
template<class... A> bool fits(const runtime_pattern&);

runtime_pattern runtime(const string& text);
```

```cpp
string entry = catalogue.at("items-left");                 // "pozostało: {}"
string s = txt::format(txt::runtime(entry), n)
               .value_or(txt::format("{} left", n));       // the built-in pattern if it does not fit
```

The pattern is walked once, and every field is weighed just before it is written; the first that does not fit stops the walk, and what was written by then is thrown away, so nothing half built is ever handed out. Everything the compiler refuses on the other road — a brace left open, a number with no value behind it, a precision asked of a whole number, `{:d}` over a name — is `nullopt` here.

It is **not** an exception and **not** a reason: every reason has the same remedy, which is to fall back on the pattern the program was written with, and that one is a literal the compiler already checked. Whoever wants to know earlier asks `fits` where the catalogue is loaded, naming the types the program will pass:

```cpp
for (auto& [key, text] : catalogue) {
    if (!txt::fits<int>(txt::runtime(text))) { /* this translation is broken */ }
}
```

`runtime_pattern` keeps the [`string`](../core/string.md) rather than pointing into it, so a pattern looked up in a table and handed straight to `format` is safe; a string of this library is shared and immutable, so keeping it costs a pointer and no characters. Naming a value by number — `{1} {0}` — matters more here than anywhere, because word order is the first thing a translation changes.

One thing is still not asked and cannot be: `{:c}` of a number no character holds is a fault of the value and not of the pattern, and it throws on this road exactly as it does on the other.

## A type of your own

Give it a `format_value` beside it, in its own namespace, and it is found there:

```cpp
namespace geometry {
    struct point { int x, y; };

    void format_value(txt::format_sink& out, const point& p, const txt::format_spec& spec) {
        char room[32];
        int n = std::snprintf(room, sizeof room, "(%d, %d)", p.x, p.y);
        txt::detail::put_padded(out, {room, size_t(n)}, spec);
    }
}

txt::format("{}", geometry::point{1, 2});          // (1, 2)
txt::format("[{:>10}]", geometry::point{3, 4});    // [    (3, 4)]
```

Nothing here has to be opened for that, and a type that says nothing at all is a message from the compiler rather than a link error.

## What it costs

The benchmark is in the tree, so the next change has something to measure against rather than starting over:

```sh
cmake --build <build> --target bench_format
<build>/benchmarks/bench_format sgcl padded      # and: std padded
<build>/benchmarks/bench_format sgcl list
```

Both sides write into a buffer the caller lends, so nothing is allocated and the writing is what is measured — `std::format_to_n` against `txt::format_to`, which is the same contract. Over a **stream of a thousand different values**, not one repeated, because half of what writing a number costs is that its length cannot be predicted:

| op | `txt::format` | `std::format` |
|---|---|---|
| `simple` `"{} left"` | **14.3 ns** | 27.8 |
| `padded` `"{:>12}"` | **16.3** | 39.9 |
| `centred` `"{:*^40}"` | **17.2** | 44.9 |
| `mixed` `"{:>8.3f} {:#x}"` | **50.6** | 87.9 |
| `whole` `"{}"` of a whole `double` | **17.2** | 60.6 |
| `text` `"{}"` of a short text | **10.2** | 17.8 |
| `five` five fields in one pattern | **66.6** | 115.0 |
| `literal` forty-five characters, no field | **7.8** | 86.2 |
| `alloc` into a string that is handed back | **23.6** | 35.1 |
| `runtime` a pattern read where it runs | **17.2** | 29.3 |

The `literal` row is the only one that flatters this side: `format_to_n` of a literal costs the standard 87.8 ns where the unbounded `format_to` costs 48.5, and over a number the two are the same. Everywhere else the bound is free.

And the ones the standard cannot be asked, C++23 being where it grew them:

| op | |
|---|---|
| `list` a list of five numbers | 52.5 ns, ten a number |
| `list100` a hundred of them | 824, eight a number |
| `elements` `"{::>5}"` | 74.5 |
| `listwidth` `"{:>40}"` over the list | 75.0 — the one road that goes through room of its own |
| `words` a list of text, escaped and quoted | 50.0 |
| `pair` | 29.8 |

Benchmarks are built at `-O2` whatever the configuration, which is what the numbers above are; the ones further down came from a harness at `-O3` and are not to be read against these.

Measured against the standard's on the same machine:

| | `txt::format` | `std::format` |
|---|---|---|
| `"{} left"` with a number | **23.2 ns** | 33.1 |
| the same through `format_to`, nothing allocated | **14.0** | — |
| `"{:>8.3f} {:#x}"` | **59.5** | 104.7 |

A list of five numbers is 50.1 ns through `format_to`, which is ten a number and less than the 75.4 the same five numbers cost written out as five fields of a pattern — a pattern of more than four fields keeps no steps and is read where it runs, and a range has one field and one step. A list of a hundred is 791 ns, or 7.9 a number. A width over a list is the one road that goes through room of its own before it is padded: the same five numbers in `{:>40}` are 75.0 ns, against the 112.6 they cost while the body was written twice. A pair is 21.8 and an `optional` that holds a number costs the number.

The floor of the work — `to_chars` and a copy — is 12.7 ns, of which a `string` of ours is 9.8; the standard does not pay that at this length, because it keeps a short string inside itself. Those three are one value repeated, which flatters the standard: over a stream of different ones the distance is wider, as the table above shows.

**The pattern is not read where the program runs at all.** The same pass that checks it writes down its steps — the run of literal text, the value that follows it, the specification already made out — so a call walks over as many steps as the pattern has fields and not over its characters. A pattern keeps four of them, which covers a message; one with more, or with a number too large for the narrow fields of a step, keeps none and is read the old way, which is correct and only a little slower.

The steps are kept small on purpose. The pattern is a temporary the conversion makes at the call site, so what it costs to build is the bytes it takes: at 280 of them that was 3.5 ns of every call, and over a literal with nothing to substitute — where there is nothing else to do — 8.9. Packed into fourteen bytes a step, the whole is 80, and a literal of forty-five characters went from 33.8 ns when it was read a byte at a time, through 9.5 when the reading was made wide, to **6.9** now that it is not read at all.

Decimal is written here and not by the standard, and without a branch on how many digits a number has: eight digits come out at once — as four dividends by a hundred and then by ten, in sixteen-bit lanes — and how many of them matter is one lookup on the bit width, after which they are slid up inside the register. It costs 1.10 ns for a 32-bit value against the standard's 1.70, and 2.96 against 4.35 for a 64-bit one, but the number that matters is over a stream of different values rather than one repeated, because half of what the standard pays there is mispredicting the length:

| over 1024 different values | before | after |
|---|---|---|
| `format_to("{}", int)` | 19.0 ns | **15.3** |
| `format_to("{}", int64_t)` | 20.2 | **17.0** |
| `format_to("{} of {}", int, int64_t)` | 45.0 | **31.9** |

Two fields gain thirteen nanoseconds, more than twice what one does, because two streams of values mispredict worse than one.

A `float` is written as a `float` and not as the `double` it widens to: the shortest text that reads back as `1.1f` is `1.1`, where the shortest that reads back as the `double` is `1.100000023841858` — the same number, a different question.

**A whole number written as `{}` does not go through the standard either.** Below 2^53 for a `double` and 2^24 for a `float` — where the significand runs out in each — the interval of values that read back as it is narrower than one, so its digits without their trailing zeros *are* the shortest representation — no algorithm needed — and when the last digit is not a zero the plain shape always wins, since the exponential one spends five more characters on the point and the exponent. The standard is at its slowest on exactly these, its digit-removal loop taking the trailing digits off one at a time:

| over 1024 values | before | after |
|---|---|---|
| whole numbers (`42.0`, `1000.0`) | 26.9 ns | **13.4** |
| powers of ten | 43.9 | **24.1** |
| a third of them whole, the rest not | 40.9 | **32.7** |

For a `float` the saving is larger still, because the standard's `to_chars` for one is not specialized the way it is for a `double`: a whole `float` costs it 24.7 ns of conversion where a whole `double` costs 12.6, which is backwards, and this road does either in 1.8.

Everything else — a fraction, a value at or above 2^53, any form with a type or a precision — is the standard's `to_chars`, and stays that way: the algorithms that find the shortest representation in general (Ryu, Schubfach) are no faster than what it already does.

The writing is kept apart from the padding for the same reason. A value that asks for no width — which is most of them — goes out as itself, and only a value in a field wider than itself takes the road that pads it. Written as one function, the compiler made a frame of 112 bytes and spilled six pairs of registers before anything happened, because the copying ladder appears in it four times over, and every value paid that whether or not it had a width: writing one number went from 9.8 ns to 7.1.
