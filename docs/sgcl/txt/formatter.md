[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::formatter\<T\>

```cpp
#include "sgcl/txt/format.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    template<class T, class = void>
    struct formatter {};
}
```

What a type may be told to do with itself in a field of [format](format.md): which
[specifications](format.md#the-specification) it takes — asked where the pattern is read, so that the compiler
refuses the others — and how it is written. Every type of the library and of the standard that `format` writes is
written by a specialization of it; any other type is written by a `format_value` found beside it, which takes
whatever it is given, or by a specialization of the program's own, which says what it takes.

The primary template is defined and empty, not merely declared: a type with no formatter of its own answers "no
member" where it is asked about, which a `requires` expression sees, where an incomplete type would be an error
before anything got to ask. Where `std::formatter` has a `parse` that reads the specification and a `format` that
writes into an iterator, this one is asked yes-or-no questions about a specification already read into a
[format_spec](format_spec.md), and writes into a [format_sink](format_sink.md); all its members are `static`.

## Rules

- A specialization of the program's own is declared in `sgcl::txt` (`template<> struct sgcl::txt::formatter<T>`) and
  wins over the library's formatter of an enumeration and over a `format_value` beside the type: it is what a type
  uses to say which specifications it accepts.
- It has [write](formatter/write.md), and, to be asked about specifications, [takes](formatter/takes.md) with
  [takes_precision](formatter/takes_precision.md). Without `takes` every specification is accepted. A type made of
  other values adds [takes_nested](formatter/takes_nested.md), and then a second colon is read as its; a type that
  reads a pattern of its own adds [takes_layout](formatter/takes_layout.md), and then the field runs from the first
  `%` to the brace.
- `write` declared `noexcept` keeps [format_to](format_to.md) `noexcept` for the type.

## Template parameters

| Parameter | Description |
|---|---|
| `T` | the type of the value written |
| (unnamed) | `void`; left for the library's partial specializations that choose by a condition on `T` |

## Member functions

The members of a specialization:

| Function | Description |
|---|---|
| [takes](formatter/takes.md) | whether the type takes a type letter |
| [takes_precision](formatter/takes_precision.md) | whether the type takes a precision |
| [takes_nested](formatter/takes_nested.md) | whether the type takes a specification for what it holds, after a second colon |
| [takes_layout](formatter/takes_layout.md) | whether the type takes a pattern of its own, from the first `%` |
| [write](formatter/write.md) | writes a value into the sink |

## Specializations

| Type | Header | Takes |
|---|---|---|
| the integral types but `bool` and `char`, at most as wide as `long long` | `format.h` | `d` `b` `B` `o` `x` `X` `c`; a wider one is refused at compile time |
| `bool` | `format.h` | `s` and the integer forms |
| `char` | `format.h` | `c`, `?` and the integer forms |
| `char32_t` | `format.h` | `c`, `?` and the integer forms; written as UTF-8 |
| the floating-point types | `format.h` | `f` `F` `e` `E` `g` `G` `a` `A` and a precision |
| a type that converts to `std::string_view` (`const char*`, `std::string`, `std::string_view`) | `format.h` | `s`, `?` and a precision |
| [string](../core/string.md), `slice<const char>` | `format.h` | `s`, `?` and a precision |
| [duration](../core/duration.md) | `format.h` | `s` |
| a pointer that is not text | `format.h` | `p` |
| an enumeration with no `format_value` beside it | `format.h` | the integer forms but `c` |
| a range: `begin` and `end`, not text, no `format_value` | `format.h` | `n`, and a nested specification for its elements |
| a pair or a tuple: what structured bindings see, not a range, not text, no `format_value` | `format.h` | `n`, `m` over two values, and a nested specification for each member |
| `optional<T>` | `format.h` | what `T` takes, and its nested specification or pattern |
| [value](value.md) | `stencil.h` | anything: what its alternative does not take is dropped from the field |
| the times of [time](../time/README.md#formatting-with-txt) and of `<chrono>` | the headers of time | a pattern of time |

## Example

A formatter of one's own, which says what the names of an enumeration take:

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

namespace cards {
    enum class suit { hearts, spades };
}

template<>
struct sgcl::txt::formatter<cards::suit> {
    static constexpr bool takes(char type) noexcept {
        return !type || type == 's' || type == 'c';
    }

    static constexpr bool takes_precision() noexcept {
        return false;
    }

    static void write(format_sink& out, cards::suit s, const format_spec& spec) noexcept {
        bool hearts = s == cards::suit::hearts;
        if (spec.type == 'c') {
            write_padded(out, hearts ? "♥" : "♠", spec);
        } else {
            write_padded(out, hearts ? "hearts" : "spades", spec);
        }
    }
};

int main() {
    println("[{:>8}] [{:c}] [{:s}]", cards::suit::spades, cards::suit::hearts, cards::suit::hearts);
    // println("{:x}", cards::suit::spades);  // does not compile: the formatter does not take x
    println("{}", txt::format(txt::runtime("{:x}"), cards::suit::spades).has_value());
    return 0;
}
```

Output:

```text
[  spades] [♥] [hearts]
false
```

## See also

- [format](format.md#a-type-of-ones-own): a type of one's own, by `format_value` or by a formatter
- [format_spec](format_spec.md), [format_sink](format_sink.md), [write_padded](write_padded.md): what a formatter
  reads and writes with
