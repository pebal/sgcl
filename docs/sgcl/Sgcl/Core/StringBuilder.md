# StringBuilder

```cpp
#include "sgcl/Sgcl/Core/StringBuilder.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class CharT, class Traits = std::char_traits<CharT>>
    class BasicStringBuilder;
    using StringBuilder = BasicStringBuilder<char>;
    using WStringBuilder = BasicStringBuilder<wchar_t>;
}
```

No counterpart in the `sgcl` interface: there the buffer is `std::string` itself.

`StringBuilder` is the scratch buffer a [`String`](String.md) is built in: appended to piece by piece (a character, a literal, a view, a `String`, a number), then made into a `String` with `ToString()`, once. A `String` is immutable, one managed object shared by copying the word; what is not yet final belongs in a buffer, and this is that buffer under the interface's names: a `std::string` inside (`Inner()`), unmanaged memory with no tracked pointer in it, so a builder lives anywhere, in a `std` container or a thread's closure included, and costs the collector nothing. `ToString()` copies the characters into one managed object; the buffer stays as it was, to build on or to `Clear()`.

## Rules

- No tracked pointer inside: the builder is not the collector's business, and lives anywhere ([The rules](../../core/README.md#the-rules) do not apply to it).
- `View()` is valid until the next `Append`, as a view of a `std::string` is; `ToString()` is a copy, and the `String` it gives is independent of the builder from then on.
- Thread safety is `std::string`'s: one writer.

## Members

```cpp
BasicStringBuilder();
explicit BasicStringBuilder(ViewType s);              // starts with s
explicit BasicStringBuilder(InnerType s) noexcept;    // takes a std::string over

BasicStringBuilder& Append(CharT c);
BasicStringBuilder& Append(SizeType n, CharT c);
BasicStringBuilder& Append(ViewType s);               // a view, a literal
BasicStringBuilder& Append(const CharT* s);
BasicStringBuilder& Append(const StringType& s);
template<class T> BasicStringBuilder& Append(T number);   // as ToString(number)
template<class T> BasicStringBuilder& operator<<(const T& v);   // Append, for a chain

SizeType Length() const noexcept;  bool IsEmpty() const noexcept;
void Reserve(SizeType n);  void Clear() noexcept;
ViewType View() const noexcept;                       // the characters so far
StringType ToString() const;                          // the String, made now
InnerType& Inner() noexcept;                          // the std::string
```

```cpp
StringBuilder b;
b.Append("item ").Append(7).Append(": ").Append(String("ready"));
b << " (" << 2.5 << ")";
String s = b.ToString();                              // "item 7: ready (2.5)"
b.Clear();
```

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

// A report assembled line by line in a builder, made into one String at
// the end and kept in a managed object: the lines are the buffer's
// business, the result the collector's.
struct Report {
    String text;
};

int main() {
    StringBuilder b;
    for (int i : Range(1, 4)) {
        b.Append("line ").Append(i).Append('\n');
    }
    Ptr report = Make<Report>(b.ToString());
    std::cout << report->text;                        // line 1 / line 2 / line 3
    std::cout << b.Length() << " characters\n";       // 21 characters
    return report->text.Length() == 21 ? 0 : 1;
}
```

The output:

```
line 1
line 2
line 3
21 characters
```

## See also

- [String](String.md): what it builds; [ToString](String.md): a number as a String
- `tests/Sgcl/sgcl.cpp`: the behaviour above, checked.
