[sgcl](../README.md) › [io](README.md)

# sgcl::io::println

```cpp
#include "sgcl/io/print.h"   // or "sgcl/io.h"

namespace sgcl::io {
    template<class... A>
    void println(const txt::format_pattern<std::type_identity_t<A>...>& pattern,    // (1)
                 const A&... args);
    void println();                                                                 // (2)
    template<class T>
    void println(const T& value);                                                   // (3)
    template<class... A>
    void println(const io::writer& to,                                              // (4)
                 const txt::format_pattern<std::type_identity_t<A>...>& pattern,
                 const A&... args);
    template<class... A>
    bool println(const txt::runtime_pattern& pattern, const A&... args);            // (5)
}

namespace sgcl {
    using io::println;
}
```

Writes text and a new line in one call: `println("{} items", n)` is
`io::stdout.write(txt::format("{} items\n", n))`. The pattern is [txt::format](../txt/format.md)'s, and the compiler
reads it: a brace left open, or a value that does not take its field, is an error of the build. The name is
`sgcl`'s as well as `io`'s, so `using namespace sgcl;` is all a program needs to write `println`. It is
[print](print.md) with the new line, C++23's `std::println`, Go's `fmt.Println` without its result.

1. The text of `pattern` and `args`, and a new line, on `io::stdout`.
2. A new line alone on `io::stdout`.
3. One value, as `"{}"` formats it, and a new line: `println(n)`, `println(name)`, `println(when)`, anything `"{}"`
   formats, a string or a text slice included. Takes part only for such a value that is not an array and
   not a writer: a literal is always the pattern.
4. The text of `pattern` and `args`, and a new line, on `to`, any writer: `println(file, "{} {}", key, value)`.
5. A pattern read while the program runs, `txt::runtime(text)` (a translation): the text and a new line on
   `io::stdout` when the pattern fits its values, nothing when it does not, so a program falls back on a pattern of
   its own.

A literal is always the pattern: `println("done")` prints `done`, and `println("{}")` does not compile (a field with
no value), so a literal with braces to be printed as they are is `println("{}", "{}")`.

## Parameters

| Parameter | Description |
|---|---|
| `pattern` | the pattern of [txt::format](../txt/format.md), a literal (1, 4), or a `txt::runtime` of a text (5) |
| `args` | the values of the fields |
| `value` | the one value to print |
| `to` | the stream to write to |

## Return value

- (1–4) None.
- (5) `true` when the pattern fitted its values and the line was written, `false` when nothing was.

## Complexity

Linear in the length of the text: the text and its new line are made as one string, once, and written in one
write.

## Exceptions

- `out_of_range` when a `{:c}` field is given a number no character holds.
- `length_error` when the text passes 4 GiB, the most a string holds.
- What the formatting of a value of the program's throws.
- What the stream's write throws: for `io::stdout`, [file::write](file/write.md)'s, `std::system_error` when the
  write would wait on the reactor and its thread cannot be started.

## Notes

Printing is not checked, as with [print](print.md): a line on a terminal that could not be written has nobody to
tell. A program that must know writes with `io::stdout.write(...)`, which answers an
[expected](../core/expected.md). A program that prints many lines from tasks writes through a
[buffered_writer](buffered_writer.md).

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    int left = 3;
    println("{} items", left);
    println(left);
    println("{}", "{}");
    println();

    io::buffer out;
    println(out, "{}={}", "key", "value");
    print("{}", out.text());

    for (string translated : {"noch {0} übrig", "{0} von {1}"}) {
        if (!println(txt::runtime(translated), left)) {  // the second does not fit one value
            println("{} left", left);
        }
    }
}
```

Output:

```text
3 items
3
{}

key=value
noch 3 übrig
3 left
```

## See also

- [print](print.md): the same without the new line
- [eprintln](eprintln.md): on the standard error
- [txt::format](../txt/format.md): the patterns
- [standard_stream](standard_stream.md): `io::stdout`, `io::stderr`
