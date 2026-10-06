//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/string.h"
#include "../core/tracked_ptr.h"
#include "../core/utf8.h"
#include "properties.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Elastic tabstops as Go's text/tabwriter sets them: cells ended by tabs,
// the tab-ended cells of consecutive lines a column as wide as its widest
// cell and the padding.
//
//     print("{}", txt::align_tabs("name\tsize\tkind\nmain.cpp\t1204\tsource\n"));
//
// A cell is as wide as a terminal shows it: an East Asian wide character two
// columns, a combining mark none, where Go counts code points; for text of
// one column a character the output is Go's byte for byte.
namespace sgcl::txt {
    struct tab_options {
        size_t min_width = 0;
        size_t tab_width = 8;
        size_t padding = 1;
        char pad_char = ' ';
        bool align_right = false;
        bool discard_empty_columns = false;
        bool tab_indent = false;
        bool filter_html = false;
        bool strip_escape = false;
        bool debug = false;
    };

    namespace detail {
        struct TabCell {
            size_t size = 0;      // bytes of its text in the buffer
            long width = 0;       // columns (an unclosed escape may take it below zero, as in Go)
            bool htab = false;    // ended by '\t', not '\v'
        };

        // The columns of a piece of a cell: a code point below U+0080 one
        // (a control too, as Go counts it), the rest as a terminal shows it
        inline long tab_text_width(std::string_view s) noexcept {
            long n = 0;
            size_t i = 0;
            while (i < s.size()) {
                size_t run = utf8::ascii_run(s, i);
                n += long(run);
                i += run;
                if (i >= s.size()) {
                    break;
                }
                auto [c, width] = utf8::decode(s, i);
                n += long(columns_fn(c));
                i += width;
            }
            return n;
        }

        class TabCore {
        public:
            explicit TabCore(const tab_options& o)
            : _o(o) {
                if (_o.pad_char == '\t') {
                    _o.align_right = false;   // tab padding aligns left
                }
                _line_start.push_back(0);
                for (unsigned char c : {'\t', '\v', '\n', '\f', '\xFF'}) {
                    _special[c] = true;
                }
                if (_o.filter_html) {
                    _special['<'] = _special['&'] = true;
                }
            }

            void write(std::string_view text, std::string& out) {
                size_t n = 0;
                for (size_t i = 0; i < text.size(); ++i) {
                    // to the next byte that means something here
                    if (_end == 0) {
                        while (i < text.size() && !_special[static_cast<unsigned char>(text[i])]) {
                            ++i;
                        }
                    } else {
                        size_t at = text.find(char(_end), i);
                        i = at == std::string_view::npos ? text.size() : at;
                    }
                    if (i == text.size()) {
                        break;
                    }
                    unsigned char ch = static_cast<unsigned char>(text[i]);
                    if (_end == 0) {
                        if (ch == '\t' || ch == '\v' || ch == '\n' || ch == '\f') {
                            append(text.substr(n, i - n));
                            update_width();
                            n = i + 1;
                            size_t cells = terminate_cell(ch == '\t');
                            if (ch == '\n' || ch == '\f') {
                                _line_start.push_back(_cells.size());
                                if (ch == '\f' || cells == 1) {
                                    // a line of one cell ends the block of columns; a form feed ends it always
                                    flush(out);
                                    if (ch == '\f' && _o.debug) {
                                        out += "---\n";
                                    }
                                }
                            }
                        } else if (ch == 0xFF) {
                            append(text.substr(n, i - n));
                            update_width();
                            n = _o.strip_escape ? i + 1 : i;
                            _end = 0xFF;
                        } else if (_o.filter_html && (ch == '<' || ch == '&')) {
                            append(text.substr(n, i - n));
                            update_width();
                            n = i;
                            _end = ch == '<' ? '>' : ';';
                        }
                    } else if (ch == _end) {
                        size_t j = ch == 0xFF && _o.strip_escape ? i : i + 1;
                        append(text.substr(n, j - n));
                        n = i + 1;
                        end_escape();
                    }
                }
                append(text.substr(n));
            }

            // the lines held, aligned, and a fresh block
            void flush(std::string& out) {
                if (_cell.size > 0) {
                    if (_end != 0) {
                        end_escape();
                    }
                    terminate_cell(false);
                }
                format(out);
                _buf.clear();
                _pos = 0;
                _cell = TabCell{};
                _end = 0;
                _cells.clear();
                _line_start.assign(1, 0);
            }

        private:
            tab_options _o;
            bool _special[256] = {};            // the bytes that end a cell or start an escape, a tag, an entity
            std::string _buf;
            size_t _pos = 0;                    // the start of the text not yet measured
            TabCell _cell;                      // the cell being read
            unsigned char _end = 0;             // the byte that ends an escape, a tag or an entity; 0 outside
            std::vector<TabCell> _cells;        // of every line, in order
            std::vector<size_t> _line_start;    // the first cell of each line

            void append(std::string_view s) {
                _buf.append(s);
                _cell.size += s.size();
            }

            void update_width() {
                _cell.width += tab_text_width(std::string_view(_buf).substr(_pos));
                _pos = _buf.size();
            }

            void end_escape() {
                if (_end == 0xFF) {
                    update_width();
                    if (!_o.strip_escape) {
                        _cell.width -= 2;   // the escape bytes take no room
                    }
                } else if (_end == ';') {
                    ++_cell.width;          // an entity is one character
                }                           // a tag takes none
                _pos = _buf.size();
                _end = 0;
            }

            size_t terminate_cell(bool htab) {
                _cell.htab = htab;
                _cells.push_back(_cell);
                _cell = TabCell{};
                return _cells.size() - _line_start.back();
            }

            void pad(std::string& out, long text, long cell, bool tabs) const {
                if (_o.pad_char == '\t' || tabs) {
                    if (_o.tab_width == 0) {
                        return;   // tabs of no width pad nothing
                    }
                    long tw = long(_o.tab_width);
                    cell = (cell + tw - 1) / tw * tw;
                    long n = cell - text;
                    if (n > 0) {
                        out.append(size_t((n + tw - 1) / tw), '\t');
                    }
                    return;
                }
                if (cell > text) {
                    out.append(size_t(cell - text), _o.pad_char);
                }
            }

            // Go's format, without its recursion: the block of column j is
            // a run of consecutive lines with a tab-ended cell at j (a
            // line's last cell ends at its line break and is in no column),
            // as wide as its widest cell and the padding
            void format(std::string& out) {
                size_t lines = _line_start.size();
                auto cells_of = [&](size_t i) {
                    return (i + 1 < lines ? _line_start[i + 1] : _cells.size()) - _line_start[i];
                };
                struct Block {
                    long width;
                    bool discardable;
                };
                std::vector<Block> blocks;
                std::vector<size_t> block_of(_cells.size(), 0);   // of a tab-ended cell
                std::vector<size_t> open;                          // the block of each column, by column
                auto close = [&](size_t keep) {
                    while (open.size() > keep) {
                        Block& b = blocks[open.back()];
                        if (b.discardable && _o.discard_empty_columns) {
                            b.width = 0;
                        }
                        open.pop_back();
                    }
                };
                for (size_t i = 0; i < lines; ++i) {
                    size_t n = cells_of(i), columns = n > 0 ? n - 1 : 0;
                    close(columns);
                    for (size_t j = 0; j < columns; ++j) {
                        if (j == open.size()) {
                            open.push_back(blocks.size());
                            blocks.push_back(Block{long(_o.min_width), true});
                        }
                        const TabCell& c = _cells[_line_start[i] + j];
                        Block& b = blocks[open[j]];
                        b.width = std::max(b.width, c.width + long(_o.padding));
                        if (c.width > 0 || c.htab) {
                            b.discardable = false;
                        }
                        block_of[_line_start[i] + j] = open[j];
                    }
                }
                close(0);
                size_t pos = 0;
                for (size_t i = 0; i < lines; ++i) {
                    size_t n = cells_of(i), columns = n > 0 ? n - 1 : 0;
                    bool tabs = _o.tab_indent;   // leading empty cells padded with tabs
                    for (size_t j = 0; j < n; ++j) {
                        size_t k = _line_start[i] + j;
                        const TabCell& c = _cells[k];
                        if (j > 0 && _o.debug) {
                            out.push_back('|');
                        }
                        if (c.size == 0) {
                            if (j < columns) {
                                pad(out, c.width, blocks[block_of[k]].width, tabs);
                            }
                            continue;
                        }
                        tabs = false;
                        if (!_o.align_right) {
                            out.append(_buf, pos, c.size);
                            if (j < columns) {
                                pad(out, c.width, blocks[block_of[k]].width, false);
                            }
                        } else {
                            if (j < columns) {
                                pad(out, c.width, blocks[block_of[k]].width, false);
                            }
                            out.append(_buf, pos, c.size);
                        }
                        pos += c.size;
                    }
                    if (i + 1 == lines) {
                        out.append(_buf, pos, _cell.size);   // the cell being read, which has no line break yet
                        pos += _cell.size;
                    } else {
                        out.push_back('\n');
                    }
                }
            }
        };

        struct TabState {
            TabCore core;

            explicit TabState(const tab_options& o)
            : core(o) {
            }
        };
    }

    // The whole text aligned: cells end at '\t' (or '\v'), lines at '\n'
    // (or '\f', which also ends every column)
    inline string align_tabs(const string& text, const tab_options& o = {}) {
        detail::TabCore core(o);
        std::string out;
        out.reserve(text.size() + text.size() / 2);
        core.write(text.view(), out);
        core.flush(out);
        return string(std::string_view(out));
    }

    // The same, streamed: text held until its block of columns ends, then
    // given back aligned. A handle: copies share the writer
    class tab_writer {
    public:
        SGCL_INLINE_HOT explicit tab_writer(const tab_options& o = {})
        : _state(make_tracked<detail::TabState>(o)) {
        }

        // The lines whose block of columns the text ended, aligned; often
        // nothing
        string write(const string& text) {
            std::string out;
            _state->core.write(text.view(), out);
            return string(std::string_view(out));
        }

        // Everything still held, aligned: the block in hand ends
        string flush() {
            std::string out;
            _state->core.flush(out);
            return string(std::string_view(out));
        }

    private:
        tracked_ptr<detail::TabState> _state;
    };
}
