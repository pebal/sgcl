//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The traces of RFC 8448 (Example Handshake Traces for TLS 1.3) read from
// ~/Programming/oracles/rfc8448/rfc8448.txt (or $SGCL_ORACLES): every step
// ("{server}  send handshake record:") with its fields ("payload",
// "complete record", ...) as bytes, in the order of the document. Shared
// by the tests of sgcl/net/tls.
#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace rfc8448 {
    using bytes_t = std::vector<uint8_t>;

    // One step of a trace: "{server}  derive secret "tls13 c hs traffic":" and
    // its fields ("PRK", "hash", "expanded", ...), each as bytes
    struct Step {
        int section = 0;
        std::string who, title;
        std::map<std::string, bytes_t> fields;
    };

    inline std::vector<Step> read() {
        std::string root;
        if (const char* o = std::getenv("SGCL_ORACLES")) {
            root = o;
        } else if (const char* home = std::getenv("HOME")) {
            root = std::string(home) + "/Programming/oracles";
        }
        std::ifstream in(root + "/rfc8448/rfc8448.txt");
        std::vector<Step> steps;
        if (!in) {
            return steps;
        }
        std::regex section(R"(^(\d+)\.  )"), step(R"(^   \{(client|server)\}  (.*))"), field(R"(^      ([^ ].*?) \((\d+) octets\):\s*(.*)$)");
        std::string line, name;
        int current = 0;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            std::smatch m;
            if (std::regex_search(line, m, section)) {
                current = std::stoi(m[1]);
                name.clear();
                continue;
            }
            if (std::regex_match(line, m, step)) {
                steps.push_back({current, m[1], m[2], {}});
                name.clear();
                continue;
            }
            if (steps.empty()) {
                continue;
            }
            auto add = [&](const std::string& text) {
                std::istringstream words(text);
                std::string w;
                while (words >> w) {
                    if (w.size() == 2 && std::isxdigit((unsigned char)w[0]) && std::isxdigit((unsigned char)w[1])) {
                        steps.back().fields[name].push_back(uint8_t(std::stoi(w, nullptr, 16)));
                    }
                }
            };
            if (std::regex_match(line, m, field)) {
                name = m[1];
                steps.back().fields[name];
                add(m[3]);
            } else if (!name.empty() && line.rfind("         ", 0) == 0) {
                add(line);
            } else if (line.rfind("Thomson", 0) == 0 || line.rfind("RFC 8448", 0) == 0 || line.empty() || line[0] == '\f') {
                // a page's break: the field goes on after it
            } else {
                name.clear();
            }
        }
        return steps;
    }

    inline const Step* find(const std::vector<Step>& steps, int section, const std::string& who, const std::string& title) {
        for (auto& s : steps) {
            if (s.section == section && s.who == who && s.title.rfind(title, 0) == 0) {
                return &s;
            }
        }
        return nullptr;
    }

    // Every handshake message of the traces: the section, who sent it and
    // its bytes (the "construct a X handshake message" steps, field X)
    struct Message {
        int section;
        std::string who, name;
        bytes_t bytes;
    };

    inline std::vector<Message> messages(const std::vector<Step>& steps) {
        std::vector<Message> out;
        std::regex construct(R"(^construct an? (\w+) handshake message)");
        for (size_t i = 0; i < steps.size(); ++i) {
            auto& s = steps[i];
            std::smatch m;
            if (!std::regex_search(s.title, m, construct)) {
                continue;
            }
            auto f = s.fields.find(m[1]);
            if (f == s.fields.end() || f->second.size() < 4) {
                continue;
            }
            auto bytes = f->second;
            size_t whole = 4 + (size_t(bytes[1]) << 16 | size_t(bytes[2]) << 8 | size_t(bytes[3]));
            // §4's ClientHello is given as the prefix its binders are
            // computed over: the whole message is the payload of the record
            // sent after it
            for (size_t j = i + 1; bytes.size() < whole && j < steps.size(); ++j) {
                auto p = steps[j].fields.find("payload");
                if (p != steps[j].fields.end() && p->second.size() >= whole && std::equal(bytes.begin(), bytes.end(), p->second.begin())) {
                    bytes.assign(p->second.begin(), p->second.begin() + long(whole));
                }
            }
            out.push_back({s.section, s.who, m[1], bytes});
        }
        return out;
    }
}
