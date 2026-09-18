# Sgcl::String

```cpp
#include "sgcl/Sgcl/Core/String.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class CharT, class Traits = std::char_traits<CharT>>
    class BasicString;
    using String = BasicString<char>;
    using WString = BasicString<wchar_t>;
    using U8String = BasicString<char8_t>;
    using U16String = BasicString<char16_t>;
    using U32String = BasicString<char32_t>;
    template<class T> String ToString(T number);
}
```

The same class in the `sgcl` interface: [string](../../core/string.md).

`String` is an immutable string on the managed heap: one word, a pointer to an object holding the length, the hash once something has asked for it (eight bytes together), the characters and a terminator, of exactly that size rounded to four: a string of ten characters is an object of 20 bytes. What a string is in Java or Go rather than in C++: made once, never modified, shared by copying the word, compared and hashed by its contents, reclaimed by the collector when nothing holds it, with no destructor (the sweep frees the slot and runs nothing) and no reference count. The empty string is null and allocates nothing. There is no small-string optimization: the word is the whole of the string, and a string of any length costs the same to copy.

What it is for: text that is kept, shared and compared. Copying one between managed objects costs a word and the write barrier; a `std::string` past its small buffer costs an allocation per copy and a `free` per destruction, in the sweep. Hashing one as a dictionary key costs a load after the first time: the hash (`std::hash` of the characters) is computed once and kept in the string's object, as Java's `String` keeps its `hashCode`. What it is not for: a scratch buffer, or text of a few characters made and dropped at once, where `std::string` costs no allocation at all; `std::string` remains the right member for those, in a managed object as anywhere ([README: string](../../core/README.md#string) has the numbers).

The interface is the read side of a string: `Length`, `Data`, `CStr`, `[]`, `First`, `Last`, `Compare`, `StartsWith`, `EndsWith`, `Contains`, `IndexOf`, `LastIndexOf`, `IndexOfAny`, `LastIndexOfAny`, `Substring` (a new string, or the same object for the whole), the comparisons and `<=>` with a string, a `string_view` or a literal, `operator+` (a new string), `std::hash`, `operator<<`, the conversions to `string_view` and to `std::string` (`ToStd()`); the constructors from a literal, `(s, n)`, a `string_view`, a `std::string` or anything a `string_view` is made of, `(n, ch)`, a range, an initializer list; `ToString(number)` for a number. No mutation, no capacity: a string is built as a `std::string` or a `string_view` and made once. The length is kept in 32 bits.

The word is a `Ptr`, so a string lives where one may, as the containers do: on a stack or inside a managed object. A piece of a String is a [`StringView`](StringView.md): two words that hold the string's object and a range in it, so `s.View(pos, n)` is a substring with no copy and no lifetime to watch, and the pieces of `Split` are such views.

## Rules

- A `String` is a tracked pointer, so it lives where one may: on a stack or inside a managed object ([The rules](../../core/README.md#the-rules), 1).
- Threads share a `String` the way they share a `Ptr` ([The rules](../../core/README.md#the-rules), 6): the object itself is immutable and read from any thread without synchronization, and a string variable that one thread replaces while others read it is an [`Atomic<String>`](../Concurrent/Atomic.md#atomicstring), one word, a load for the string as it was and a store for a new one. The hash is computed by the first thread that asks and stored relaxed: every thread computes the same value.
- `Data()` and the range are valid while some string holds the object: a `string_view` taken from a temporary dangles as it would from a `std::string`.
- A string's object is never traced and never zeroed: its bytes are characters and nothing else.

## Members

```cpp
using CharType = CharT;  using SizeType = size_t;  using ViewType = std::basic_string_view<CharT, Traits>;
using InnerType = sgcl::basic_string<CharT, Traits>;
static constexpr SizeType NoPosition;                     // what a search that finds nothing returns

BasicString() noexcept;                                   // empty: null, nothing allocated
BasicString(const CharT* s);
BasicString(const CharT* s, SizeType n);
BasicString(ViewType s);
template<class V> explicit BasicString(const V& v);       // anything a ViewType is made of: a String
BasicString(SizeType n, CharT c);
template<std::input_iterator It> BasicString(It first, It last);
BasicString(std::initializer_list<CharT>);
BasicString(InnerType s) noexcept;
BasicString(const BasicString&) noexcept;                 // the same object
BasicString& operator=(...);                              // the same set

const CharT* Data() const noexcept;  const CharT* CStr() const noexcept;   // terminated; the empty string's is a terminator
SizeType Length() const noexcept;  bool IsEmpty() const noexcept;
operator ViewType() const noexcept;                       // a std::string_view of the characters, borrowed
BasicStringView View() const noexcept;  BasicStringView View(SizeType pos, SizeType n = NoPosition) const;   // a view that holds the object: the whole, a range
explicit BasicString(const BasicStringView& v);           // the view's own object when it is the whole of it, a copy otherwise
std::basic_string<CharT, Traits> ToStd() const;           // a copy, to build on
const CharT& operator[](SizeType) const noexcept;
const CharT& First() const noexcept;  const CharT& Last() const noexcept;
SizeType CopyTo(CharT* dest, SizeType n, SizeType pos = 0) const;
int Compare(ViewType) const noexcept;
bool StartsWith(ViewType) const noexcept;  bool StartsWith(CharT) const noexcept;
bool EndsWith(ViewType) const noexcept;  bool EndsWith(CharT) const noexcept;
bool Contains(ViewType) const noexcept;  bool Contains(CharT) const noexcept;
SizeType IndexOf(ViewType s, SizeType from = 0) const noexcept;  SizeType IndexOf(CharT c, SizeType from = 0) const noexcept;
SizeType LastIndexOf(ViewType s, SizeType from = NoPosition) const noexcept;  SizeType LastIndexOf(CharT c, SizeType from = NoPosition) const noexcept;
SizeType IndexOfAny(ViewType chars, SizeType from = 0) const noexcept;  SizeType LastIndexOfAny(ViewType chars, SizeType from = NoPosition) const noexcept;
BasicString Substring(SizeType pos = 0, SizeType n = NoPosition) const;   // a new string; the same object for the whole
using Pieces = InnerType::pieces;                         // a forward range of views (sgcl::string_view, a StringView each) into the string: what Split and SplitWords return
Pieces Split(ViewType sep, SizeType maxParts = 0) const;  // and (CharT sep), (const CharT* sep), (const BasicString& sep): the pieces between the separators, in order
Pieces SplitWords() const;                                // the words between runs of white space, none empty
template<std::ranges::input_range R> static BasicString Join(R&& parts, ViewType sep);   // and (R&&, CharT), (R&&, const CharT*): parts convertible to ViewType
BasicString Trim() const;  BasicString Trim(ViewType chars) const;               // without white space (the characters of `chars`) at both ends
BasicString TrimStart() const;  BasicString TrimStart(ViewType chars) const;     // at the start
BasicString TrimEnd() const;  BasicString TrimEnd(ViewType chars) const;         // at the end
BasicString TrimPrefix(ViewType prefix) const;  BasicString TrimSuffix(ViewType suffix) const;   // without it when it is there
BasicString Replace(ViewType from, ViewType to, SizeType count = 0) const;   // and (CharT, CharT, count): every occurrence, or the first `count`
BasicString Repeat(SizeType count) const;                 // the string `count` times over
BasicString ToLower() const;  BasicString ToUpper() const;   // the ASCII letters
void Swap(BasicString&) noexcept;
size_t Hash() const noexcept;                             // std::hash of the characters, computed once, kept in the object
const void* Object() const noexcept;                      // the object's address: the identity; null when empty
bool Equals(const BasicString&) const noexcept;           // what == does
InnerType& Inner() noexcept;  const InnerType& Inner() const noexcept;
```

The free functions, in `Sgcl`:

```cpp
bool operator==(const BasicString<CharT, Traits>&, const BasicString<CharT, Traits>&) noexcept;   // the same object, or the lengths, the hashes when known, the characters
bool operator==(const BasicString&, ViewType) noexcept;  bool operator==(const BasicString&, const CharT*) noexcept;
std::strong_ordering operator<=>(const BasicString&, const BasicString&) noexcept;   // and with a ViewType, a const CharT*
BasicString operator+(String, String);  (String, view);  (view, String);  (String, const CharT*);  (const CharT*, String);  (String, CharT);  (CharT, String);
std::basic_ostream& operator<<(std::basic_ostream&, const BasicString&);
void swap(BasicString&, BasicString&) noexcept;
const CharT* begin(const BasicString&) noexcept;  const CharT* end(const BasicString&) noexcept;   // a range-for over the characters
template<class T> String ToString(T number);              // an integral or a floating-point number; "true"/"false" for a bool; a char
template<class T> Optional<T> Parse(std::string_view text, int base = 10);   // an integer from its text; Parse<double>(text), Parse<bool>(text): None unless the text is exactly one number that fits
template<...> struct std::hash<BasicString<...>>;   // the cached hash; transparent: a string_view or a literal hashes as the String would
template<...> struct std::equal_to<BasicString<...>>;  template<...> struct std::less<BasicString<...>>;   // transparent
```

`ToString` makes a String of a number, `Parse<T>` a number of a text: `Parse<int>("42")`, `Parse<double>("2.5")`, `Parse<bool>("true")`, `Parse<int>("ff", 16)`, an `Optional` that is `None` unless the text is exactly one number of the type (no white space, no `+`, no sign for an unsigned type, nothing after the digits, in range): C#'s `TryParse` as an `Optional`. A `String` converts to the view, so `Parse<int>(s)` reads a String.

A `Dictionary`, `SortedDictionary`, `HashSet` or `SortedSet` keyed by strings is searched with a `string_view` or a literal as with a `String`, and no `String` is made for the search: `Find`, `ContainsKey`, `Contains`, `Remove` take either, and the hash of a view is the hash the string keeps. `ages.Find("alice")` allocates nothing. What inserts (`Add`, `Set`, `operator[]`) takes a key.

The equality compares the words first (a copy is the same object), then the lengths, then the hashes when both have been computed (different hashes: unequal, without reading the characters), then the characters.

```cpp
String name = "alice";                 // one object: 8 bytes of length and hash, six characters, in a slot of 16
String same = name;                    // the same object
String other("alice");                 // another object, equal contents
assert(same.Object() == name.Object() && other == name && other.Object() != name.Object());
assert(name.StartsWith("al") && name.Substring(1, 3) == "lic" && name + "!" == "alice!");
Dictionary<String, int> ages;          // a String as a key, the hash kept in the string's object
ages[name] = 30;
assert(*ages.Find("alice") == 30);     // searched with the literal: no String made for it
assert(ages.ContainsKey(std::string_view("alice")) && !ages.ContainsKey("bob"));
```

The operations past `std::string` (`Split`, `SplitWords`, `Join`, `Trim`, `TrimStart`, `TrimEnd`, `TrimPrefix`, `TrimSuffix`, `Replace`, `Repeat`, `ToLower`, `ToUpper`: what C# and Java have) return a new string, or the same object when there is nothing to change (`Trim` of a string without white space at its ends, `Replace` of what does not occur, `ToLower` of a string with no upper-case letter), so a result may be compared by `Object()` as by `==`. `Split` and `SplitWords` return `Pieces`: a value of a few words (the string, the separator as a copy, the limit) that is a forward range of views into the string, each piece found as the walk reaches it and nothing allocated, as `std::views::split`; each piece is a [`StringView`](StringView.md) that holds the string's object, valid on its own wherever it is kept. `for (StringView piece : s.Split(','))` walks them, `List<StringView> parts(s.Split(','))` keeps them (every sequence has a constructor from a range), `List<String> strings(s.Split(','))` makes a String of each, and `Join` takes the range as it is. `Split` keeps an empty piece where two separators meet or one ends the string, as C#'s `Split` does; with `maxParts` the last piece holds the rest of the string; an empty separator splits into characters; an empty string splits into nothing. `SplitWords` drops the empty pieces. `Replace` goes left to right without overlapping and never looks into what it has put in; an empty `from` changes nothing. `Join` takes any range whose elements a `ViewType` is made of: a `List<String>`, views, literals, `std::string`. `ToLower` and `ToUpper` change the ASCII letters; Unicode is for `text`.

```cpp
String line = "  name = alice, bob ,carol  ";
List<String> names;
for (StringView part : line.Trim().TrimPrefix("name = ").Split(',')) {   // a view per piece, trimmed as a view, kept as a String
    names.Add(part.Trim().ToString());
}
assert(names.Count() == 3 && names[1] == "bob");
assert(String::Join(names, "; ") == "alice; bob; carol");
List<StringView> words(line.SplitWords());                     // "name", "=", "alice,", "bob", ",carol": views, each holding the line
assert(words.Count() == 5 && words[2] == "alice," && words[2].Object() == line.Object());
size_t letters = 0;
for (StringView piece : line.Split(',')) {                     // views: nothing allocated
    letters += piece.Length();
}
assert(letters == line.Length() - 2);
String title = "the quick brown fox";
assert(title.Replace(" ", "_").ToUpper() == "THE_QUICK_BROWN_FOX");
assert(title.Replace("quick", "slow", 1).TrimSuffix(" fox") == "the slow brown");
assert(String("ab").Repeat(3) == "ababab");
assert(title.ToLower().Object() == title.Object());            // nothing to change: the same object
```

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

// A document tree whose element names are shared: every <p> holds the
// same string object, and a name compares by the word before it compares
// by the characters. Nothing is freed by hand, nothing is counted.
struct Element {
    String name;
    String text;
    List<Ptr<Element>> children;
};

Ptr<Element> MakeElement(String name, String text = {}) {
    Ptr e = Make<Element>();
    e->name = name;                             // a word: the name's object is shared
    e->text = text;
    return e;
}

int Count(const Ptr<Element>& e, const String& name) {
    int n = e->name == name ? 1 : 0;            // the same object: a comparison of two words
    for (auto& child : e->children) {
        n += Count(child, name);
    }
    return n;
}

int main() {
    String p = "p", div = "div";       // the names, made once
    Ptr root = MakeElement(div);
    for (int i : Range(3)) {
        Ptr section = MakeElement(div);
        for (int j : Range(4)) {
            section->children.Add(MakeElement(p, "paragraph " + ToString(j)));
        }
        root->children.Add(section);
    }
    std::cout << Count(root, p) << " paragraphs, " << Count(root, "div") << " divs\n";   // 12 paragraphs, 4 divs
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

- [StringView](StringView.md): a piece of a String that holds the object; [Ptr](Ptr.md): the word; [Dictionary](../Containers/Dictionary.md), [SortedDictionary](../Containers/SortedDictionary.md): the containers a string keys
- [Atomic](../Concurrent/Atomic.md#atomicstring): a string variable shared between threads
- README: [string](../../core/README.md#string), [The rules](../../core/README.md#the-rules), [Allocation](../../../garbage_collector/benchmarks.md#allocation)
