[sgcl](../../README.md) › [txt](../README.md) › [stencil_error](README.md)

# sgcl::txt::stencil_error::message

```cpp
string message() const noexcept;
```

Why the source is not a template, in a few words: "the action is not closed", "a block was left open", "no function
of that name", "that is not a specification", "too many arguments to one function", and the like.

## Parameters

None.

## Return value

The reason, as a string.

## Complexity

Linear in its length.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (auto source : {"{{ end }}", "{{ x | shout }}", "{{ }}",
                        "{{ if a }}{{ else }}{{ else }}{{ end }}"}) {
        println("{}", txt::stencil::parse(source).error().message());
    }
    return 0;
}
```

Output:

```text
an end with nothing open
no function of that name
an empty action
a second else
```

## See also

- [sgcl::txt::stencil_error](README.md)
