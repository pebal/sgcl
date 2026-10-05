[sgcl](../../README.md) › [txt](../README.md) › [idna](../idna/README.md)

# sgcl::txt::idna::outcome

```cpp
#include "sgcl/txt/idna.h"   // or "sgcl/txt.h"

namespace sgcl::txt::idna {
    struct outcome {
        string text;
        failure reason;

        explicit operator bool() const noexcept;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

What came out of [ascii_form](../idna/ascii_form.md) and [unicode_form](../idna/unicode_form.md), and what was wrong with
it. [UTS #46](https://www.unicode.org/reports/tr46/) converts as far as it can even when it fails, and that text is
worth having: a browser shows the user the name it would not look up, with the label the failure names marked. A
caller that only wants the name uses [to_ascii](../idna/to_ascii.md) and [to_unicode](../idna/to_unicode.md), which hand
back nothing at all when something was wrong.

## Rules

- An aggregate.

## Member objects

| Member | Description |
|---|---|
| `text` | the name as far as it was converted |
| `reason` | what was wrong with it, a [failure](../idna-failure/README.md); its `rule` is `error::none` when nothing was |

## Member functions

| Function | Description |
|---|---|
| [operator bool](operator_bool.md) | checks whether nothing was wrong |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (auto name : {"bücher.de", "bücher.-x-.de"}) {
        auto made = txt::idna::ascii_form(name);
        println("{} -> {} ({})", name, made.text, made ? "fine" : made.reason.message());
    }
}
```

Output:

```text
bücher.de -> xn--bcher-kva.de (fine)
bücher.-x-.de -> xn--bcher-kva.-x-.de (a hyphen in the third and fourth place, or at an end)
```

## See also

- [ascii_form](../idna/ascii_form.md), [unicode_form](../idna/unicode_form.md): what gives one
- [failure](../idna-failure/README.md): what was wrong
- [sgcl::txt::idna](../idna/README.md)
