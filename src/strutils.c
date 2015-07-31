#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "strutils.h"
#include "utils.h"
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

size_t tokenize(const char *str, const char *delim, char *tokens[], size_t max_tokens ) {
    int cnt = 0;

    char tmp[PATH_MAX];

    strncpy(tmp, str, PATH_MAX - 1);
    char *p = strtok(tmp, delim);
    if (max_tokens < 1 || max_tokens > MAX_TOKENS) {
        max_tokens = MAX_TOKENS;
    }

    if (p != NULL) {
        tokens[cnt++] = strdup(p);
        while(cnt < max_tokens && p != NULL) {
            p = strtok(NULL, delim);
            if (p != NULL) {
                tokens[cnt++] = strdup(p);
            }
        }
    }
    return cnt;
}

void free_strings(char **array, int n)
{
    int i;

    for(i = 0; i < n; i++) {
        SAFE_FREE(array[i]);
    }
}

int read_lines(const int fd, char* ptr, unsigned int maxlen)
{
    int lines = 0;
    while (1) {
        int len = read_line(fd, ptr, maxlen);
        if(len < 0) {
            break;
        }
        ptr += len;
        *ptr++ = '\n';
        len++;
        maxlen -= len;
        lines++;
    }
    return lines;
}

int read_line(const int fd, char* ptr, const unsigned int maxlen)
{
    unsigned int n = 0;
    char c[2];

    while(n != maxlen) {
        if(sdb_read(fd, c, 1) != 1) {
            break;
        }
        if(*c == '\r') {
            continue;
        }
        if(*c == '\n') {
            ptr[n] = 0;
            return n;
        }
        ptr[n++] = *c;
    }
    return -1; // no space
}

/**
 * The standard strncpy() function does not guarantee that the resulting string is null terminated.
 * char ntbs[NTBS_SIZE];
 * strncpy(ntbs, source, sizeof(ntbs)-1);
 * ntbs[sizeof(ntbs)-1] = '\0'
 */
char *s_strncpy(char *dest, const char *source, size_t n) {

    char *start = dest;

    if (n) {
        while (--n) {
            if (*source == '\0') {
                break;
            }
            *dest++ = *source++;
        }
        *dest = '\0';
    }

    return start;
}

/**
 * Mingw doesn't have strnlen.
 */
size_t s_strnlen(const char *s, size_t maxlen) {
    size_t len;
    for (len = 0; len < maxlen; len++, s++) {
       if (!*s) {
            break;
       }
    }
    return len;
}

char* strlchr(const char*s, int chr) {
    if(s == NULL) {
        return NULL;
    }
    int len = s_strnlen(s, PATH_MAX);
    int i = len - 1;
    for(; i>-1; i--) {
        if(s[i] == chr) {
            fflush(stdout);
            return (char*)(s + i);
        }
    }

    return NULL;
}

char* trim(char *s) {
    rtrim(s);
    return ltrim(s);
}

void rtrim(char* s) {

    int len = s_strnlen(s, PATH_MAX) - 1;
    char* t = s + len;

    for(; len > -1; len--) {
        if(*t != ' ') {
            *(t+1) = '\0';
            break;
        }
        t--;
    }
}

char* ltrim(char *s) {
    char* begin;
    begin = s;

    while (*begin != '\0') {
        if (*begin == ' ')
            begin++;
        else {
            s = begin;
            break;
        }
    }

    return s;
}
