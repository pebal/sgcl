[sgcl](../../README.md) › [txt](../README.md) › [stencil](../stencil.md)

# sgcl::txt::stencil::stencil

```cpp
stencil() = default;                                                           // (1)
explicit stencil(const string& source);                                        // (2)
explicit stencil(const string& source, const stencil_functions& functions);    // (3)
```

1. An empty template, which renders an empty page.
2. The template a literal of the program spells, with the six functions every template has: the value of
   [parse](parse.md), or `bad_expected_access<stencil_error>` with parse's error and its message. A source read from
   outside the program — a file, a setting — is parsed; one the program itself wrote is constructed, and a slip in
   it is the mistake of the program, found the first time the line runs.
3. The same with the program's own table of [functions](../stencil_functions.md), whose functions the template keeps.

## Parameters

| Parameter | Description |
|---|---|
| `source` | the text of the template |
| `functions` | the functions its pipelines may call |

## Complexity

- (1) Constant.
- (2–3) Linear in the length of `source`.

## Exceptions

- (2–3) `bad_expected_access<stencil_error>` when `source` is not a template; its `error()` says where and why, its
  `what()` is the message.
- (3) What the copy of a function of the program's table throws.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::stencil greeting("Hello, {{ name }}!");
    println("{}", greeting.render(txt::object{{"name", "Ada"}}));
    try {
        txt::stencil broken("Hello, {{ if name }}!");
    } catch (const bad_expected_access<txt::stencil_error>& e) {
        println("{}:{}: {}", e.error().line(), e.error().column(), e.what());
    }
    return 0;
}
```

Output:

```text
Hello, Ada!
1:11: a block was left open
```

## See also

- [parse](parse.md): a source that may be wrong
- [sgcl::txt::stencil](../stencil.md)
