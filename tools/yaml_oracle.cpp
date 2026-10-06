// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The oracle for the YAML of sgcl/encoding (yaml.h): libyaml 0.2 (Homebrew's)
// asked to read the same texts, the node graph it reads written out as a C++
// header the tests include:
//
//     clang++ -std=c++20 -O1 -I. -I/opt/homebrew/opt/libyaml/include tools/yaml_oracle.cpp \
//         /opt/homebrew/opt/libyaml/lib/libyaml.a -framework CoreFoundation -framework Security \
//         -framework Accelerate -framework CoreGraphics -framework ImageIO -o yaml_oracle
//     ./yaml_oracle tests/encoding/yaml/corpus.yaml-list > tests/encoding/yaml_tests.h
//
// The texts: the corpus (the examples of the spec's chapter 2 and the edge cases
// of every construct, separated by lines "#%%% name"), and 600 documents the
// writer of yaml.h writes of random values (so that libyaml reads what the
// writer writes as the same graph). For each, every document's graph: a scalar
// as the kind the core schema reads it as (n, b, i, f, s; the core tags force
// the kind, an application's tag is written before it as <tag>) and its
// content; [ ] a sequence, { } a mapping (key, value, key, value); an alias as
// the graph of its anchor. Or "error" when libyaml refuses the text.
#include "sgcl/encoding/yaml.h"

#include <yaml.h>

#include <cstdio>
#include <fstream>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace {
    std::string cstr(const std::string& s) {
        std::string out = "\"";
        for (unsigned char c : s) {
            if (c == '"' || c == '\\') {
                out += '\\';
                out += char(c);
            } else if (c >= 0x20 && c < 0x7F && c != '?') {
                out += char(c);
            } else {
                char b[8];
                std::snprintf(b, sizeof b, "\\x%02x\"\"", c);
                out += b;
            }
        }
        return out + "\"";
    }

    const std::string core = "tag:yaml.org,2002:";

    std::string escape(const std::string& s) {
        std::string out;
        for (unsigned char c : s) {
            if (c == '\\' || c == '|') {
                out += '\\';
                out += char(c);
            } else if (c == '\0') {
                out += "\\0";
            } else if (c == '\n') {
                out += "\\n";
            } else {
                out += char(c);
            }
        }
        return out;
    }

    std::string scalar_dump(const char* tag, bool plain, const std::string& value) {
        std::string t = tag ? tag : "";
        char k;
        std::string kept;
        if (t.empty()) {
            k = plain ? "nbifsqm"[int(sgcl::encoding::detail::yaml_resolve(value))] : 's';
        } else if (t == "!") {
            k = 's';
        } else if (t.compare(0, core.size(), core) == 0 && (t == core + "str" || t == core + "int" || t == core + "float" || t == core + "bool" || t == core + "null")) {
            std::string n = t.substr(core.size());
            k = n == "str" ? 's' : n == "int" ? 'i' : n == "float" ? 'f' : n == "bool" ? 'b' : 'n';
        } else {
            // an application's tag: the plain text read by the core schema all the same
            k = plain ? "nbifsqm"[int(sgcl::encoding::detail::yaml_resolve(value))] : 's';
            kept = "<" + t + ">";
        }
        return kept + k + "|" + escape(value) + "|";
    }

    // The graph of every document of the text, or "error"
    std::string read(const std::string& text) {
        yaml_parser_t parser;
        yaml_parser_initialize(&parser);
        yaml_parser_set_input_string(&parser, reinterpret_cast<const unsigned char*>(text.data()), text.size());
        std::string out;
        std::map<std::string, std::string> anchors;
        struct Open {
            std::string anchor;
            size_t start;
        };
        std::vector<Open> open;
        for (;;) {
            yaml_event_t e;
            if (!yaml_parser_parse(&parser, &e)) {
                yaml_parser_delete(&parser);
                return "error";
            }
            yaml_event_type_t type = e.type;
            switch (type) {
                case YAML_DOCUMENT_START_EVENT:
                    anchors.clear();
                    break;
                case YAML_DOCUMENT_END_EVENT:
                    out += "\n";
                    break;
                case YAML_SCALAR_EVENT: {
                    std::string v(reinterpret_cast<const char*>(e.data.scalar.value), e.data.scalar.length);
                    std::string d = scalar_dump(reinterpret_cast<const char*>(e.data.scalar.tag), e.data.scalar.style == YAML_PLAIN_SCALAR_STYLE, v);
                    if (e.data.scalar.anchor) {
                        anchors[reinterpret_cast<const char*>(e.data.scalar.anchor)] = d;
                    }
                    out += d + " ";
                    break;
                }
                case YAML_ALIAS_EVENT:
                    out += anchors[reinterpret_cast<const char*>(e.data.alias.anchor)] + " ";
                    break;
                case YAML_SEQUENCE_START_EVENT:
                case YAML_MAPPING_START_EVENT: {
                    bool seq = type == YAML_SEQUENCE_START_EVENT;
                    const char* tag = reinterpret_cast<const char*>(seq ? e.data.sequence_start.tag : e.data.mapping_start.tag);
                    const char* anchor = reinterpret_cast<const char*>(seq ? e.data.sequence_start.anchor : e.data.mapping_start.anchor);
                    std::string t = tag ? tag : "";
                    size_t start = out.size();
                    if (!t.empty() && t != "!" && t != core + (seq ? "seq" : "map")) {
                        out += "<" + t + ">";
                    }
                    out += seq ? "[ " : "{ ";
                    open.push_back({anchor ? anchor : "", start});
                    break;
                }
                case YAML_SEQUENCE_END_EVENT:
                case YAML_MAPPING_END_EVENT: {
                    out += type == YAML_SEQUENCE_END_EVENT ? "] " : "} ";
                    Open o = open.back();
                    open.pop_back();
                    if (!o.anchor.empty()) {
                        std::string d = out.substr(o.start);
                        anchors[o.anchor] = d.substr(0, d.size() - 1);
                    }
                    break;
                }
                default:
                    break;
            }
            yaml_event_delete(&e);
            if (type == YAML_STREAM_END_EVENT) {
                break;
            }
        }
        yaml_parser_delete(&parser);
        return out;
    }

    using sgcl::encoding::yaml;

    yaml random_value(std::mt19937_64& r, int depth) {
        static const char* strings[] = {"plain", "two words", "", " lead", "trail ", "a: b", "a #b", "#c", "- x", "? y", ": z",
                                        "true", "null", "12", "1.5", "0x1F", ".inf", "multi\nline", "multi\nline\n", "end\n\n",
                                        "\ttab", "quote\"s", "it's", "zażółć", "日本", "[flow]", "{x}", "a,b", "*a", "&b", "!c",
                                        "|d", ">e", "%f", "@g", "`h", "---", "...", "x\r\ny", "\x01", " \n indented", "back\\slash"};
        int k = int(r() % (depth > 3 ? 6 : 8));
        switch (k) {
            case 0: return yaml();
            case 1: return yaml(bool(r() & 1));
            case 2: return yaml(int64_t(r()) >> (r() % 64));
            case 3: return yaml(double(int64_t(r() % 100000)) / 64.0);
            case 4:
            case 5: return yaml(sgcl::string(strings[r() % (sizeof strings / sizeof *strings)]));
            case 6: {
                sgcl::vector<yaml> items;
                for (int i = int(r() % 4); i > 0; --i) {
                    items.push_back(random_value(r, depth + 1));
                }
                return yaml::sequence(items);
            }
            default: {
                sgcl::vector<yaml::member> ms;
                for (int i = int(r() % 4); i > 0; --i) {
                    yaml key = r() % 6 == 0 ? random_value(r, depth + 2) : yaml(sgcl::string(strings[r() % (sizeof strings / sizeof *strings)]));
                    bool dup = false;
                    for (auto& m : ms) {
                        dup = dup || m.key == key;
                    }
                    if (!dup) {
                        ms.push_back(yaml::member{key, random_value(r, depth + 1)});
                    }
                }
                return yaml::mapping(ms);
            }
        }
    }
}

int main(int argc, char** argv) {
    std::ifstream f(argv[1], std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    std::string all = ss.str();
    std::vector<std::pair<std::string, std::string>> cases;
    size_t at = 0;
    while ((at = all.find("#%%% ", at)) != std::string::npos) {
        size_t nl = all.find('\n', at);
        std::string name = all.substr(at + 5, nl - at - 5);
        size_t next = all.find("\n#%%% ", nl);
        if (next == std::string::npos || name == "end") {
            break;
        }
        cases.push_back({name, all.substr(nl + 1, next - nl)});
        at = next + 1;
    }
    std::mt19937_64 r(20261006);
    for (int i = 0; i < 600; ++i) {
        yaml v = random_value(r, 0);
        cases.push_back({"written " + std::to_string(i), std::string(v.to_string().view())});
    }
    std::printf("// Generated by tools/yaml_oracle.cpp (libyaml %s): do not edit.\n#pragma once\n#include <string_view>\nnamespace yaml_oracle {\n", yaml_get_version_string());
    std::printf("struct read_case { std::string_view name; std::string_view text; std::string_view graph; };\ninline constexpr read_case reads[] = {\n");
    for (auto& [name, text] : cases) {
        std::printf("    {%s, %s, %s},\n", cstr(name).c_str(), cstr(text).c_str(), cstr(read(text)).c_str());
    }
    std::printf("};\n}\n");
}
