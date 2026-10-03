[sgcl](../README.md) › [txt](README.md) › [idna](idna.md)

# sgcl::txt::idna::failure

```cpp
#include "sgcl/txt/idna.h"   // or "sgcl/txt.h"

namespace sgcl::txt::idna {
    struct failure {
        static constexpr size_t whole_name = size_t(-1);
        error rule = error::none;
        size_t label = 0;
        size_t at = 0;
        size_t size = 0;

        string message() const noexcept;
    };
}
```

What was wrong with a name, and where: the error of [to_ascii](idna/to_ascii.md) and
[to_unicode](idna/to_unicode.md), and the `reason` of an [outcome](idna-outcome.md). A name is refused for something
**one of its labels** did, and a caller who has to show the name needs to know which: a browser underlines the label,
it does not grey out the address bar. So the failure carries the label, which one it is counting from zero, and the
bytes it took up **in the text that was handed in**, which are not the bytes of the converted text and are what a
caller can point at.

## Rules

- The range is of the label **as it arrived**, so it covers what was mapped away and what was ignored: in
  `a.-b­.com` the label at fault is the four bytes of `-b` and the soft hyphen, even though the soft hyphen leaves no
  trace in the answer; and in `a。-b.com` the second label begins after the three bytes of the ideographic full stop,
  not after the one byte of the stop it becomes.
- `name_too_long` is the one failure that is no label's fault, and it says so with `label == failure::whole_name`;
  its range is then the whole name.
- The bytes are found by a second pass over the name, made only once the name has already failed: a name that is
  fine pays nothing for them.
- An aggregate, trivially copyable.

## Member objects

| Member | Description |
|---|---|
| `whole_name` | `size_t(-1)`, the `label` of a failure of the whole name, `static constexpr size_t` |
| `rule` | the first rule the name broke, an [error](idna-error.md); `error::none` by default |
| `label` | which label, counting from zero; `whole_name` for `name_too_long` |
| `at` | where that label begins in the text that was given, in bytes |
| `size` | how many bytes of that text the label took up |

## Member functions

| Function | Description |
|---|---|
| [message](idna-failure/message.md) | the rule, in words |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (auto name : {"a.-b\u00AD.com", "a。-b.com"}) {
        auto host = txt::idna::to_ascii(name);
        auto why = host.error();
        println("label {}, bytes {} to {}", why.label, why.at, why.at + why.size);
    }
    auto huge = txt::idna::to_ascii(string("a.").repeat(130) + "com");
    println("{}: {}", huge.error().message(), huge.error().label == txt::idna::failure::whole_name);
}
```

Output:

```text
label 1, bytes 2 to 6
label 1, bytes 4 to 6
a name longer than 253 bytes: true
```

## See also

- [error](idna-error.md): the rules
- [outcome](idna-outcome.md): the text and the failure
- [sgcl::txt::idna](idna.md)
