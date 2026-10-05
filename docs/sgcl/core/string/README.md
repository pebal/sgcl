[sgcl](../../README.md) › [core](../README.md)

# sgcl::string

```cpp
#include "sgcl/core/string.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class CharT, class Traits = std::char_traits<CharT>>
    class basic_string;

    using string = basic_string<char>;
    using wstring = basic_string<wchar_t>;
    using u8string = basic_string<char8_t>;
    using u16string = basic_string<char16_t>;
    using u32string = basic_string<char32_t>;
    using string_slice = slice<const char>;
}
```

**Requires [rooted](../rooted/README.md) outside a stack or a managed object.**

`sgcl::string` is an immutable string on the managed heap. It is one word, a pointer to an object that holds the length
and the hash (eight bytes together), the characters and a terminator, of exactly that size rounded to four: a string of
ten characters is an object of 20 bytes. It is what a string is in Java or Go rather than in C++: made once, never
modified, shared by copying the word, compared and hashed by its contents, and reclaimed by the collector when nothing
holds it, with no destructor (the sweep frees the slot and runs nothing) and no reference count. The empty string is
null and allocates nothing. A move is a copy of the word, as the move of a `tracked_ptr` is: the moved-from string keeps
its value. There is no small-string optimization: the word is the whole of the string, and a string of any length costs
the same to copy.

It is for text that is kept, shared and compared. A copy between managed objects costs a word and the write barrier,
where a `std::string` past its small buffer costs an allocation per copy and a `free` per destruction, in the sweep.
A hash costs a load after the first time: it is computed once and kept in the string's object, as Java's `String`
keeps its `hashCode`. It is not for a scratch buffer or for text of a few characters made and dropped at once, where
a `std::string` costs no allocation at all; `std::string` remains the right member for those, in a managed object as
anywhere.

The interface is the read side of `std::string` and all of `std::string_view`, the latter from
[mixin::text](../mixin/text/README.md), which a text slice shares. There is no mutation and no `capacity`: a text is built as a
`std::string` or a `std::string_view` and made a string once. Past `std::string` are the operations of Go's
`strings` and of Java's `String`: [split](split.md), [fields](fields.md), [join](join.md),
[concat](concat.md), [trim](trim.md) and its kin, [replace](replace.md),
[repeat](repeat.md), [to_lower](to_lower.md) and [to_upper](to_upper.md), each returning a new
string, or the same object when there is nothing to change, so a result may be compared by `object()` as by `==`.
The length is kept in 32 bits: a string holds at most 4 294 967 295 characters, and a longer text is refused with
`length_error` before a character of it is read, as `std::string` refuses what passes its own maximum.

The text is UTF-8: `size()` counts bytes, `runes()` walks the code points, a `char32_t` is a character wherever a
`char` is, and an `int`, such as the multi-character literal `'ab'`, is refused ([utf8](../utf8/README.md),
[unicode](../unicode/README.md)). A piece of a string is a [slice](../slice/README.md), `string_slice`: the string's object as the owner
and a range in it, a substring with no copy and no lifetime to watch. The pieces of `split` and `fields` are such
slices, and a string made of a slice that is the whole of one is that string's object again.

The hash is keyed: a hash of the characters under a key of four words drawn once per process, so which texts share a
bucket is not known outside the process, and a map keyed by what a client sends (the names of HTTP headers, the keys
of a JSON object) cannot be filled with keys of one bucket, the attack known as HashDoS; Go and Rust key their maps
the same way. The order of a `map` or a `set` of strings therefore differs between runs; the environment variable
`SGCL_HASH_SEED`, a number, fixes the key for a test that prints one. A `std::string` key hashes by
`std::hash<std::string>`, which is not keyed: a map of untrusted keys is keyed by `sgcl::string`.

## Rules

- Threads share a string the way they share a `tracked_ptr` ([The rules](../README.md#the-rules), 6). The object is
  immutable and is read from any thread without synchronization; a string variable that one thread replaces while
  others read it is an [atomic](../atomic.md) of the string, one word: a load for the string as it was and a store for
  a new one. The hash is computed by the first thread that asks and stored relaxed: every thread computes the same
  value.
- `data()`, `c_str()` and the iterators are valid while some string holds the object: a `std::string_view` taken from
  a temporary string dangles as it would from a `std::string`; a slice does not, it holds the object.
- A string's object is never traced and never zeroed: its bytes are the header, the characters and the terminator.
- A string is made once, at its size. When the size is known up front, it is one allocation: `concat` of the pieces,
  `repeat`, `txt::format`, a number's `to_string`. When the text comes in pieces, they are gathered and joined once:
  `join` over a range of them, `concat` over a few known ones. A string is never grown by `a + b + c` or by `+` in a
  loop, where each `+` is a string of its own. A `std::string` built with `+=` and copied into a string at the end is
  for code off the hot path only: its buffer doubles as it grows, and its bytes are copied once more at the end.
- A text given as characters is read as the constructor reads it, everywhere: an array of `CharT` (a literal) up to
  its first NUL or its end, whichever comes first, so an array filled to the brim is not read past its end; a
  `CharT*` or a `const CharT*` up to its NUL. `nullptr` and every other pointer do not compile. The assignment,
  `compare`, `starts_with`, `ends_with`, `contains`, the six `find`s, `equal_fold`, `split`, `join`, `==`, `<=>`,
  `+` and `std::hash` take the array and the pointer; `trim`, `trim_left`, `trim_right`, `trim_prefix`,
  `trim_suffix`, `replace` and `concat` take the array.
- An `int` is no character. `'ż'` in a UTF-8 source is two bytes in one literal: a compiler that takes it makes it a
  multi-character literal of type `int` (Clang refuses it as too large for a `char`), which a search for a `char`
  would cut to its last byte and find inside every `ź`, `ż` and `Ż`. The overloads that take an `int` are deleted,
  so `find('ż')` and `find('ab')` do not compile; the character is written `U'ż'`. A `char` is still a byte:
  `find('a')` searches for it, and `find(0)` is `find('\0')`.
- A wide string is UTF-16 (`u16string`, and `wstring` where `wchar_t` has 16 bits) or a code point a unit
  (`u32string`, a 32-bit `wstring`): the case goes by code points, a UTF-16 surrogate pair one letter (Deseret,
  Adlam) and a lone surrogate left as it is, and the white space per unit, all of it being below U+FFFF. A
  `char32_t` as a character and a set of code points (`find(U'😀')`, `trim(U"«»")`, `split(U'·')`) are encoded
  into the string's units (a value that is no code point is found nowhere) and walked by code points in every
  string, a surrogate pair one code point, as `rune_count` counts them; in a `u32string` they are its own
  characters. The members that read UTF-8 (`runes`, `decode`, `is_valid_utf8`) are those of `string` and
  `u8string`.

## Template parameters

| Parameter | Description |
|---|---|
| `CharT` | The character type: `char` (UTF-8), `wchar_t`, `char8_t` (UTF-8), `char16_t` or `char32_t`. |
| `Traits` | The character traits, `std::char_traits<CharT>` by default; the type of the views the string converts to and is compared with. |

## Member types

| Type | Definition |
|---|---|
| `traits_type` | `Traits` |
| `value_type` | `CharT` |
| `size_type` | `size_t` |
| `difference_type` | `ptrdiff_t` |
| `reference` | `const CharT&` |
| `const_reference` | `const CharT&` |
| `pointer` | `const CharT*` |
| `const_pointer` | `const CharT*` |
| `iterator` | `const CharT*` |
| `const_iterator` | `const CharT*` |
| `reverse_iterator` | `std::reverse_iterator<const_iterator>` |
| `const_reverse_iterator` | `std::reverse_iterator<const_iterator>` |
| `view_type` | `std::basic_string_view<CharT, Traits>` |
| `slice_type` | `slice<const CharT>`: a piece of the string that holds it |
| [pieces](../string-pieces/README.md) | the range of slices `split` and `fields` return |

## Member objects

| Constant | Value | Description |
|---|---|---|
| `npos` | `view_type::npos` | a position past every character: "to the end" as a length, "not found" as the result of a search, `static constexpr size_type` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](string.md) | constructs the string |
| `(destructor)` | drops the word; the object is left to the collector, and nothing runs when it is freed |
| [operator=](operator_assign.md) | assigns another string, or a new one made of characters |

#### Element access

| Function | Description |
|---|---|
| [operator[]](operator_at.md) | the character at a position |
| [front](front.md) | the first character |
| [back](back.md) | the last character |
| [data](data.md) | the characters, terminated |
| [c_str](c_str.md) | the characters as a C string |
| [as_slice, operator slice_type](as_slice.md) | the characters, or a part of them, as a slice that holds the object |

#### Iterators

| Function | Description |
|---|---|
| [begin, cbegin](begin.md) | an iterator to the first character |
| [end, cend](end.md) | an iterator past the last character |
| [rbegin, crbegin](rbegin.md) | a reverse iterator to the last character |
| [rend, crend](rend.md) | a reverse iterator before the first character |

#### Capacity

| Function | Description |
|---|---|
| [empty](empty.md) | checks whether the string has no characters |
| [size, length](size.md) | the number of characters |
| [max_size](max_size.md) | the largest number of characters a string holds |

#### Operations

| Function | Description |
|---|---|
| [substr](substr.md) | a new string of a part of the characters |
| [split](split.md) | the pieces between the occurrences of a separator |
| [fields](fields.md) | the words between runs of white space |
| [join](join.md) | one string of the parts with a separator between each two, `static` |
| [concat](concat.md) | one string of a few pieces in order, `static` |
| [trim](trim.md) | without white space, or the characters given, at both ends |
| [trim_left](trim_left.md) | without white space, or the characters given, at the start |
| [trim_right](trim_right.md) | without white space, or the characters given, at the end |
| [trim_prefix](trim_prefix.md) | without a prefix, when it is there |
| [trim_suffix](trim_suffix.md) | without a suffix, when it is there |
| [replace](replace.md) | with every occurrence of a text, or the first ones, replaced |
| [repeat](repeat.md) | the string a number of times over |
| [to_lower](to_lower.md) | with every letter in lower case |
| [to_upper](to_upper.md) | with every letter in upper case |
| [swap](swap.md) | swaps the words of two strings |

#### Identity and hash

| Function | Description |
|---|---|
| [hash](hash.md) | the hash of the characters, computed once and kept in the object |
| [hash_of](hash_of.md) | the hash a string of the given characters has, `static` |
| [object](object.md) | the address of the string's object |

#### From mixin::text

The read side of `std::string_view`, which a text slice shares ([mixin::text](../mixin/text/README.md)).

| Function | Description |
|---|---|
| `view`, `operator view_type` | the characters as a `std::basic_string_view`, borrowed |
| `str` | a `std::basic_string` of the characters, to build on |
| `at` | the character at a position, with bounds checking |
| `copy` | copies characters into an array |
| `compare` | compares with another text |
| `starts_with`, `ends_with` | checks whether the string begins or ends with a text or a character |
| `contains` | checks whether the string contains a text or a character |
| `find`, `rfind` | the position of the first, the last occurrence of a text or a character |
| `find_first_of`, `find_last_of` | the position of the first, the last character of a set |
| `find_first_not_of`, `find_last_not_of` | the position of the first, the last character not in a set |
| `runes` | the code points, decoded as they are walked ([runes](../runes/README.md)) |
| `rune_count` | the number of code points |
| `decode` | the code point at a byte position and its width |
| `is_valid_utf8` | checks whether every sequence is valid UTF-8 |
| `equal_fold` | checks whether two texts are the same letters in either case |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator\<=\>](operator_cmp.md) | compare two strings, or a string and a text slice, by their characters |
| [operator+](operator_arith.md) | a new string of two texts |
| `operator<<` | writes the characters to a `std::basic_ostream` |
| [swap](swap.md) | swaps the words of two strings |

## Specializations

```cpp
namespace std {
    template<class CharT, class Traits> struct hash<sgcl::basic_string<CharT, Traits>>;
    template<class CharT, class Traits> struct equal_to<sgcl::basic_string<CharT, Traits>>;
    template<class CharT, class Traits> struct less<sgcl::basic_string<CharT, Traits>>;
}
```

The hash, the equality and the order of a string are transparent (`is_transparent`): a map or a set keyed by strings
is searched with a `std::basic_string_view`, a text slice, a literal or a pointer as with a string, and no string is
made for the search. The hash of a string is the one it keeps ([hash](hash.md)); the hash of a view, a slice,
an array or a pointer is the hash a string of those characters has ([hash_of](hash_of.md)), so the containers'
lookups (`find`, `count`, `contains`, `at`, `equal_range`, `lower_bound`, `upper_bound`, `erase`, `extract`) take
any of them: a search for `"alice"` allocates nothing, as `m["alice"]` costs nothing in Go. What inserts
(`operator[]`, `try_emplace`, `insert_or_assign`) takes a key, as in `std`.

## Complexity

- A copy, a move, an assignment of a string and `swap`: constant, one word.
- Making a string: linear in the number of characters, one allocation; none for the empty string.
- `==`: constant when the two are the same object, differ in length, or have different hashes already computed;
  linear in the length otherwise.
- `hash`: linear in the length the first time, a load after.

The costs against `std::string`, Go's string and Java's `String` are on
[Benchmarks: Strings](../benchmarks.md#strings).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

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
    e->name = name;  // a word: the name's object is shared
    e->text = text;
    return e;
}

int count(const tracked_ptr<Element>& e, const string& name) {
    int n = e->name == name ? 1 : 0;  // the same object: a comparison of two words
    for (auto& child : e->children) {
        n += count(child, name);
    }
    return n;
}

int main() {
    string p = "p", div = "div";  // the names, made once
    tracked_ptr root = make(div);
    for (int i : range(3)) {
        tracked_ptr section = make(div);
        for (int j : range(4)) {
            section->children.push_back(make(p, "paragraph " + to_string(j)));
        }
        root->children.push_back(section);
    }
    println("{} paragraphs, {} divs", count(root, p), count(root, "div"));
    println("{}", root->children[0]->children[1]->text);
}
```

Output:

```text
12 paragraphs, 4 divs
paragraph 1
```

## See also

- [slice](../slice/README.md): a piece of a string that holds the object
- [utf8](../utf8/README.md), [unicode](../unicode/README.md), [runes](../runes/README.md): the encoding, the code point, the code points of a text
- [to_string](../to_string.md), [parse](../parse.md): a number as a string, and back
- [mixin::text](../mixin/text/README.md): the read interface a string shares with a text slice
- [map](../map/README.md), [sorted_map](../sorted_map/README.md): the containers a string keys
- [txt::to_upper_full](../../txt/to_upper_full.md): the full case mapping and the rules of a language
- [tracked_ptr](../tracked_ptr/README.md): the word; [README: The rules](../README.md#the-rules)
- [Allocation](../../../garbage_collector/benchmarks.md#allocation), [Benchmarks: Strings](../benchmarks.md#strings)
