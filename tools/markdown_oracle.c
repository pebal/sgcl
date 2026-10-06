// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The oracle of sgcl/txt/markdown.h: md4c 0.5 (Homebrew), CommonMark 0.31
// compliant, asked the same questions. Only a tool, never in the library;
// tools/markdown_vectors.py runs it over the cases it writes.
//
// A question is a line: the mode (c: CommonMark, g: GitHub's dialect —
// tables, strikethrough, task lists, permissive autolinks) and the text in
// hex; the answer is the HTML (XHTML voids) in hex.
//
//   cc -O1 tools/markdown_oracle.c -I/opt/homebrew/include -L/opt/homebrew/lib -lmd4c-html -lmd4c -o markdown_oracle
#include <md4c-html.h>
#include <md4c.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char* data;
    size_t size, cap;
} Buffer;

static void put(const MD_CHAR* text, MD_SIZE size, void* user) {
    Buffer* b = (Buffer*)user;
    if (b->size + size > b->cap) {
        b->cap = (b->size + size) * 2 + 64;
        b->data = realloc(b->data, b->cap);
    }
    memcpy(b->data + b->size, text, size);
    b->size += size;
}

static int nibble(char c) {
    return c <= '9' ? c - '0' : (c | 32) - 'a' + 10;
}

int main(void) {
    size_t cap = 1 << 20;
    char* line = malloc(cap);
    while (fgets(line, (int)cap, stdin)) {
        size_t n = strlen(line);
        while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) {
            line[--n] = 0;
        }
        char mode = line[0];
        const char* hex = line + 2;
        size_t len = strlen(hex) / 2;
        char* text = malloc(len + 1);
        for (size_t i = 0; i < len; ++i) {
            text[i] = (char)(nibble(hex[2 * i]) * 16 + nibble(hex[2 * i + 1]));
        }
        Buffer b = {0, 0, 0};
        unsigned flags = mode == 'g' ? MD_DIALECT_GITHUB : MD_DIALECT_COMMONMARK;
        md_html(text, (MD_SIZE)len, put, &b, flags, MD_HTML_FLAG_XHTML);
        for (size_t i = 0; i < b.size; ++i) {
            printf("%02x", (unsigned char)b.data[i]);
        }
        printf("\n");
        free(b.data);
        free(text);
    }
    return 0;
}
