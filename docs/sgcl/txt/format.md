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
template<class... A> size_t format_to(slice<char> buffer, pattern, const A&... args);

struct format_spec { char fill; char align; char sign; bool alternate; bool zero;
                     unsigned width; int precision; char type; };
class format_sink { void put(char); void put(std::string_view); void fill(char, size_t);
                    size_t size() const; };
template<class T, class = void> struct formatter;
```

`format` returns a [`string`](../core/string.md). `format_to` writes into memory the caller lends and returns **what the whole text takes**, whether or not it fitted — so a buffer can be sized from a first call with an empty one, and a caller who knows its text is short pays no allocation at all:

```cpp
char room[64];
size_t needed = txt::format_to(slice<char>(room, room + sizeof room), "{} left", n);
if (needed > sizeof room) { /* the text was cut; ask for that much */ }
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
| `char`, `char32_t` | `c` (the default), the integer forms; a code point is written as UTF-8 |
| text (`string`, `slice<const char>`, `const char*`, `std::string_view`) | `s`, a width and a precision **in columns** |
| pointers | `p` |

`{{` and `}}` stand for a brace. A field may name its value by number — `{1} {0}` — or leave it out, and then the values are taken in order.

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

Measured against the standard's on the same machine:

| | `txt::format` | `std::format` |
|---|---|---|
| `"{} left"` with a number | **23.2 ns** | 33.1 |
| the same through `format_to`, nothing allocated | **14.0** | — |
| `"{:>8.3f} {:#x}"` | **59.5** | 104.7 |

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
