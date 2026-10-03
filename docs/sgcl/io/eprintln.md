[sgcl](../README.md) › [io](README.md)

# sgcl::io::eprintln

```cpp
#include "sgcl/io/print.h"   // or "sgcl/io.h"

namespace sgcl::io {
    /*(1)*/ template<class... A>
            void eprintln(const txt::format_pattern<std::type_identity_t<A>...>& pattern,
                          const A&... args);
    /*(2)*/ void eprintln();
    /*(3)*/ template<class T>
            void eprintln(const T& value);
}

namespace sgcl {
    using io::eprintln;
}
```

Writes text and a new line on the standard error in one call: [println](println.md) on `io::stderr`. The pattern is
[txt::format](../txt/format.md)'s, read by the compiler. The name is `sgcl`'s as well as `io`'s. Go's
`fmt.Fprintln(os.Stderr, ...)` without its result; what a program prints before it ends with a failure status.

1. The text of `pattern` and `args`, and a new line, on `io::stderr`.
2. A new line alone on `io::stderr`.
3. One value, as `"{}"` formats it, and a new line: `eprintln(e.message())`. Takes part only for a value `"{}"`
   formats that is not an array and not a writer: a literal is always the pattern.

## Parameters

| Parameter | Description |
|---|---|
| `pattern` | the pattern of [txt::format](../txt/format.md), a literal |
| `args` | the values of the fields |
| `value` | the one value to print |

## Return value

None.

## Complexity

Linear in the length of the text: the text and its new line are made as one string, once, and written in one
write.

## Exceptions

- `out_of_range` when a `{:c}` field is given a number no character holds.
- `length_error` when the text passes 4 GiB, the most a string holds.
- What the formatting of a value of the program's throws.
- What the write to `io::stderr` throws, [file::write](file/write.md)'s: `std::system_error` when the write would
  wait on the reactor and its thread cannot be started.

## Notes

Printing is not checked, as with [print](print.md). A line is made with its new line and written in one call.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto settings = io::read_text("settings.ini");
    if (!settings) {
        eprintln("cannot read the settings: {}", settings.error().message());
        eprintln(settings.error().is_not_found() ? "using the defaults" : "giving up");
        eprintln();
    }
}
```

Output:

```text
cannot read the settings: open settings.ini: No such file or directory
using the defaults

```

## See also

- [eprint](eprint.md): the same without the new line
- [println](println.md): on the standard output, or any writer
- [error](error.md): what an operation reports
- [standard_stream](standard_stream.md): `io::stderr`
