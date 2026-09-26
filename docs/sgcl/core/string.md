# sgcl::string

```cpp
#include "sgcl/core/string.h"   // or "sgcl/sgcl.h"

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

What it is for: text that is kept, shared and compared. Copying one between managed objects costs a word and the write barrier; a `std::string` past its small buffer costs an allocation per copy and a `free` per destruction, in the sweep. Hashing one as a map key costs a load after the first time: the hash (`std::hash` of the characters) is computed once and kept in the string's object, as Java's `String` keeps its `hashCode`. What it is not for: a scratch buffer, or text of a few characters made and dropped at once, where `std::string` costs no allocation at all; `std::string` remains the right member for those, in a managed object as anywhere ([README: string](README.md#string) has the numbers).

The interface is the read side of `std::string` and all of `std::string_view`: `size`, `data`, `c_str`, `[]`, `at`, `front`, `back`, the iterators, `compare`, `starts_with`, `ends_with`, `contains`, the six `find`s, `substr` (a new string, or the same object for the whole), the comparisons and `<=>` with a string, a `string_view` or a literal, `operator+` (a new string), `std::hash`, `operator<<`, the conversions to `string_view` and to `std::string` (`str()`); the constructors from a literal, `(s, n)`, a `string_view`, a `std::string` or anything a `string_view` is made of, `(n, ch)`, a range, an initializer list. No mutation, no `capacity`: a string is built as a `std::string` or a `string_view` and made once. The length is kept in 32 bits: `max_size()` is 4 294 967 295 characters, and a longer text is refused with `length_error` before a character of it is read, as `std::string` refuses what passes its own.

The hash is keyed: a hash of the characters under a key of four words drawn once per process (detail/hash_bytes.h), so which texts share a bucket is not known outside the process, and a map keyed by what a client sends — the names of HTTP headers, the keys of a JSON object — cannot be filled with keys of one bucket (HashDoS; Go and Rust key their maps the same way). The order of a `map` or a `set` of strings therefore differs between runs; the environment variable `SGCL_HASH_SEED`, a number, fixes the key, for a test that prints one. It is faster than the standard library's unkeyed hash at every length (2.4 ns for three bytes, 4.2 ns for a hundred, 10 GB/s over a long text) and is computed once per string, which keeps it. A `std::string` key hashes by `std::hash<std::string>`, which is not keyed: a map of untrusted keys is keyed by `sgcl::string`.

Past `std::string`, what the strings of Go (`strings`) and Java have, each a new string or the same object when there is nothing to change: `split` (the pieces between the separators, a range of slices walked as it goes: `pieces`), `fields` (the words between runs of white space, the same range), `join` (static: the parts with a separator between each two, built once), `trim`, `trim_left`, `trim_right` (Unicode white space, or the characters given), `trim_prefix`, `trim_suffix`, `replace` (every occurrence, or the first `count`), `repeat`, `to_lower`, `to_upper` (by Unicode's simple case mapping: `"ŁÓDŹ"` to `"łódź"`; the full mapping, where `"ß"` becomes `"SS"`, and the locales are [`txt::to_upper_full`](../txt/case.md)), `equal_fold` (the same letters in either case). The text is UTF-8: `size()` counts bytes, `runes()` walks the code points, a `char32_t` is a character wherever a `char` is (`find(U'ż')`, `split(U'·')`) and an `int` — `'ż'`, a multi-character literal — is refused ([utf8](utf8.md)).

The word is a `tracked_ptr`, so a string lives where one may, as the containers do: on a stack or inside a managed object. A piece of a string is a [`slice`](slice.md) (`string_slice`, a `slice<const char>`): the string's object as the owner and a range in it, so `s.as_slice(pos, n)` is a substring with no copy and no lifetime to watch, and the pieces of `split` are such slices. The read interface below is the mixin `mixin::text`, which a text slice shares.

A map or a set keyed by strings is searched with a `std::string_view`, a slice or a literal as with a string, and no string is made for the search: `std::hash`, `std::equal_to` and `std::less` of a `string` are transparent, so the containers' lookups (`find`, `count`, `contains`, `at`, `equal_range`, `lower_bound`, `upper_bound`, `erase`, `extract`) take either, and the hash of a view or a slice is the hash the string keeps (`hash_of`). `ages.find("alice")` allocates nothing; in Go `m["alice"]` costs nothing either, and this is the same. What inserts (`operator[]`, `try_emplace`, `insert_or_assign`) takes a key, as in `std`.

## Rules

- A `string` is a tracked pointer, so it lives where one may: on a stack or inside a managed object ([The rules](README.md#the-rules), 1).
- Threads share a `string` the way they share a `tracked_ptr` ([The rules](README.md#the-rules), 6): the object itself is immutable and read from any thread without synchronization, and a string variable that one thread replaces while others read it is an [`atomic<string>`](atomic.md#atomicbasic_string), one word, a load for the string as it was and a store for a new one. The hash is computed by the first thread that asks and stored relaxed: every thread computes the same value.
- `data()` and the iterators are valid while some string holds the object: a `std::string_view` taken from a temporary dangles as it would from a `std::string`; a slice does not, it holds the object.
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
operator view_type() const noexcept;                      // a std::string_view of the characters, borrowed
slice<const CharT> as_slice() const noexcept;  slice<const CharT> as_slice(size_type pos, size_type n = npos) const;   // a slice that holds the object: the whole, a range (out_of_range past the end)
operator slice<const CharT>() const noexcept;             // as_slice()
explicit basic_string(const slice<const CharT>& v);       // the slice's own object when it is the whole of a string, a copy otherwise
view_type view() const noexcept;                          // the characters as a std::string_view, borrowed
std::basic_string<CharT, Traits> str() const;             // a copy, to build on
const CharT& operator[](size_type) const noexcept;  const CharT& at(size_type) const;   // at: out_of_range
const CharT& front() const noexcept;  const CharT& back() const noexcept;
const_iterator begin() const noexcept;  const_iterator end() const noexcept;   // and cbegin, cend, rbegin, rend, crbegin, crend
size_type copy(CharT* dest, size_type n, size_type pos = 0) const;
int compare(...) const;  bool starts_with(...) const;  bool ends_with(...) const;  bool contains(...) const;   // the overloads of std::string_view
size_type find(...) const;  rfind, find_first_of, find_last_of, find_first_not_of, find_last_not_of         // the overloads of std::string_view
basic_string substr(size_type pos = 0, size_type n = npos) const;   // a new string; the same object for the whole
class pieces;                                             // a forward range of string_slices (slices that hold the object) into the string: what split and fields return
pieces split(view_type sep, size_type max_parts = 0) const;   // and (CharT sep), (char32_t sep), (const CharT* sep), (const basic_string& sep): the pieces between the separators, in order
pieces fields() const;                                    // the words between runs of Unicode white space, none empty
template<std::ranges::input_range R> static basic_string join(R&& parts, view_type sep);   // and (R&&, CharT), (R&&, char32_t), (R&&, const CharT*): parts convertible to view_type
basic_string trim() const;  basic_string trim(view_type chars) const;  basic_string trim(std::u32string_view set) const;   // without Unicode white space (the characters of `chars`; the code points of `set`) at both ends
basic_string trim_left() const;  basic_string trim_left(view_type chars) const;  basic_string trim_left(std::u32string_view set) const;   // at the start
basic_string trim_right() const;  basic_string trim_right(view_type chars) const;  basic_string trim_right(std::u32string_view set) const;   // at the end
basic_string trim_prefix(view_type prefix) const;  basic_string trim_suffix(view_type suffix) const;   // without it when it is there
basic_string replace(view_type from, view_type to, size_type count = 0) const;   // and (CharT, CharT, count), (char32_t, char32_t, count): every occurrence, or the first `count`
basic_string repeat(size_type count) const;               // the string `count` times over
basic_string to_lower() const;  basic_string to_upper() const;   // by Unicode's simple case mapping (unicode::to_lower), the length free to change
bool equal_fold(view_type s) const noexcept;              // the same letters in either case, code point by code point
runes runes() const noexcept;  size_type rune_count() const noexcept;  pair<char32_t, size_type> decode(size_type pos) const noexcept;  bool is_valid_utf8() const noexcept;   // the code points (utf8.md)
size_type find(char32_t c, size_type pos = 0) const noexcept;   // and rfind, contains, starts_with, ends_with: a character as a code point; find(int) and the rest are deleted ('ż' is an int: write U'ż')
size_type find_first_of(std::u32string_view set, size_type pos = 0) const noexcept;   // and find_last_of, find_first_not_of, find_last_not_of: a set of code points
void swap(basic_string&) noexcept;
size_t hash() const noexcept;                             // std::hash of the characters, computed once, kept in the object
static size_t hash_of(view_type s) noexcept;              // the hash a string of these characters has: for a lookup by a view
const void* object() const noexcept;                      // the object's address: the identity; null when empty
bool operator==(view_type) const noexcept;  bool operator==(const CharT*) const noexcept;
std::strong_ordering operator<=>(view_type) const noexcept;  std::strong_ordering operator<=>(const CharT*) const noexcept;
```

The free functions, in `sgcl`:

```cpp
bool operator==(const basic_string<CharT, Traits>&, const basic_string<CharT, Traits>&) noexcept;   // the same object, or the lengths, the hashes when known, the characters
std::strong_ordering operator<=>(const basic_string<CharT, Traits>&, const basic_string<CharT, Traits>&) noexcept;
basic_string operator+(string, string);  (string, std view);  (std view, string);  (string, const CharT*);  (const CharT*, string);  (string, CharT);  (CharT, string);
std::basic_ostream& operator<<(std::basic_ostream&, const basic_string&);
void swap(basic_string&, basic_string&) noexcept;
template<class T> string to_string(T number);              // an integral or a floating-point number; "true"/"false" for a bool; a char as a string of one
template<class T> expected<T, number_error> parse(std::string_view text, int base = 10);   // an integer from its text; parse<double>(text), parse<bool>(text): the error unless the text is exactly one number that fits
class number_error;   // why(): empty, not_a_number, trailing, out_of_range; offset(), code() (std::errc as from_chars), message()
template<...> struct std::hash<basic_string<...>>;   // the cached hash; transparent: a std::string_view, a slice or a literal hashes as the string would
template<...> struct std::equal_to<basic_string<...>>;   // transparent: a string against a std view, a slice or a literal
template<...> struct std::less<basic_string<...>>;       // transparent
```

The equality compares the words first (a copy is the same object), then the lengths, then the hashes when both have been computed (different hashes: unequal, without reading the characters), then the characters.

```cpp
string name = "alice";                  // one object: 8 bytes of length and hash, six characters, in a slot of 16
string same = name;                     // the same object
string other("alice");                  // another object, equal contents
assert(same.object() == name.object() && other == name && other.object() != name.object());
assert(name.starts_with("al") && name.substr(1, 3) == "lic" && name + "!" == "alice!");
map<string, int> ages;    // a string as a key, the hash kept in the string's object
ages[name] = 30;
assert(ages.at("alice") == 30);               // searched with the literal: no string made for it
assert(ages.find(std::string_view("alice")) != ages.end() && !ages.contains("bob"));
```

`to_string` makes a string of a number, `parse<T>` a number of a text: `parse<int>("42")`, `parse<double>("2.5")`, `parse<bool>("true")`, `parse<int>("ff", 16)`, an `expected` whose error, a `number_error`, says why the text is not exactly one number of the type (no white space, no `+`, no sign for an unsigned type, nothing after the digits, in range) and the byte it stopped on, `std::from_chars` under it, so no locale and no allocation: C#'s `TryParse`, Go's `strconv`, Java's `parseInt` without the exception. A `string` converts to the view, so `parse<int>(s)` reads a string.

The operations past `std::string` return a new string, or the same object when there is nothing to change (`trim` of a string without white space at its ends, `replace` of what does not occur, `to_lower` of a string with no upper-case letter — by Unicode's case, `"łódź"` has none), so a result may be compared by `object()` as by `==`. `split` and `fields` return `pieces`: a value of a few words (the string, the separator as a copy, the limit) that is a forward range of [`string_slice`](slice.md)s into the string, each piece found as the walk reaches it, one `find` per step and no allocation, as `std::views::split` and Go's `strings.SplitSeq`. Each piece holds the string's object, so it is valid on its own, wherever it is kept: `for (sgcl::string_slice piece : s.split(','))` walks them, `sgcl::vector<sgcl::string_slice> parts(s.split(','))` keeps them (every sequence has a constructor from a range), `sgcl::vector<sgcl::string> strings(s.split(','))` makes a string of each, and `join` takes the range as it is. `split` keeps an empty piece where two separators meet or one ends the string, as Go's `strings.Split` does and Java's `split` does not; with `max_parts` the last piece holds the rest of the string; an empty separator splits into characters; an empty string splits into nothing. `fields` drops the empty pieces: the words. `replace` goes left to right without overlapping and never looks into what it has put in; an empty `from` changes nothing. `join` takes any range whose elements a `std::string_view` is made of: strings, slices, literals, `std::string`.

```cpp
string line = "  name = alice, bob ,carol  ";
vector<string> names;
for (string_slice part : line.trim().trim_prefix("name = ").split(',')) {   // a slice per piece, trimmed as a slice, kept as a string
    names.push_back(string(part.trim()));
}
assert(names.size() == 3 && names[1] == "bob");
assert(string::join(names, "; ") == "alice; bob; carol");
vector<string_slice> words(line.fields());         // "name", "=", "alice,", "bob", ",carol": slices, each holding the line
assert(words.size() == 5 && words[2] == "alice," && words[2].object() == line.object());
size_t letters = 0;
for (string_slice piece : line.split(',')) {            // slices: nothing allocated
    letters += piece.size();
}
assert(letters == line.size() - 2);
string title = "the quick brown fox";
assert(title.replace(" ", "_").to_upper() == "THE_QUICK_BROWN_FOX");
assert(title.replace("quick", "slow", 1).trim_suffix(" fox") == "the slow brown");
assert(string("ab").repeat(3) == "ababab");
assert(title.to_lower().object() == title.object());           // nothing to change: the same object
```

The text is Unicode, UTF-8 in the bytes ([utf8](utf8.md)): the case, the white space and a character given as a `char32_t` are code points, the positions are bytes.

```cpp
string city = "\u00A0ŁÓDŹ\u3000";                       // a no-break space, an ideographic space
assert(city.trim() == "ŁÓDŹ" && city.trim().to_lower() == "łódź" && city.size() == 12 && city.rune_count() == 6);
assert(city.find(U'Ó') == 4 && city.contains(U'Ź') && city.trim().replace(U'Ó', U'o') == "ŁoDŹ");
assert(vector<string>(string("a·b·c").split(U'·')).size() == 3 && string("«x»").trim(U"«»") == "x");
assert(string("Łódź").equal_fold("ŁÓDŹ"));
for (char32_t c : city.runes()) {                           // the code points, decoded as they are walked
    assert(c == U'\u00A0' || unicode::is_upper(c) || c == U'\u3000');
}
// city.find('Ó') does not compile: 'Ó' is an int (two bytes in one literal), write U'Ó'
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace sgcl;

// A document tree whose element names are shared: every <p> holds the
// same string object, and a name compares by the word before it compares
// by the characters. Nothing is freed by hand, nothing is counted.
struct Element {
    string name;
    string text;
    vector<tracked_ptr<Element>> children;
};

tracked_ptr<Element> make(string name, string text = {}) {
    tracked_ptr e = make_tracked<Element>();
    e->name = name;                             // a word: the name's object is shared
    e->text = text;
    return e;
}

int count(const tracked_ptr<Element>& e, const string& name) {
    int n = e->name == name ? 1 : 0;            // the same object: a comparison of two words
    for (auto& child : e->children) {
        n += count(child, name);
    }
    return n;
}

int main() {
    string p = "p", div = "div";            // the names, made once
    tracked_ptr root = make(div);
    for (int i : range(3)) {
        tracked_ptr section = make(div);
        for (int j : range(4)) {
            section->children.push_back(make(p, "paragraph " + string(std::to_string(j))));
        }
        root->children.push_back(section);
    }
    std::cout << count(root, p) << " paragraphs, " << count(root, "div") << " divs\n";   // 12 paragraphs, 4 divs
    std::cout << root->children[0]->children[1]->text << "\n";                            // paragraph 1
    return 0;
}
```

The output:

```
12 paragraphs, 4 divs
paragraph 1
```

## See also

- [slice](slice.md): a piece of a string that holds the object; [tracked_ptr](tracked_ptr.md): the word; [map](map.md), [sorted_map](sorted_map.md): the containers a string keys
- README: [string](README.md#string), [The rules](README.md#the-rules), [Allocation](../../garbage_collector/benchmarks.md#allocation)
- `benchmarks/core/string.cpp`: the cost against `std::string`; `tests/core/string.cpp`: every behaviour above, checked.
