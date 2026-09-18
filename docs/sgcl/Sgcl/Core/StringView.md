# Sgcl::StringView

```cpp
#include "sgcl/Sgcl/Core/String.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class CharT, class Traits = std::char_traits<CharT>>
    class BasicStringView;
    using StringView = BasicStringView<char>;
    using WStringView = BasicStringView<wchar_t>;
    using U8StringView = BasicStringView<char8_t>;
    using U16StringView = BasicStringView<char16_t>;
    using U32StringView = BasicStringView<char32_t>;
}
```

The same class in the `sgcl` interface: [string_view](../../core/string_view.md).

`StringView` is a view of a [String](String.md) that holds the string's object: two words, the string's word (a `Ptr`, so the object stays alive for as long as the view does) and the range within it. A `std::string_view` borrows and leaves the lifetime to the caller; this one owns what it looks at, which the collector makes free: the string's object is immutable, so a view of it is as safe as the string itself, wherever it is kept. What a substring is in Go and a `ReadOnlyMemory<char>` in C#.

What it is for: a piece of a String with no copy: a token, a field, a line, a name inside a path, the pieces of [`Split`](String.md#members) (each one converts to a `StringView`), kept in a `List` or a managed object as they are. `s.View(pos, n)` is a substring that costs nothing, where `Substring` makes a new String; `Trim` and its kin on a view are views again. A String is made of a view by `String(v)` or `v.ToString()`: the string's own object when the view is the whole of it, a new String of the characters otherwise.

The interface is the read side of a String over the range (`Length`, `Data`, `[]`, `First`, `Last`, `Compare`, `StartsWith`, `EndsWith`, `Contains`, `IndexOf`, `LastIndexOf`, `IndexOfAny`, `LastIndexOfAny`, `CopyTo`), `Substring` as a view of the same object, `RemovePrefix` and `RemoveSuffix` narrowing the view in place, `Trim`, `TrimStart`, `TrimEnd`, `TrimPrefix`, `TrimSuffix` as views, the conversion to `std::string_view` (so everything that takes one takes this: `Join`, `Parse`, the streams, the transparent lookups of the dictionaries), `ToString`, `ToStd`, the comparisons and `<=>` with a view, a String, a `std::string_view` or a literal, `std::hash`, `operator<<`. A view is made from a String (`View()`, `View(pos, n)`), from a view (`Substring`) or by `Split`; there is no constructor from a literal, which holds nothing.

## Rules

- A `StringView` is a tracked pointer, so it lives where one may: on a stack or inside a managed object, never in `new`/`malloc` memory, a `std` container, a global or a `thread_local` ([The rules](../../core/README.md#the-rules), 1).
- `Data()` is not terminated: a view is a range, not a C string.
- Threads share a view the way they share a `Ptr` ([The rules](../../core/README.md#the-rules), 6): the object is immutable and read from any thread; a view variable one thread replaces while others read it needs the program's synchronization.

## Members

```cpp
using CharType = CharT;  using SizeType = size_t;  using ViewType = std::basic_string_view<CharT, Traits>;
using InnerType = sgcl::basic_string_view<CharT, Traits>;  using StringType = BasicString<CharT, Traits>;
static constexpr SizeType NoPosition;

BasicStringView() noexcept;                               // empty: holds nothing
BasicStringView(const StringType& s) noexcept;            // the whole of the string
BasicStringView(InnerType v) noexcept;
BasicStringView(const BasicStringView&) noexcept;         // the same object and range

const CharT* Data() const noexcept;                       // the characters of the range, not terminated
SizeType Length() const noexcept;  bool IsEmpty() const noexcept;
operator ViewType() const noexcept;  ViewType View() const noexcept;
StringType ToString() const;                              // the same object for the whole, a copy for a piece
std::basic_string<CharT, Traits> ToStd() const;
const CharT& operator[](SizeType) const noexcept;
const CharT& First() const noexcept;  const CharT& Last() const noexcept;
BasicStringView Substring(SizeType pos = 0, SizeType n = NoPosition) const;   // a view of the same object; std::out_of_range past the end
void RemovePrefix(SizeType n) noexcept;  void RemoveSuffix(SizeType n) noexcept;   // the view narrowed, in place
SizeType IndexOf(ViewType s, SizeType from = 0) const noexcept;  SizeType IndexOf(CharT c, SizeType from = 0) const noexcept;
SizeType LastIndexOf(ViewType s, SizeType from = NoPosition) const noexcept;  SizeType LastIndexOf(CharT c, SizeType from = NoPosition) const noexcept;
SizeType IndexOfAny(ViewType chars, SizeType from = 0) const noexcept;  SizeType LastIndexOfAny(ViewType chars, SizeType from = NoPosition) const noexcept;
bool Contains(ViewType) const noexcept;  bool Contains(CharT) const noexcept;
bool StartsWith(ViewType) const noexcept;  bool StartsWith(CharT) const noexcept;
bool EndsWith(ViewType) const noexcept;  bool EndsWith(CharT) const noexcept;
BasicStringView Trim() const noexcept;  BasicStringView Trim(ViewType chars) const noexcept;             // views: without white space (the characters) at both ends
BasicStringView TrimStart() const noexcept;  BasicStringView TrimStart(ViewType chars) const noexcept;   // at the start
BasicStringView TrimEnd() const noexcept;  BasicStringView TrimEnd(ViewType chars) const noexcept;       // at the end
BasicStringView TrimPrefix(ViewType prefix) const noexcept;  BasicStringView TrimSuffix(ViewType suffix) const noexcept;
int Compare(ViewType) const noexcept;
size_t Hash() const noexcept;                             // the hash a String of the characters has
const void* Object() const noexcept;                      // the string's object; null when empty
SizeType CopyTo(CharT* dest, SizeType n, SizeType pos = 0) const;
void Swap(BasicStringView&) noexcept;
InnerType& Inner() noexcept;  const InnerType& Inner() const noexcept;
```

The free functions, in `Sgcl`:

```cpp
bool operator==(const BasicStringView&, const BasicStringView&) noexcept;   // and with a String, a ViewType, a const CharT*
std::strong_ordering operator<=>(const BasicStringView&, const BasicStringView&) noexcept;   // and with a String, a ViewType, a const CharT*
std::basic_ostream& operator<<(std::basic_ostream&, const BasicStringView&);
void swap(BasicStringView&, BasicStringView&) noexcept;
const CharT* begin(const BasicStringView&) noexcept;  const CharT* end(const BasicStringView&) noexcept;   // a range-for over the characters
template<...> struct std::hash<BasicStringView<...>>;   // transparent: a String, a std::string_view or a literal hashes as the view would
template<...> struct std::equal_to<BasicStringView<...>>;  template<...> struct std::less<BasicStringView<...>>;   // transparent
```

And on the String: `BasicStringView View() const noexcept` (the whole), `BasicStringView View(SizeType pos, SizeType n = NoPosition) const` (a range), `explicit BasicString(const BasicStringView&)`.

```cpp
String path = "/usr/local/bin/sgcl";
StringView name = path.View(path.LastIndexOf('/') + 1);   // "sgcl": two words, no copy, the path's object held
assert(name == "sgcl" && name.Object() == path.Object());
StringView dir = path.View(0, path.LastIndexOf('/'));     // "/usr/local/bin"
List<StringView> parts(dir.ToString().Split('/'));        // views into the directory string, kept safely
assert(parts.Count() == 4 && parts[1] == "usr");
String copy(name);                                        // a new String of the piece
String same(path.View());                                 // the whole: the same object
assert(copy.Object() != path.Object() && same.Object() == path.Object());
```

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

// A record kept as views into the line it was read from: one object for
// the line, three views of it, nothing copied and nothing dangling
struct Record {
    StringView name;
    StringView unit;
    StringView value;
};

Ptr<Record> ParseRecord(const String& line) {
    Ptr record = Make<Record>();
    List<StringView> fields(line.Split(';'));
    record->name = fields[0].Trim();
    record->unit = fields[1].Trim();
    record->value = fields[2].Trim();
    return record;
}

int main() {
    List<Ptr<Record>> records;
    for (const char* text : {"temperature ; C ; 21.5", " pressure; hPa ;1013"}) {
        String line = text;                           // the line's object outlives this loop: the views hold it
        records.Add(ParseRecord(line));
    }
    // Optional: the collector runs its cycles by itself; forced here only
    // to show that the lines the views hold are not collected
    Collector::Collect(true);
    for (const auto& record : records) {
        std::cout << record->name << " = " << *Parse<double>(record->value) << " " << record->unit << "\n";
    }
    return 0;
}
```

The output:

```
temperature = 21.5 C
pressure = 1013 hPa
```

## See also

- [String](String.md): what a view is of; [Split](String.md#members): the pieces as views
- [Ptr](Ptr.md): the word a view holds the object by
- README: [String](../../core/README.md#string), [The rules](../../core/README.md#the-rules)
