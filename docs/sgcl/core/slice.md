# sgcl::slice

```cpp
#include "sgcl/core/slice.h"   // or "sgcl/core/core.h", "sgcl/sgcl.h"

namespace sgcl {
    template<class T>
    class slice;
    using string_slice = slice<const char>;   // a piece of a string that holds it (string.h)
    template<class T> slice<const std::byte> as_bytes(const slice<T>& s) noexcept;
    template<class T> slice<std::byte> as_writable_bytes(const slice<T>& s) noexcept;
}
```

`sgcl::slice<T>` is the elements `[begin, end)` of some contiguous storage and the managed object they lie in, kept alive by the slice for as long as the slice exists: what a slice is in Go (a piece of the array that shares it and holds it), and what `std::span` and `std::string_view` are not (a range with no duty to keep its memory). Three words: the owner, a `tracked_ptr` to the object — a [string](string.md), the buffer of a [vector](../containers/vector.md), the block of a [buffered_reader](../io/buffered.md) — and two raw pointers into it. A slice of unmanaged memory (a stack array, a `std::vector`, a `std::span`) has no owner: its word is null, and the slice promises what a span does, the memory valid for the call. Which of the two a slice is follows from where the memory comes from, not from a choice: a string, a vector, a reader's block hand out slices with the owner set (`s.as_slice()`, `v.as_slice()`, a line of `read_line`); a raw pointer or a std container give one without. The owner is given by whoever knows it, never guessed from an address (a pointer into an object finds the object only within its first page).

What it is for, with an owner: a piece of a string with no copy — a token, a field, a line, the pieces of [`split`](string.md#members) — kept in a container or a managed object as it is, the source alive for as long as any piece is; a line of a file handed out by a reader without an allocation, valid after the reader has moved on to the next block; a fragment of a buffer given to an asynchronous `read`, the buffer rooted by the argument while the task waits. Without an owner: what a `std::span` is for, one type for both. `slice<const char>` is text and has the read interface of a string (`mixin::text`: `find`, `starts_with`, `compare`, `trim`, `substr`…); `slice<std::byte>` is a buffer to read into, `slice<const std::byte>` data to write; `slice<T>` of anything else is a span with an owner.

What it costs: a slice without an owner is three word stores — no barrier, no registration of the thread, the price of a span. A slice with an owner is a `tracked_ptr`'s copy: the write barrier, and the registration of the thread's stack the first time a managed word lands on it. The rule the two paths keep is that a non-null owner never lands on a stack the collector does not know: the constructor from an owner, and the copy and the assignment from an owned slice, register; the paths without an owner skip it.

## Rules

- A slice lives where a `tracked_ptr` may: on a stack or inside a managed object, never in `new`/`malloc` memory, a `std` container, a global or a `thread_local` ([The rules](README.md#the-rules), 1) — even a slice without an owner, since one may be assigned to it. A `std::span` (`operator std::span<T>()`) is what goes to those places.
- The owner is explicit: `slice(owner, first, last)` with the object the elements lie in; in a debug build the constructor asserts that `[first, last)` lies in it. A slice of a `string` or a `vector` comes from the container (`as_slice`), which knows its object.
- `data()` is not terminated: a slice is a range, not a C string; `str()` for a `std::string`, `string(s)` for a string.
- A slice's elements are `T`: a `slice<int>` writes through, a `slice<const int>` does not; `slice<T>` converts to `slice<const T>`, the owner carried over.
- The elements a slice with an owner points at are the memory of that moment: a reader reuses its block for the next lines, a vector overwrites its buffer, so a slice kept across such a change reads what was written since — alive and well-formed, not what it read before. A slice kept for its text is copied first (`string(line)`).
- Inside a managed object, the raw `begin` and `end` are words the collector's pointer map traces like any address-holding word, so the destructor nulls them: a container slot a slice was destroyed in keeps nothing alive.
- Threads share a slice the way they share a `tracked_ptr` ([The rules](README.md#the-rules), 6): a slice variable that one thread replaces while others read it needs the program's synchronization (three words: not an atomic).

## Members

```cpp
using element_type = T;  using value_type = std::remove_cv_t<T>;  using size_type = size_t;
using iterator = T*;  using const_iterator = const T*;  reverse_iterator, const_reverse_iterator
static constexpr size_type npos;

slice() noexcept;                                                     // empty, no owner
slice(T* first, T* last) noexcept;  slice(T* first, size_type n) noexcept;                        // unmanaged memory: no owner
slice(const tracked_ptr<const void>& owner, T* first, T* last) noexcept;  slice(owner, T* first, size_type n) noexcept;   // the elements of the managed object owner
slice(std::span<T>) noexcept;  slice(T (&)[N]) noexcept;  slice(std::array<U, N>&) noexcept;  slice(std::vector<U, A>&) noexcept;   // no owner; the const forms likewise
slice(std::basic_string_view<CharT>) noexcept;                        // text: no owner
template<class U> slice(const slice<U>& o) noexcept;                  // slice<T> from slice<U> where U* converts to T*: the owner carried over
slice(const slice&) noexcept;  slice& operator=(const slice&) noexcept;   // an owner registers the thread, a null owner costs nothing

const tracked_ptr<const void>& owner() const noexcept;  bool owned() const noexcept;
T* data() const noexcept;  size_type size() const noexcept;  size_type size_bytes() const noexcept;  bool empty() const noexcept;
T& operator[](size_type i) const noexcept;  T& front() const noexcept;  T& back() const noexcept;
iterator begin() const noexcept;  iterator end() const noexcept;  cbegin, cend, rbegin, rend, crbegin, crend
slice subslice(size_type pos, size_type n = npos) const;             // [pos, pos + n) of the same owner; out_of_range past the end
slice subspan(size_type pos, size_type n = npos) const;              // the same, std::span's name
slice first(size_type n) const noexcept;  slice last(size_type n) const noexcept;
void remove_prefix(size_type n) noexcept;  void remove_suffix(size_type n) noexcept;
operator std::span<T>() const noexcept;                               // for a std interface: the range without the owner
void swap(slice& o) noexcept;

// the mixins (mixin/README.md): a slice answers what a vector answers, and is sorted in place when T is not const
// mixin::enumerable: find_if, find_index, exists, all, count_of, for_each, contains, index_of, last_index_of, min, max
// mixin::ordered: is_sorted, binary_search, sorted_index_of, lower_bound, upper_bound; sort, sort_by, stable_sort (slice<T> only)
// mixin::sequence (slice<T> only): fill, reverse
// mixin::equatable, mixin::comparable: == and <=> by the elements, for elements that compare

// slice<const CharT>, text (mixin::text over the characters): the read side of std::string_view
view_type view() const noexcept;  operator view_type() const noexcept;  std::string str() const;
size_type length() const noexcept;  const CharT& at(size_type i) const;
compare, starts_with, ends_with, contains, find, rfind, find_first_of, find_last_of, find_first_not_of, find_last_not_of, copy
slice substr(size_type pos = 0, size_type n = npos) const;           // subslice, under its text name
slice trim() const;  slice trim(view_type chars) const;  trim_left, trim_right;  slice trim_prefix(view_type) const;  slice trim_suffix(view_type) const;   // slices of the same owner
friend bool operator==(const slice& a, view_type s);  friend bool operator==(const slice& a, const CharT* s);  <=> likewise
std::hash<slice<const CharT>>, std::equal_to, std::less;             // transparent: a string of the characters hashes the same
operator<<(std::ostream&, const slice<const CharT>&);

template<class T> slice<const std::byte> as_bytes(const slice<T>& s) noexcept;              // the bytes, the owner carried over
template<class T> slice<std::byte> as_writable_bytes(const slice<T>& s) noexcept;
```

```cpp
string text = "name = alice, bob";
string_slice value = text.as_slice(7);          // "alice, bob": a piece of text that holds it
slice<const char> name = value.trim_prefix("alice, ");   // "bob", the same owner, nothing copied
for (auto piece : text.split(','))                    // the pieces of a string: slices of it
    std::cout << piece.trim() << '\n';

vector<std::byte> buffer(4096);
slice<std::byte> room = buffer;                  // the buffer's own object as the owner
auto n = file->read(room);                             // io::reader::read takes a slice; an async_read holds the buffer while the task waits
auto data = room.first(*n);

int local[16];
slice<int> ints(local);                          // a stack array: no owner, a span
ints.fill(0);
vector v = {5, 3, 4};
slice<int> tail = v.as_slice(1);
tail.sort();                                           // the mixins: v is 5 3 4 now, the slice sorted in place; tail.max() is 4
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace sgcl;

// A tokenizer whose tokens are slices of the text: nothing copied, the
// text alive for as long as any token is, wherever the tokens go
struct Token {
    string_slice text;
    int line;
};

vector<Token> tokenize(const string& source) {
    vector<Token> tokens;
    int line = 1;
    for (auto raw : source.split('\n')) {                          // each piece a slice of source
        for (auto word : string(raw).fields()) {             // the words of the line (fields is the string's: a string of the line, one allocation per line)
            tokens.push_back({word, line});
        }
        ++line;
    }
    return tokens;
}

int main() {
    vector<Token> tokens;
    {
        string source = "let x = 1\nlet y = x + 2";          // dies at the brace, as a variable
        tokens = tokenize(source);
    }
    collector::force_collect();                              // optional, to show the result at once
    for (auto& t : tokens) {                                       // the texts live on: each slice holds its line
        std::cout << t.line << ": " << t.text << '\n';
    }
    std::cout << tokens.size() << " tokens, " << (tokens[0].text.owned() ? "owned" : "unowned") << '\n';   // 10 tokens, owned
}
```

## See also

- [string](string.md): `as_slice`, `split` and `fields` hand out slices; a string of a slice; [vector](../containers/vector.md): `as_slice` over the buffer
- [buffered_reader](../io/buffered.md): lines as slices of the block; [stream](../io/stream.md): `read` and `write` take slices
- [tracked_ptr](tracked_ptr.md): the owner's word and its barrier; [README: The rules](README.md#the-rules)
- `tests/core/slice.cpp`: owners and none, subslices, the owner kept alive by a slice alone, a dead slice in a container, the text interface, the bytes, a fresh thread registered by an owned slice.
