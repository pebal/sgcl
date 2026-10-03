[sgcl](../../README.md) › [slog](../README.md) › [message](../message.md)

# sgcl::slog::message::message

```cpp
/*(1)*/ template<class C>
        requires std::same_as<C, const char*> || std::same_as<C, char*>
        message(C text,
                std::source_location where = std::source_location::current()) noexcept;
/*(2)*/ template<size_t N>
        message(const char (&text)[N],
                std::source_location where = std::source_location::current()) noexcept;
/*(3)*/ message(const string& text,
                std::source_location where = std::source_location::current()) noexcept;
/*(4)*/ message(const std::string& text,
                std::source_location where = std::source_location::current()) noexcept;
/*(5)*/ message(const slice<const char>& text,
                std::source_location where = std::source_location::current()) noexcept;
```

Constructs a message that refers to `text`, at the place `where`. Not explicit: a verb's first argument converts.

1. A C string, to its first NUL; a null pointer is the empty text.
2. A literal or a character array, to its first NUL and never past its end.
3. A [string](../../core/string.md).
4. A `std::string`.
5. A text slice.

- (1–5) `where` is, by default, the place the message is made: the call of the verb that takes it.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text of the record; referred to, not copied |
| `where` | the place of the call; the call itself by default |

## Complexity

- (1) Linear in the length of the text.
- (2) Linear in `N`.
- (3–5) Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

#include <string>

using namespace sgcl;

int main() {
    slog::logger log(io::stdout);
    const char* none = nullptr;
    char buffer[16] = "in a buffer";
    log.info(none, "n", 1);
    log.info(buffer);
    log.info(std::string("std::string"));
    slog::message m("made apart");
    println("{} {}", m.text(), m.where().line());
}
```

Output:

```text
time=2026-09-28T14:05:01.123+02:00 level=INFO msg="" n=1
time=2026-09-28T14:05:01.123+02:00 level=INFO msg="in a buffer"
time=2026-09-28T14:05:01.123+02:00 level=INFO msg=std::string
made apart 15
```

## See also

- [text](text.md), [where](where.md)
- [sgcl::slog::message](../message.md)
