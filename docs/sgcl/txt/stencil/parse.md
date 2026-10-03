[sgcl](../../README.md) › [txt](../README.md) › [stencil](../stencil.md)

# sgcl::txt::stencil::parse

```cpp
/*(1)*/ static expected<stencil, stencil_error> parse(const string& source) noexcept;
/*(2)*/ static expected<stencil, stencil_error> parse(const string& source,
                                                      const stencil_functions& functions);
```

Reads `source` into a template, in one pass, left to right: the runs of literal text, the fields, the blocks and
their jumps, every [specification](../format.md#the-specification) read by `format`'s own reader and every function
of a pipeline found in the table. Everything that can be settled where the source is read is settled here — the
shape of every specification, that every function named is one the table knows, that every block is closed, that no
jump goes nowhere — and what is not a template is the error: where the reading stopped and why
([stencil_error](../stencil_error.md)). The [syntax](../stencil.md#the-syntax) is on the page of the class.

1. With the six functions every template has ([stencil_functions](../stencil_functions/builtin.md)).
2. With the program's own table: the template keeps the functions it calls, so the table need not outlive it.

## Parameters

| Parameter | Description |
|---|---|
| `source` | the text of the template |
| `functions` | the functions its pipelines may call |

## Return value

The template, or the error that says where and why `source` is not one.

## Complexity

Linear in the length of `source`, and a look-up in the table for every function named.

## Exceptions

- (1) None.
- (2) What the copy of a function of the program's table throws.

## Example

Parsed once, rendered as often as there is data:

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string source = "Hello, {{ name }}!";
    auto t = txt::stencil::parse(source);
    if (!t) {
        println("{}", t.error().message());  // the source is not a template
        return 1;
    }
    for (auto name : {"Ada", "Alan"}) {
        println("{}", t->render(txt::object{{"name", name}}));
    }
    return 0;
}
```

Output:

```text
Hello, Ada!
Hello, Alan!
```

A source that is not a template, answered with where and why:

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (auto source : {"Hello, {{ name ", "{{ if a }}yes", "{{ name | shout }}",
                        "{{ n:>70000 }}"}) {
        auto t = txt::stencil::parse(source);
        if (!t) {
            println("{}:{}: {}", t.error().line(), t.error().column(), t.error().message());
        }
    }
    return 0;
}
```

Output:

```text
1:8: the action is not closed
1:4: a block was left open
1:11: no function of that name
1:6: that is not a specification
```

## See also

- [stencil](stencil.md): the constructor, for a literal of the program's own
- [parses](parses.md): whether a source parses, with nothing kept
- [stencil_error](../stencil_error.md): where and why
- [sgcl::txt::stencil](../stencil.md)
