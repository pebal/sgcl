# sgcl::txt::stencil

```cpp
#include "sgcl/txt/stencil.h"   // or "sgcl/txt/txt.h"

class stencil;                                // a template, read once and written many times
class stencil_error;                          // where and why a source is not a template
class value;                                  // text, a number, a truth, nothing, a list, a mapping
class list : public value;                    // list{1, 2, 3}
class object : public value;                  // object{{"a", 1}, {"b", 2}}
enum class value_kind { none, boolean, integer, real, text, list, object };

using stencil_function = function<value(const value&, slice<const value>)>;
class stencil_functions;                      // the functions a pipeline may call
```

A text written from a shape the program did not write: a page, a letter, a report, whose form lives in a file and is changed by whoever owns the words rather than by whoever owns the program. The shape is text with **actions** in it, and the values that go in are handed over by the caller.

A template is **read once and written many times** — that is the whole of what it is for — so parsing and rendering are separate calls with a compiled form in between.

## Why this is in txt

Because of the colon. A value may carry the **specification of [`format`](format.md)** after it, and that specification is read by `format.h`'s own reader and written by `format.h`'s own writers.

`{{ total:>8.2f }}` pads and rounds exactly as `txt::format("{:>8.2f}", total)` does — same grammar, same refusals, same writers. Nothing about converting a number, padding a field, stopping a precision on a code point or escaping a debug form is written a second time here. Two implementations of writing a number in one module is exactly what this project avoids, and the [test](../../../tests/txt/stencil.cpp) asks both sides the same eleven questions and compares, so the two cannot drift apart unnoticed.

It follows that a field of text is **measured in the columns it takes**, not in its bytes, as everywhere else in this module: `żółć` is eight bytes and four columns, so `{{ s:>10 }}` pads it by six.

And a `formatter<value>` carries it the rest of the way, so a list and a mapping come out in the shapes of C++23 without a bracket being written anywhere in this header.

**The second colon is the one place a field here is not a field there**, and it is worth knowing before it surprises you. A pattern knows the type of every value where it is read, so it can tell a colon that opens a specification for the elements from a colon that is merely a character to pad with: `txt::format("{::>4}", 7)` is `:::7`, a number holding nothing for a nested specification to be about. A template knows no types at all — the values arrive long after the source is read — so it always reads the second colon as opening one, and where the value turns out to hold nothing the nested part is dropped and the rest of the field kept.

A fill of colons is the whole of what is lost.

**A code point is the other**, and for the same reason the other way round. `{:c}` of `format` narrows to a `char` and throws over a number no `char` holds, the value being what is wrong and the caller being there to catch it. A template has no such caller — the letter comes from a file and the number from the data — so `{{ n:c }}` is the **code point** `n`, written as the bytes it takes, and a number that is no code point drops the letter and keeps the column.

That also keeps every page valid UTF-8, which narrowing would not: `{:c}` of 233 as a single byte is not a character at all.

## The syntax

| | |
|---|---|
| `{{ name }}` | a field of the current element |
| `{{ .name }}` | the same, written the way Go writes it |
| `{{ a.b.c }}` | a path down through mappings |
| `{{ . }}` | the current element itself |
| `{{ $ }}`, `{{ $.a }}` | the whole of the data, from any depth |
| `{{ name:spec }}` | written with that [format specification](format.md) |
| `{{ name \| upper }}` | piped through a function, and through as many as you like |
| `{{ if x }}…{{ else if y }}…{{ else }}…{{ end }}` | |
| `{{ range xs }}…{{ else }}…{{ end }}` | `.` is the element; the `else` arm is taken when there is nothing to walk |
| `{{ with x }}…{{ else }}…{{ end }}` | `.` becomes `x` where `x` is true |
| `{{/* … */}}` | a comment, and nothing inside it is read |
| `{{- x }}`, `{{ x -}}` | the white space before or after the action is taken away |

A **bare name is a field of the current element**, which is the one place this departs from Go on purpose: over there a name with no dot is a function call, here it is the shorthand that Jinja and Handlebars have. `{{ name }}` and `{{ .name }}` mean the same thing.

**Truth** is the rule a reader of Go or of Python expects: nothing, `false`, a number that is nought, text with no characters, and a list or a mapping with no elements are all false. Everything else is true.

**A name the data does not carry writes nothing** — not `<no value>`, which is what Go writes — and still fills its field, so `[{{ missing:>5 }}]` is five spaces in brackets and a column stays a column.

There is no escape for a literal `{{`. Write it as a literal value: `{{ "{{" }}`.

## The values

C++20 has no reflection, so a template cannot walk the fields of a struct of yours and this does not pretend to. You build a `value`, and the braces are arranged so the call reads as data.

A `value` is thirty-two bytes — a [`variant`](../core/variant.md) over nothing, a truth, a whole number, a real one, a [`string`](../core/string.md) and two pointers — and nothing at all is allocated for a number or a truth.

One trap of the language comes with the braces and is worth saying out loud: `value v{"one"}` is a **list of one piece of text**, where `value v("one")` is the text. The initializer list wins in copy-list-initialization, as it does everywhere else in C++.

A mapping keeps the order you wrote it in, not the order of a hash ([`ordered_map`](../core/ordered_map.md)): a page written twice running has to be the same page. A walk over a mapping makes the element a row of `key` and `value`.

## The pipeline

Six functions are always there:

| | |
|---|---|
| `upper`, `lower`, `title` | the **full** mappings of [`case`](case.md) — `straße` upper-cases to `STRASSE` |
| `trim` | the white space off both ends |
| `escape_html` | `& < > " '` as entities |
| `default` | the argument, where what came down the pipe is empty |

Yours join them, and may replace them. A function takes what came down the pipe and whatever was written after its name — literals and paths both.

A template calling a name the table does not know **fails to parse**, rather than writing nothing where a word was wanted.

**A pipeline function must be pure of side effects.** A render that does not fit the room it keeps on the stack writes one step again — one field, into room twice the size, rather than the page — so the pipeline of that field runs a second time, path and all. Which field that is depends on where the page happens to cross a kilobyte and each doubling after it, so a function that counts, logs or reads a clock is right on most pages and wrong on some, and the some are not the ones anybody tests:

```text
a page of  909 characters, three fields: the function ran 3 times
a page of 1029 characters, three fields: 4
a page of 2109 characters, three fields: 4
```

Nothing else in a render depends on it.

## The boundary: escaping is yours

**There is no escaping here that knows where a value lands.** Go has that in `html/template` — it reads the HTML around a field and escapes for an attribute, a URL or a script accordingly. That is a parser for a second language and a promise this module is not in a position to keep, so it is not made.

What is here is `escape_html` as a function of the pipeline, asked for by name. Whoever writes the template decides where it is needed, and **the safety of the HTML is the caller's**.

In this module a boundary is written down rather than discovered.

## Nothing throws

A source that does not parse is a thing that really happens — a file somebody edited — so it is an answer and not an exception, the same register [`format`](format.md) uses for a pattern read where the program runs. `parse` gives back an `expected`, whose error, a `stencil_error`, says where and why.

A source the program itself writes, a literal in the code, is not data that may be wrong: it is constructed, `txt::stencil t("Hello, {{ name }}!")`, and a slip in it throws `bad_expected_access<stencil_error>` with the same error — the mistake of the program, found the first time the line runs.

Everything that can be settled where the source is read is settled there: the shape of every specification, that every function named is one the table knows, that every block is closed, that no jump goes nowhere. The one thing that cannot be is the **type** a field will meet, the values arriving long after. `{:d}` over a name is that case: refusing the page would punish the reader for the template author's slip and writing nothing would lose the value, so the type letter is dropped and the fill, the alignment and the width are kept — `[{{ s:>6d }}]` over `ada` is `[   ada]`.

`render` and `render_to` throw nothing either, and that is a promise about the values and not only about the source — it is the same field and the same data meeting for the first time. The two roads that could take a page down are shut:

- `{{ n:c }}` over a number no character holds is the **code point** `n`, or, where `n` is none, the number with the letter dropped. `format` throws there; a template has a reader waiting for the page.
- A value **put inside itself** — `object o; o.set("self", o)` shares one mapping, since a value reaches what it holds through a tracked pointer — is written sixty-four levels deep and then `...`. The collector is happy with a cycle; the writer has no memory of where it has been.

A width past 65535 is refused where the source is read, like any other specification that does not fit.

## What it costs

Measured on this machine at `-O2`, over a stream of different values ([`benchmarks/txt/stencil.cpp`](../../../benchmarks/txt/stencil.cpp)), in nanoseconds per render:

| | render | parse |
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
| a hundred such rows | 5296.6 | |
| the same ten rows into a buffer you lend | 524.4 | |

Reading a source is two to four times writing from it, which is the argument for the compiled form. Of a single field's 52.1 ns, about 26 is the [`string`](../core/string.md) handed back and most of the rest is hashing the name in the mapping; `render_to` into a buffer you lend allocates nothing at all.

A render allocates for its own bookkeeping **not at all** while the blocks are shallower than four, which is nearly every template: the parser counts how deep they go and the walk keeps its frames, its dots and its owned values on the stack. Past that the three move to vectors, in a call of their own, so that the page with no block in it does not carry them. A walk over a list points straight at the elements and copies none of them.

### A page longer than the kilobyte on the stack

A page that fits that kilobyte is written once. A longer one **grows the room** rather than being written twice: the walk asks after each step that writes whether that step ran off the end, and when it did it takes twice as much room, carries over what stood before the step and writes that one step again. Never the page — a page of a hundred kilobytes is one walk and seven doublings, not two walks.

The asking is what a page that fits pays for a page that does not, and it has to be made to cost nothing. Two things do that, and both were arrived at by reading the disassembly rather than by guessing: the walk reads the capacity **once** into a local of its own, because a field of the sink would have to be read back after every call a step makes, and the test is marked `[[unlikely]]`, because a test whose other side calls is otherwise laid out with the common case **jumping over** the call. The second was the larger by far — the compare itself is two instructions, while the branch at every literal run of every row came to fourteen per cent of a page of ten rows.

This is where a page parts company with a [format pattern](format.md), which is written twice and stays that way: a pattern's second pass is `to_chars` and `memcpy` over a handful of fields and costs less than the copying a doubling buffer does, where a page's second pass is every branch tested again, every row of every loop walked again and every `upper` and `escape_html` run again. The benchmark keeps both roads so the question can be asked again — `bench_stencil sgcl` against `bench_stencil twopass`, the second being exactly what `render` did before. Best of three alternated runs, nanoseconds a page:

| page | grown | written twice |
|---|---|---|
| 244 characters, fits the stack room | 517.9 | 514.4 |
| 1137 characters | 2354.8 | 4559.4 |
| 2359 characters (the hundred rows above) | 5096.1 | 9890.2 |
| 9879 characters | 19946.8 | 40592.3 |
| 98709 characters | 193570.5 | 401917.1 |

Half, at every size, because the second walk was the whole of the cost and the copying is next to none of it: the seven doublings on the way to a hundred kilobytes carry 127 KB of characters, and one grown walk comes to 193.6 µs against the 201.0 that half of two walks is. The page that fits pays nothing — the first row is the same number twice, and against the commit before the room could grow at all, two binaries alternated in one window and best of three, the whole short end is level as well: `empty` 28.9 against 28.7, `simple` 51.2 against 51.9, `five` 178.5 against 178.1, `branch` 57.1 against 55.9, `rows` 521.8 against 526.2, `render_to` 502.5 against 506.8. These rows were measured together and are not comparable to the cent with the table above them, which is from an earlier round.

## Against Go's `text/template`

The same eleven shapes and the same stream of values on both sides ([`benchmarks/go/stencil`](../../../benchmarks/go/stencil/main.go)), nanoseconds per render, each side writing a fresh string:

| | sgcl | Go | |
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

| | sgcl | Go | |
|---|---|---|---|
| one value in a line of text | 127.5 | 1516.7 | 11.9× |
| five values | 396.8 | 3214.1 | 8.1× |
| an `if` with an `else` | 234.6 | 2115.9 | 9.0× |
| ten rows of two fields | 504.5 | 3620.2 | 7.2× |

**Where the difference comes from, and where it does not.** Not from allocation: keeping one buffer instead of building a string helps Go nothing (2654.6 against 2644.5) and helps this nothing either (502.9 against 517.4), so neither side is bound by the writing. It comes from how a name is looked up. Go walks the data with reflection — `map[string]any` at every step, an `interface{}` unwrapped, a `reflect.Value` made — where a [`value`](#the-values) here is a variant of thirty-two bytes and a mapping is an [`ordered_map`](../core/ordered_map.md) of them. That is also why the widest gap is the specification: `{{ printf "%8.2f" .d }}` is a function called through reflection over there, and `{{ d:>8.2f }}` is [`format`](format.md)'s own writer here, which is the reason this module has a template at all.

Both columns above were measured in one sitting after the room was made to grow, the two binaries alternated under an empty environment, and they are left as they were rather than half refreshed. One row has moved since: a stage of a pipeline that takes no arguments no longer builds room for eight of them, so the two functions read about 145 ns here and not 153.1, which is 4.0× rather than 3.8×. It wants a fresh sitting of both columns and not a number dropped into one of them. The hundred rows were the row that flattered this side least, 2.4×, while a page over a kilobyte was still written twice here; it reads 4.7× now, which is where the rest of the table already was. The row that flatters this side most is reading a source, where the shapes are small and Go's parser builds a tree of nodes on the heap.

Two shapes are not the same question on both sides and are marked as such rather than quietly counted: a name the data does not carry writes nothing here and `<no value>` in Go, and the specification is a format specification here and a `printf` call there. The rest are the same source, the same data and the same output — the [oracle below](#held-to) is what keeps them so.

## Held to

Go's own `text/template`, over the subset both can be asked: [`tools/stencil_oracle.go`](../../../tools/stencil_oracle.go) puts fifty-three sources to it over one shape of data and writes its answers out as the vectors the test uses. Where the two differ the case is left out rather than bent until it passes, and the Go file says which and why — the bare name, a missing name, a walk over a mapping, the full case mappings, and the spelling of a quote in `escape_html`. Those five are checked by hand instead.

The half no oracle can reach is held to `format.h` directly, and both halves are fuzzed under the address and undefined-behaviour sanitizers: twenty thousand sources nobody meant, every one either refused with a place and a reason or rendered without a crash; two hundred thousand **specifications** after the colon, each rendered and then asked again through `render_to`, which must report the same size; and twenty thousand random value trees over thirteen templates, `render` against `render_to` into a buffer with a guard behind it. The last two are what said that a field of a template is a field of a pattern everywhere but the second colon, and that `{:c}` was the one road that could throw.

## Members

### stencil

```cpp
class stencil {
    static expected<stencil, stencil_error> parse(const string& source);
    static expected<stencil, stencil_error> parse(const string& source, const stencil_functions&);
    explicit stencil(const string& source);   // a literal: parse(source), or bad_expected_access<stencil_error> with its message (DESIGN 234)
    explicit stencil(const string& source, const stencil_functions&);   // the same with a table
    static bool parses(const string& source);

    string render(const value& data) const;
    size_t render_to(const slice<char>& buffer, const value& data) const;
    size_t steps() const;
    const string& source() const;
};
```

`parse` answers an `expected` for a source that may be wrong, and the constructor throws for a literal of the program's own; `render` writes the page, and `render_to` writes it into a buffer the caller lends and answers the size the page takes.

### stencil_error

```cpp
class stencil_error {                         // where and why a source is not a template
    size_t offset() const noexcept;           // the byte the reading stopped on
    size_t line() const noexcept;             // from 1
    size_t column() const noexcept;           // from 1
    string message() const;                   // why, in a few words
};
```

What `parse` answers when a source is not a template.

### value

```cpp
class value {                                 // text, a number, a truth, nothing, a list, a mapping
    value_kind kind() const noexcept;
    bool is_none() const noexcept;
    bool truthy() const noexcept;             // what if, with and default ask: nothing, false, 0, "" and an empty list or mapping are false
    size_t size() const noexcept;             // the elements of a list or a mapping; 0 for anything else
    const value* find(const string& name) const noexcept;   // a mapping's value of the name, or null
    const value* at(size_t index) const noexcept;           // a list's n-th element, or null
    const string* text() const noexcept;      // the text this holds, or null
    string to_string() const;                 // written as format writes it, with no specification
};
class list : public value;                    // list{1, 2, 3}
class object : public value;                  // object{{"a", 1}, {"b", 2}}
```

Built with `list` and `object`, and asked what it holds; `find`, `at` and `text` answer null where there is nothing of that kind.

### stencil_functions

```cpp
class stencil_functions {
    stencil_functions();                     // the six below
    void add(const string& name, stencil_function fn);
    const stencil_function* find(const string& name) const;
    static const stencil_functions& builtin();
};
```

The functions a pipeline may call: a table made with no arguments holds the six, and `add` joins one to them or replaces one of the same name.

## Examples

Parsed once, rendered as often as there is data:

```cpp
#include "sgcl/io/io.h"
#include "sgcl/txt/txt.h"

using namespace sgcl;

int main() {
    string source = "Hello, {{ name }}!";
    auto t = txt::stencil::parse(source);
    if (!t) {
        println("{}", t.error().message());  // the source is not a template
        return 1;
    }
    println("{}", t->render(txt::object{{"name", "Ada"}}));
    return 0;
}
```

Output:

```text
Hello, Ada!
```

A value written with a specification of `format`, and a field of text measured in columns:

```cpp
#include "sgcl/io/io.h"
#include "sgcl/txt/txt.h"

using namespace sgcl;

int main() {
    txt::stencil t("{{ total:>8.2f }} | {{ name:?}} | {{ s:>10 }}|");
    println("{}", t.render(txt::object{{"total", 3.14159}, {"name", "Ada"}, {"s", "żółć"}}));
    return 0;
}
```

Output:

```text
    3.14 | "Ada" |       żółć|
```

A list in the shapes of C++23, the second colon belonging to its elements:

```cpp
#include "sgcl/io/io.h"
#include "sgcl/txt/txt.h"

using namespace sgcl;

int main() {
    txt::value data = txt::object{{"xs", txt::list{1, 2, 3}}};
    for (auto source : {"{{ xs }}", "{{ xs::>4 }}", "{{ xs:n }}"}) {
        println("{}", txt::stencil(source).render(data));
    }
    return 0;
}
```

Output:

```text
[1, 2, 3]
[   1,    2,    3]
1, 2, 3
```

Over a value that holds nothing the nested part is dropped, where a pattern pads with colons:

```cpp
#include "sgcl/io/io.h"
#include "sgcl/txt/txt.h"

using namespace sgcl;

int main() {
    txt::value data = txt::object{{"n", 7}};
    println("[{}] [{}]", txt::stencil("{{ n::>4 }}").render(data),
            txt::stencil("{{ n:c>4 }}").render(data));
    println("[{}]", txt::format("{::>4}", 7));
    return 0;
}
```

Output:

```text
[7] [ccc7]
[:::7]
```

`{{ n:c }}` is the code point `n`, and a number that is no code point keeps its column:

```cpp
#include "sgcl/io/io.h"
#include "sgcl/txt/txt.h"

using namespace sgcl;

int main() {
    txt::stencil letter("{{ n:c }}");
    for (int n : {65, 233, 12345}) {
        print("{} ", letter.render(txt::object{{"n", n}}));
    }
    println("{}", txt::stencil("[{{ n:>6c }}]").render(txt::object{{"n", -1}}));
    return 0;
}
```

Output:

```text
A é 〹 [    -1]
```

A value built from braces, which read as data:

```cpp
#include "sgcl/io/io.h"
#include "sgcl/txt/txt.h"

using namespace sgcl;

int main() {
    txt::value data = txt::object{
        {"user",   txt::object{{"name", "Ada"}, {"admin", true}}},
        {"scores", txt::list{91.5, 88.0}},
        {"tags",   {"one", "two"}},  // a list inside a mapping needs no type named
        {"count",  7},
    };
    txt::stencil t("{{ user.name }}{{ if user.admin }} (admin){{ end }}: {{ scores }}, "
                   "{{ tags:n }}, {{ count }}");
    println("{}", t.render(data));
    return 0;
}
```

Output:

```text
Ada (admin): [91.5, 88], "one", "two", 7
```

A walk over a mapping, in the order it was written:

```cpp
#include "sgcl/io/io.h"
#include "sgcl/txt/txt.h"

using namespace sgcl;

int main() {
    txt::value data = txt::object{{"m", txt::object{{"b", 2}, {"a", 1}}}};
    println("{}", txt::stencil("{{ range m }}{{ .key }}={{ .value }} {{ end }}").render(data));
    return 0;
}
```

Output:

```text
b=2 a=1 
```

A function of one's own in the pipeline, with an argument written after its name:

```cpp
#include "sgcl/io/io.h"
#include "sgcl/txt/txt.h"

using namespace sgcl;

int main() {
    txt::stencil_functions table;  // starts with the six
    table.add("money", [](const txt::value& v, slice<const txt::value> args) {
        return txt::value(txt::format("{:.2f} {}", v, args[0]));
    });
    auto t = txt::stencil::parse("{{ price | money \"zł\" }}", table);
    println("{}", t->render(txt::object{{"price", 12.5}}));
    return 0;
}
```

Output:

```text
12.50 zł
```

Escaping, asked for by name where the template author knows it is needed; a value that lands in a URL is not HTML text, and nothing here knows that:

```cpp
#include "sgcl/io/io.h"
#include "sgcl/txt/txt.h"

using namespace sgcl;

int main() {
    txt::value data = txt::object{{"comment", "<b>hi</b> & bye"}};
    println("{}", txt::stencil("<p>{{ comment | escape_html }}</p>").render(data));
    return 0;
}
```

Output:

```text
<p>&lt;b&gt;hi&lt;/b&gt; &amp; bye</p>
```

A source that is not a template, answered with where and why:

```cpp
#include "sgcl/io/io.h"
#include "sgcl/txt/txt.h"

using namespace sgcl;

int main() {
    auto t = txt::stencil::parse("Hello, {{ name ");
    if (!t) {
        println("{}:{}: {}", t.error().line(), t.error().column(), t.error().message());
    }
    return 0;
}
```

Output:

```text
1:8: the action is not closed
```

## See also

[The module](README.md); [`format`](format.md), whose specification a field carries; [`case`](case.md), the full mappings `upper`, `lower` and `title` use; [`ordered_map`](../core/ordered_map.md), the mapping a value keeps.
