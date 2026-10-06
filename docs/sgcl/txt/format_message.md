[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::format_message

```cpp
#include "sgcl/txt/message_format.h"   // or "sgcl/txt.h"

expected<string, message_error> format_message(const string& pattern, const locale& l,
                                               const value& args) noexcept;
```

Reads a message for a locale and writes it with the arguments, once: the one-line form of
[message_format](message_format/README.md).

## Parameters

| Parameter | Description |
|---|---|
| `pattern` | the message |
| `l` | the locale |
| `args` | the arguments: an object or a list |

## Return value

The text, or the [message_error](message_error/README.md) of [parse](message_format/parse.md).

## Complexity

Linear in the lengths of the pattern and of the text.

## Exceptions

None that the program can cause; running out of memory ends it.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto r =
        txt::format_message("{0} is {1, selectordinal, one {#st} two {#nd} few {#rd} other {#th}}.",
                            txt::locale("en"), txt::list{"Ada", 2});
    println("{}", *r);
}
```

Output:

```text
Ada is 2nd.
```

## See also

- [message_format](message_format/README.md)
- [sgcl::txt](README.md)
