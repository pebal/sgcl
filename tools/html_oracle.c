// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The oracle of sgcl/txt/html.h: gumbo-parser (Homebrew, 0.13) parses the same
// documents and prints their trees in html5lib's test format. Only a tool,
// never in the library; tools/html_vectors.py runs it.
//
//   cc -O1 -I/opt/homebrew/opt/gumbo-parser/include tools/html_oracle.c \
//      -L/opt/homebrew/opt/gumbo-parser/lib -lgumbo -o /tmp/html_oracle
//
// Reads documents from stdin, one a line, with \\ \n \r \t \0 escaped; prints
// each tree, then a line "#end".
#include <gumbo.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void indent(int depth) {
    printf("| ");
    for (int i = 0; i < depth; ++i) {
        printf("  ");
    }
}

static void print_text(const char* s) {
    fputs(s, stdout);
}

static int cmp_attr(const void* a, const void* b) {
    const GumboAttribute* x = *(const GumboAttribute* const*)a;
    const GumboAttribute* y = *(const GumboAttribute* const*)b;
    char nx[512], ny[512];
    const char* px = x->attr_namespace == GUMBO_ATTR_NAMESPACE_XLINK ? "xlink " : x->attr_namespace == GUMBO_ATTR_NAMESPACE_XML ? "xml " : x->attr_namespace == GUMBO_ATTR_NAMESPACE_XMLNS ? "xmlns " : "";
    const char* py = y->attr_namespace == GUMBO_ATTR_NAMESPACE_XLINK ? "xlink " : y->attr_namespace == GUMBO_ATTR_NAMESPACE_XML ? "xml " : y->attr_namespace == GUMBO_ATTR_NAMESPACE_XMLNS ? "xmlns " : "";
    snprintf(nx, sizeof nx, "%s%s", px, x->name);
    snprintf(ny, sizeof ny, "%s%s", py, y->name);
    return strcmp(nx, ny);
}

static void tag_name(const GumboElement* e, char* out, size_t n) {
    if (e->tag != GUMBO_TAG_UNKNOWN) {
        snprintf(out, n, "%s", gumbo_normalized_tagname(e->tag));
    } else {
        GumboStringPiece p = e->original_tag;
        gumbo_tag_from_original_text(&p);
        size_t k = p.length < n - 1 ? p.length : n - 1;
        for (size_t i = 0; i < k; ++i) {
            char c = p.data[i];
            out[i] = c >= 'A' && c <= 'Z' ? c + 32 : c;
        }
        out[k] = 0;
    }
    if (e->tag_namespace == GUMBO_NAMESPACE_SVG) {
        GumboStringPiece p = e->original_tag;
        gumbo_tag_from_original_text(&p);
        const char* fixed = gumbo_normalize_svg_tagname(&p);
        if (fixed) {
            snprintf(out, n, "%s", fixed);
        }
    }
}

static void dump(const GumboNode* node, int depth) {
    switch (node->type) {
        case GUMBO_NODE_ELEMENT:
        case GUMBO_NODE_TEMPLATE: {
            const GumboElement* e = &node->v.element;
            char name[512];
            tag_name(e, name, sizeof name);
            indent(depth);
            if (e->tag_namespace == GUMBO_NAMESPACE_SVG) {
                printf("<svg %s>\n", name);
            } else if (e->tag_namespace == GUMBO_NAMESPACE_MATHML) {
                printf("<math %s>\n", name);
            } else {
                printf("<%s>\n", name);
            }
            const GumboAttribute** attrs = malloc(sizeof(GumboAttribute*) * (e->attributes.length + 1));
            for (unsigned i = 0; i < e->attributes.length; ++i) {
                attrs[i] = e->attributes.data[i];
            }
            qsort(attrs, e->attributes.length, sizeof(GumboAttribute*), cmp_attr);
            for (unsigned i = 0; i < e->attributes.length; ++i) {
                const GumboAttribute* a = attrs[i];
                indent(depth + 1);
                const char* p = a->attr_namespace == GUMBO_ATTR_NAMESPACE_XLINK ? "xlink " : a->attr_namespace == GUMBO_ATTR_NAMESPACE_XML ? "xml " : a->attr_namespace == GUMBO_ATTR_NAMESPACE_XMLNS ? "xmlns " : "";
                const char* nm = a->name;
                if (*p) {
                    const char* colon = strchr(nm, ':');
                    if (colon) {
                        nm = colon + 1;
                    }
                }
                printf("%s%s=\"", p, nm);
                print_text(a->value);
                printf("\"\n");
            }
            free((void*)attrs);
            int child_depth = depth + 1;
            if (node->type == GUMBO_NODE_TEMPLATE) {
                indent(depth + 1);
                printf("content\n");
                child_depth = depth + 2;
            }
            for (unsigned i = 0; i < e->children.length; ++i) {
                dump(e->children.data[i], child_depth);
            }
            break;
        }
        case GUMBO_NODE_TEXT:
        case GUMBO_NODE_WHITESPACE:
        case GUMBO_NODE_CDATA:
            indent(depth);
            printf("\"");
            print_text(node->v.text.text);
            printf("\"\n");
            break;
        case GUMBO_NODE_COMMENT:
            indent(depth);
            printf("<!-- ");
            print_text(node->v.text.text);
            printf(" -->\n");
            break;
        default:
            break;
    }
}

static size_t unescape(const char* s, size_t n, char* out) {
    size_t o = 0;
    for (size_t i = 0; i < n; ++i) {
        if (s[i] == '\\' && i + 1 < n) {
            char c = s[++i];
            out[o++] = c == 'n' ? '\n' : c == 'r' ? '\r' : c == 't' ? '\t' : c == '0' ? '\0' : c;
        } else {
            out[o++] = s[i];
        }
    }
    return o;
}

int main(void) {
    static char line[1 << 20], doc[1 << 20];
    while (fgets(line, sizeof line, stdin)) {
        size_t len = strlen(line);
        if (len && line[len - 1] == '\n') {
            line[--len] = 0;
        }
        size_t n = unescape(line, len, doc);
        GumboOutput* out = gumbo_parse_with_options(&kGumboDefaultOptions, doc, n);
        const GumboDocument* d = &out->document->v.document;
        if (d->has_doctype) {
            indent(0);
            if (*d->public_identifier || *d->system_identifier) {
                printf("<!DOCTYPE %s \"%s\" \"%s\">\n", d->name, d->public_identifier, d->system_identifier);
            } else {
                printf("<!DOCTYPE %s>\n", d->name);
            }
        }
        for (unsigned i = 0; i < d->children.length; ++i) {
            dump(d->children.data[i], 0);
        }
        printf("#end\n");
        gumbo_destroy_output(&kGumboDefaultOptions, out);
    }
    return 0;
}
