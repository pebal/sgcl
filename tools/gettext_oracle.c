// The answers of GNU libintl to gettext lookups, for tools/gettext_vectors.py
// (the oracle of tests/txt/catalog.cpp): not part of the build.
//
//   cc -I/opt/homebrew/opt/gettext/include tools/gettext_oracle.c \
//      -L/opt/homebrew/opt/gettext/lib -lintl -o gettext_oracle
//   LANGUAGE=xx gettext_oracle <localedir> < queries > answers
//
// A query is a line of tab-separated fields, each with \\ \t \n \xHH escaped:
// domain, kind (g p n np), context, id, plural id, n. The .mo of a domain is
// <localedir>/xx/LC_MESSAGES/<domain>.mo. pgettext and npgettext are what
// gettext.h makes of them: the key context EOT id, and the id or the plural
// id when libintl hands the key back.
#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t unescape(const char* s, size_t len, char* out) {
    size_t o = 0;
    for (size_t i = 0; i < len; ++i) {
        if (s[i] != '\\' || i + 1 >= len) {
            out[o++] = s[i];
            continue;
        }
        char c = s[++i];
        if (c == 't') out[o++] = '\t';
        else if (c == 'n') out[o++] = '\n';
        else if (c == 'x' && i + 2 < len) {
            char h[3] = {s[i + 1], s[i + 2], 0};
            out[o++] = (char)strtol(h, 0, 16);
            i += 2;
        } else out[o++] = c;
    }
    out[o] = 0;
    return o;
}

static void print_escaped(const char* s) {
    for (; *s; ++s) {
        unsigned char c = (unsigned char)*s;
        if (c == '\\') fputs("\\\\", stdout);
        else if (c == '\t') fputs("\\t", stdout);
        else if (c == '\n') fputs("\\n", stdout);
        else if (c < 0x20) printf("\\x%02x", c);
        else putchar(c);
    }
    putchar('\n');
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: LANGUAGE=xx gettext_oracle <localedir> < queries\n");
        return 2;
    }
    setlocale(LC_ALL, "en_US.UTF-8");
    static char line[1 << 20];
    static char field[6][1 << 18];
    static char key[1 << 19];
    static char bound[4096][64];
    int nbound = 0;
    while (fgets(line, sizeof line, stdin)) {
        size_t len = strlen(line);
        if (len && line[len - 1] == '\n') line[--len] = 0;
        char* p = line;
        for (int f = 0; f < 6; ++f) {
            char* tab = f < 5 ? strchr(p, '\t') : 0;
            size_t n = tab ? (size_t)(tab - p) : strlen(p);
            unescape(p, n, field[f]);
            p = tab ? tab + 1 : p + n;
        }
        const char* domain = field[0];
        int known = 0;
        for (int i = 0; i < nbound; ++i) known |= !strcmp(bound[i], domain);
        if (!known && nbound < 4096) {
            bindtextdomain(domain, argv[1]);
            bind_textdomain_codeset(domain, "UTF-8");
            strcpy(bound[nbound++], domain);
        }
        const char* kind = field[1];
        const char *ctx = field[2], *id = field[3], *plural = field[4];
        unsigned long n = strtoul(field[5], 0, 10);
        const char* r;
        if (!strcmp(kind, "g")) {
            r = dgettext(domain, id);
        } else if (!strcmp(kind, "n")) {
            r = dngettext(domain, id, plural, n);
        } else {
            snprintf(key, sizeof key, "%s\004%s", ctx, id);
            if (!strcmp(kind, "p")) {
                r = dgettext(domain, key);
                if (r == key) r = id;
            } else {
                r = dngettext(domain, key, plural, n);
                if (r == key || r == plural) r = n == 1 ? id : plural;
            }
        }
        print_escaped(r);
    }
    return 0;
}
