[sgcl](../README.md) › [io](README.md)

# sgcl::io::eprint

```cpp
#include "sgcl/io/print.h"   // or "sgcl/io.h"

namespace sgcl::io {
    /*(1)*/ template<class... A>
            void eprint(const txt::format_pattern<std::type_identity_t<A>...>& pattern,
                        const A&... args);
    /*(2)*/ template<class T>
            void eprint(const T& value);
}

namespace sgcl {
    using io::eprint;
}
```

Writes text on the standard error in one call: [print](print.md) on `io::stderr`, `eprint("{} failed", name)` is
`io::stderr.write(txt::format("{} failed", name))`. The pattern is [txt::format](../txt/format.md)'s, read by the
compiler. The name is `sgcl`'s as well as `io`'s. Go's `fmt.Fprintf(os.Stderr, ...)` without its result.

1. The text of `pattern` and `args` on `io::stderr`.
2. One value, as `"{}"` formats it. Takes part only for a value `"{}"` formats that is not an array and not
   a writer: a literal is always the pattern.

## Parameters

| Parameter | Description |
|---|---|
| `pattern` | the pattern of [txt::format](../txt/format.md), a literal |
| `args` | the values of the fields |
| `value` | the one value to print |

## Return value

None.

## Complexity

Linear in the length of the text: it is made once and written in one write.

## Exceptions

- `out_of_range` when a `{:c}` field is given a number no character holds.
- `length_error` when the text passes 4 GiB, the most a string holds.
- What the formatting of a value of the program's throws.
- What the write to `io::stderr` throws, [file::write](file/write.md)'s: `std::system_error` when the write would
  wait on the reactor and its thread cannot be started.

## Notes

Printing is not checked, as with [print](print.md). Each call is one write, with no buffer between: a text made of
several calls may interleave with what other threads write between them.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    eprint("warning: ");
    eprint("{} of {} files skipped", 2, 5);
    eprint('\n');
}
```

Output:

```text
warning: 2 of 5 files skipped
```

## See also

- [eprintln](eprintln.md): the same and a new line
- [print](print.md): on the standard output, or any writer
- [standard_stream](standard_stream.md): `io::stderr`
