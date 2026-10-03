[sgcl](../README.md) › [io](README.md)

# sgcl::io::print

```cpp
#include "sgcl/io/print.h"   // or "sgcl/io.h"

namespace sgcl::io {
    template<class... A>
    void print(const txt::format_pattern<std::type_identity_t<A>...>& pattern,    // (1)
               const A&... args);
    template<class T>
    void print(const T& value);                                                   // (2)
    template<class... A>
    void print(const io::writer& to,                                              // (3)
               const txt::format_pattern<std::type_identity_t<A>...>& pattern,
               const A&... args);
    template<class... A>
    bool print(const txt::runtime_pattern& pattern, const A&... args);            // (4)
}

namespace sgcl {
    using io::print;
}
```

Writes text in one call: `print("{} items", n)` is `io::stdout.write(txt::format("{} items", n))`. The pattern is
[txt::format](../txt/format.md)'s, and the compiler reads it: a brace left open, or a value that does not take its
field, is an error of the build. The name is `sgcl`'s as well as `io`'s, so `using namespace sgcl;` is all a program
needs to write `print`. [println](println.md) is the same with a new line, [eprint](eprint.md) the same on the
standard error. What C++23's `std::print` does, and Go's `fmt.Printf` and `fmt.Fprintf` without their result.

1. The text of `pattern` and `args` on `io::stdout`.
2. One value, as `"{}"` formats it: anything `"{}"` formats, a string or a text slice included. Takes part only for
   such a value that is not an array and not a writer: a literal is always the pattern, so `print("done")`
   reads it as one, braces and all.
3. The text of `pattern` and `args` on `to`, any writer: a file, a connection, a buffer, a buffered writer.
4. A pattern read while the program runs, `txt::runtime(text)` (a translation): the text on `io::stdout` when the
   pattern fits its values, nothing when it does not.

## Parameters

| Parameter | Description |
|---|---|
| `pattern` | the pattern of [txt::format](../txt/format.md), a literal (1, 3), or a `txt::runtime` of a text (4) |
| `args` | the values of the fields |
| `value` | the one value to print |
| `to` | the stream to write to |

## Return value

- (1–3) None.
- (4) `true` when the pattern fitted its values and the text was written, `false` when nothing was.

## Complexity

Linear in the length of the text: it is made once and written in one write.

## Exceptions

- `out_of_range` when a `{:c}` field is given a number no character holds.
- `length_error` when the text passes 4 GiB, the most a string holds.
- What the formatting of a value of the program's throws.
- What the stream's write throws: for `io::stdout`, [file::write](file/write.md)'s, `std::system_error` when the
  write would wait on the reactor and its thread cannot be started.

## Notes

Printing is not checked. A text on a terminal that could not be written has nobody to tell, and a result every call
would have to drop is noise, so the functions return nothing (the runtime form says only whether the pattern
fitted). A program that must know, a pipe closed under it, writes with `io::stdout.write(...)`, which answers an
[expected](../core/expected/README.md).

The streams stay what they are: `io::stdout` is a stream, given to `io::copy`, a
[buffered_writer](buffered_writer/README.md) over it, or `co_await io::stdout.async_write(...)` in a task. `print` is only
the short way to put text on it, or on any other writer. In a task, `print` writes as `io::stdout.write` does, on
the calling worker; a program that prints much from tasks writes through a buffered writer.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    print("{} + {} = ", 2, 3);
    print(2 + 3);
    print("\n");

    io::buffer report;
    print(report, "{:>6}|", "right");
    print("{}\n", report.text());

    if (!print(txt::runtime("{0} of {1}\n"), 7)) {  // one value for two fields: nothing printed
        print("{} of an unknown number\n", 7);
    }
}
```

Output:

```text
2 + 3 = 5
 right|
7 of an unknown number
```

## See also

- [println](println.md): the same and a new line
- [eprint](eprint.md), [eprintln](eprintln.md): on the standard error
- [txt::format](../txt/format.md): the patterns
- [standard_stream](standard_stream/README.md): `io::stdout`, `io::stderr`
- [writer](writer/README.md): any stream to print on
