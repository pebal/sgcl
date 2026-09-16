# sgcl::string

```cpp
#include "sgcl/string.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class CharT, class Traits = std::char_traits<CharT>>
    class basic_string;
    using string = basic_string<char>;
    using wstring = basic_string<wchar_t>;
    using u8string = basic_string<char8_t>;
    using u16string = basic_string<char16_t>;
    using u32string = basic_string<char32_t>;
}
```

`sgcl::string` is an immutable string on the managed heap: one word, a pointer to an object holding the length, the hash once something has asked for it (eight bytes together), the characters and a terminator, of exactly that size rounded to four: a string of ten characters is an object of 20 bytes. What a string is in Java or Go rather than in C++: made once, never modified, shared by copying the word, compared and hashed by its contents, reclaimed by the collector when nothing holds it, with no destructor (the sweep frees the slot and runs nothing) and no reference count. The empty string is null and allocates nothing. There is no small-string optimization: the word is the whole of the string, and a string of any length costs the same to copy.

What it is for: text that is kept, shared and compared. Copying one between managed objects costs a word and the write barrier; a `std::string` past its small buffer costs an allocation per copy and a `free` per destruction, in the sweep. Hashing one as a map key costs a load after the first time: the hash (`std::hash` of the characters) is computed once and kept in the string's object, as Java's `String` keeps its `hashCode`. What it is not for: a scratch buffer, or text of a few characters made and dropped at once, where `std::string` costs no allocation at all; `std::string` remains the right member for those, in a managed object as anywhere ([README: string](../README.md#string) has the numbers).

The interface is the read side of `std::string` and all of `std::string_view`: `size`, `data`, `c_str`, `[]`, `at`, `front`, `back`, the iterators, `compare`, `starts_with`, `ends_with`, `contains`, the six `find`s, `substr` (a new string, or the same object for the whole), the comparisons and `<=>` with a string, a `string_view` or a literal, `operator+` (a new string), `std::hash`, `operator<<`, the conversions to `string_view` and to `std::string` (`str()`); the constructors from a literal, `(s, n)`, a `string_view`, a `std::string` or anything a `string_view` is made of, `(n, ch)`, a range, an initializer list. No mutation, no `capacity`: a string is built as a `std::string` or a `string_view` and made once. The length is kept in 32 bits.

The word is a `tracked_ptr`, so a string lives where one may, as the containers do: on a stack or inside a managed object.

## Rules

- A `string` is a tracked pointer, so it lives where one may: on a stack or inside a managed object ([The rules](../README.md#the-rules), 1).
- Threads share a `string` the way they share a `tracked_ptr` ([The rules](../README.md#the-rules), 6): the object itself is immutable and read from any thread without synchronization, and a string variable that one thread replaces while others read it is an [`atomic<string>`](atomic.md#atomicbasic_string), one word, a load for the string as it was and a store for a new one. The hash is computed by the first thread that asks and stored relaxed: every thread computes the same value.
- `data()` and the iterators are valid while some string holds the object: a `string_view` taken from a temporary dangles as it would from a `std::string`.
- A string's object is never traced and never zeroed: its bytes are characters and nothing else.

## Members

```cpp
using value_type = CharT;  using traits_type = Traits;  using size_type = size_t;  using view_type = std::basic_string_view<CharT, Traits>;
using const_iterator = const CharT*;  using const_reverse_iterator = std::reverse_iterator<const_iterator>;   // iterator, reverse_iterator: the same
static constexpr size_type npos;

basic_string() noexcept;                                  // empty: null, nothing allocated
basic_string(const CharT* s);
basic_string(const CharT* s, size_type n);
basic_string(view_type s);
template<class V> explicit basic_string(const V& v);     // anything a view_type is made of: a std::string
basic_string(size_type n, CharT c);
template<std::input_iterator It> basic_string(It first, It last);
basic_string(std::initializer_list<CharT>);
basic_string(const basic_string&) noexcept;               // the same object
basic_string& operator=(...);                             // the same set

const CharT* data() const noexcept;  const CharT* c_str() const noexcept;   // terminated; the empty string's is a terminator
size_type size() const noexcept;  size_type length() const noexcept;  bool empty() const noexcept;
operator view_type() const noexcept;  view_type view() const noexcept;
std::basic_string<CharT, Traits> str() const;             // a copy, to build on
const CharT& operator[](size_type) const noexcept;  const CharT& at(size_type) const;   // at: std::out_of_range
const CharT& front() const noexcept;  const CharT& back() const noexcept;
const_iterator begin() const noexcept;  const_iterator end() const noexcept;   // and cbegin, cend, rbegin, rend, crbegin, crend
size_type copy(CharT* dest, size_type n, size_type pos = 0) const;
int compare(...) const;  bool starts_with(...) const;  bool ends_with(...) const;  bool contains(...) const;   // the overloads of string_view
size_type find(...) const;  rfind, find_first_of, find_last_of, find_first_not_of, find_last_not_of         // the overloads of string_view
basic_string substr(size_type pos = 0, size_type n = npos) const;   // a new string; the same object for the whole
void swap(basic_string&) noexcept;
size_t hash() const noexcept;                             // std::hash of the characters, computed once, kept in the object
const void* object() const noexcept;                      // the object's address: the identity; null when empty
bool equals(const basic_string&) const noexcept;   // what == does
bool operator==(view_type) const noexcept;  bool operator==(const CharT*) const noexcept;
std::strong_ordering operator<=>(view_type) const noexcept;  std::strong_ordering operator<=>(const CharT*) const noexcept;
```

The free functions, in `sgcl`:

```cpp
bool operator==(const basic_string<CharT, Traits>&, const basic_string<CharT, Traits>&) noexcept;   // the same object, or the lengths, the hashes when known, the characters
std::strong_ordering operator<=>(const basic_string<CharT, Traits>&, const basic_string<CharT, Traits>&) noexcept;
basic_string operator+(string, string);  (string, view);  (view, string);  (string, const CharT*);  (const CharT*, string);  (string, CharT);  (CharT, string);
std::basic_ostream& operator<<(std::basic_ostream&, const basic_string&);
void swap(basic_string&, basic_string&) noexcept;
template<...> struct std::hash<sgcl::basic_string<...>>;   // the cached hash
```

The equality compares the words first (a copy is the same object), then the lengths, then the hashes when both have been computed (different hashes: unequal, without reading the characters), then the characters.

```cpp
sgcl::string name = "alice";                  // one object: 8 bytes of length and hash, six characters, in a slot of 16
sgcl::string same = name;                     // the same object
sgcl::string other("alice");                  // another object, equal contents
assert(same.object() == name.object() && other == name && other.object() != name.object());
assert(name.starts_with("al") && name.substr(1, 3) == "lic" && name + "!" == "alice!");
std::unordered_map<sgcl::string, int> ages;     // sgcl::string in a std container, the hash kept in the string's object
ages[sgcl::string(name)] = 30;
assert(ages[sgcl::string("alice")] == 30);
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

// A document tree whose element names are shared: every <p> holds the
// same string object, and a name compares by the word before it compares
// by the characters. Nothing is freed by hand, nothing is counted.
struct Element {
    sgcl::string name;
    sgcl::string text;
    sgcl::vector<sgcl::tracked_ptr<Element>> children;
};

sgcl::tracked_ptr<Element> make(sgcl::string name, sgcl::string text = {}) {
    sgcl::tracked_ptr e = sgcl::make_tracked<Element>();
    e->name = name;                             // a word: the name's object is shared
    e->text = text;
    return e;
}

int count(const sgcl::tracked_ptr<Element>& e, const sgcl::string& name) {
    int n = e->name == name ? 1 : 0;            // the same object: a comparison of two words
    for (auto& child : e->children) {
        n += count(child, name);
    }
    return n;
}

int main() {
    sgcl::string p = "p", div = "div";            // the names, made once
    sgcl::tracked_ptr root = make(div);
    for (int i = 0; i < 3; ++i) {
        sgcl::tracked_ptr section = make(div);
        for (int j = 0; j < 4; ++j) {
            section->children.push_back(make(p, "paragraph " + sgcl::string(std::to_string(j))));
        }
        root->children.push_back(section);
    }
    std::cout << count(root, p) << " paragraphs, " << count(root, "div") << " divs\n";   // 12 paragraphs, 4 divs
    std::cout << root->children[0]->children[1]->text << "\n";                            // paragraph 1
    return 0;
}
```

## See also

- [tracked_ptr](tracked_ptr.md): the word; [unordered_map](unordered_map.md), [map](map.md): the containers a string keys
- README: [string](../README.md#string), [The rules](../README.md#the-rules), [Allocation](../README.md#allocation)
- `benchmarks/string.cpp`: the cost against `std::string`; `tests/string.cpp`: every behaviour above, checked.
