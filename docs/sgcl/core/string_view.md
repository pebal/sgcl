# sgcl::string_view

```cpp
#include "sgcl/core/string.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class CharT, class Traits = std::char_traits<CharT>>
    class basic_string_view;
    using string_view = basic_string_view<char>;
    using wstring_view = basic_string_view<wchar_t>;
    using u8string_view = basic_string_view<char8_t>;
    using u16string_view = basic_string_view<char16_t>;
    using u32string_view = basic_string_view<char32_t>;
}
```

The same class in the `Sgcl` interface: [StringView](../Sgcl/Core/StringView.md).

`sgcl::string_view` is a view of a [string](string.md) that holds the string's object: two words, the string's word (a `tracked_ptr`, so the object stays alive for as long as the view does) and the range within it, an offset and a length of 32 bits each. A `std::string_view` borrows and leaves the lifetime to the caller; this one owns what it looks at, which a collector makes free: the string's object is immutable, so a view of it is as safe as the string itself, wherever it is kept and however long. What a substring is in Go (a slice of the same array) and a `ReadOnlyMemory<char>` in C#.

What it is for: a piece of a string with no copy: a token, a field, a line of a text, a name inside a path, the pieces of [`split`](string.md#members) (each one is a `string_view`), kept in a container or a managed object as they are. `s.view(pos, n)` is a substring that costs nothing, where `substr` makes a new string; `trim` and its kin on a view are views again. A `string` is made of a view by `string(v)`: the string's own object when the view is the whole of it (no copy), a new string of the characters otherwise.

The interface is the read side of `std::string_view` over the range (`size`, `data`, `[]`, `at`, `front`, `back`, the iterators, `compare`, `starts_with`, `ends_with`, `contains`, the six `find`s, `copy`), `substr` as a view of the same object, `remove_prefix` and `remove_suffix` narrowing the view in place as `std`'s do, `trim`, `trim_left`, `trim_right`, `trim_prefix`, `trim_suffix` as views, the conversion to `std::string_view` (so everything that takes one takes this: `join`, `parse`, the streams, the transparent lookups of the maps), `str()` for a string, the comparisons and `<=>` with a view, a string, a `std::string_view` or a literal, `std::hash` (the hash a string of the characters has, transparent), `operator<<`. A view is made from a string (`s.view()`, `s.view(pos, n)`), from a view (`substr`) or by `split`; there is no constructor from a literal or a `std::string_view`, which hold nothing: make a string of those first.

## Rules

- A `string_view` is a tracked pointer, so it lives where one may: on a stack or inside a managed object, never in `new`/`malloc` memory, a `std` container, a global or a `thread_local` ([The rules](README.md#the-rules), 1). A `std::string_view` is what goes to those places, for as long as some string or view holds the object.
- `data()` is not terminated: a view is a range, not a C string; `str().c_str()` or `std::string` for one.
- Threads share a view the way they share a `tracked_ptr` ([The rules](README.md#the-rules), 6): the object is immutable and read from any thread; a view variable that one thread replaces while others read it needs the program's synchronization (two words: not an atomic).

## Members

```cpp
using value_type = CharT;  using traits_type = Traits;  using size_type = size_t;  using view_type = std::basic_string_view<CharT, Traits>;
using const_iterator = const CharT*;  using const_reverse_iterator = std::reverse_iterator<const_iterator>;   // iterator, reverse_iterator: the same
static constexpr size_type npos;

basic_string_view() noexcept;                             // empty: holds nothing
basic_string_view(const basic_string& s) noexcept;        // the whole of the string
basic_string_view(const basic_string_view&) noexcept;     // the same object and range
basic_string_view& operator=(...) noexcept;

const CharT* data() const noexcept;                       // the characters of the range, not terminated
size_type size() const noexcept;  size_type length() const noexcept;  bool empty() const noexcept;
operator view_type() const noexcept;  view_type view() const noexcept;
basic_string str() const;                                 // the same object for the whole, a copy for a piece
const CharT& operator[](size_type) const noexcept;  const CharT& at(size_type) const;   // at: std::out_of_range
const CharT& front() const noexcept;  const CharT& back() const noexcept;
const_iterator begin() const noexcept;  const_iterator end() const noexcept;   // and cbegin, cend, rbegin, rend, crbegin, crend
size_type copy(CharT* dest, size_type n, size_type pos = 0) const;
int compare(...) const;  bool starts_with(...) const;  bool ends_with(...) const;  bool contains(...) const;   // the overloads of string_view
size_type find(...) const;  rfind, find_first_of, find_last_of, find_first_not_of, find_last_not_of         // the overloads of string_view
basic_string_view substr(size_type pos = 0, size_type n = npos) const;   // a view of the same object; std::out_of_range past the end
void remove_prefix(size_type n) noexcept;  void remove_suffix(size_type n) noexcept;   // the view narrowed, in place
basic_string_view trim() const noexcept;  basic_string_view trim(view_type chars) const noexcept;             // views: without white space (the characters) at both ends
basic_string_view trim_left() const noexcept;  basic_string_view trim_left(view_type chars) const noexcept;   // at the start
basic_string_view trim_right() const noexcept;  basic_string_view trim_right(view_type chars) const noexcept; // at the end
basic_string_view trim_prefix(view_type prefix) const noexcept;  basic_string_view trim_suffix(view_type suffix) const noexcept;
void swap(basic_string_view&) noexcept;
size_t hash() const noexcept;                             // the hash a string of the characters has
const void* object() const noexcept;                      // the string's object: the identity of what is viewed; null when empty
bool operator==(view_type) const noexcept;  bool operator==(const CharT*) const noexcept;
std::strong_ordering operator<=>(view_type) const noexcept;  std::strong_ordering operator<=>(const CharT*) const noexcept;
```

The free functions, in `sgcl`:

```cpp
bool operator==(const basic_string_view&, const basic_string_view&) noexcept;   // the same range of the same object, or the same characters
bool operator==(const basic_string_view&, const basic_string&) noexcept;        // and the reverse, by C++20's rewriting
std::strong_ordering operator<=>(const basic_string_view&, const basic_string_view&) noexcept;  // and with a string
std::basic_ostream& operator<<(std::basic_ostream&, const basic_string_view&);
void swap(basic_string_view&, basic_string_view&) noexcept;
template<...> struct std::hash<sgcl::basic_string_view<...>>;      // transparent: a string, a std::string_view or a literal hashes as the view would
template<...> struct std::equal_to<sgcl::basic_string_view<...>>;  template<...> struct std::less<sgcl::basic_string_view<...>>;   // transparent
```

And on the string: `basic_string_view view() const noexcept` (the whole), `basic_string_view view(size_type pos, size_type n = npos) const` (a range; `std::out_of_range` past the end), `explicit basic_string(const basic_string_view&)`.

```cpp
sgcl::string path = "/usr/local/bin/sgcl";
sgcl::string_view name = path.view(path.rfind('/') + 1);   // "sgcl": two words, no copy, the path's object held
assert(name == "sgcl" && name.object() == path.object());
sgcl::string_view dir = path.view(0, path.rfind('/'));    // "/usr/local/bin"
sgcl::vector<sgcl::string_view> parts(dir.str().split('/'));   // views into the directory string, kept safely
assert(parts.size() == 4 && parts[1] == "usr");
sgcl::string copy(name);                                  // a new string of the piece
sgcl::string same(path.view());                           // the whole: the same object
assert(copy.object() != path.object() && same.object() == path.object());
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

// A record kept as views into the line it was read from: one object for
// the line, three views of it, nothing copied and nothing dangling
struct Record {
    sgcl::string_view name;
    sgcl::string_view unit;
    sgcl::string_view value;
};

sgcl::tracked_ptr<Record> parse_record(const sgcl::string& line) {
    sgcl::tracked_ptr record = sgcl::make_tracked<Record>();
    sgcl::vector<sgcl::string_view> fields(line.split(';'));
    record->name = fields[0].trim();
    record->unit = fields[1].trim();
    record->value = fields[2].trim();
    return record;
}

int main() {
    sgcl::vector<sgcl::tracked_ptr<Record>> records;
    for (const char* text : {"temperature ; C ; 21.5", " pressure; hPa ;1013"}) {
        sgcl::string line = text;                     // the line's object outlives this loop: the views hold it
        records.push_back(parse_record(line));
    }
    // Optional: the collector runs its cycles by itself; forced here only
    // to show that the lines the views hold are not collected
    sgcl::collector::force_collect(true);
    for (const auto& record : records) {
        std::cout << record->name << " = " << *sgcl::parse<double>(record->value) << " " << record->unit << "\n";
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

- [string](string.md): what a view is of; [split](string.md#members): the pieces as views
- [tracked_ptr](tracked_ptr.md): the word a view holds the object by
- README: [string](README.md#string), [The rules](README.md#the-rules)
