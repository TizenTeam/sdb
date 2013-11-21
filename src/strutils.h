#ifndef _STRUTILS_H_
#define _STRUTILS_H_

#define MAX_TOKENS 30

size_t tokenize(const char *str, const char *delim, char *tokens[], size_t max_tokens);
void free_strings(char **array, int n);
int read_lines(const int fd, char* ptr, const unsigned int maxlen);
int read_line(const int fd, char* ptr, const unsigned int maxlen);
char *s_strncpy(char *dest, const char *source, size_t n);
size_t s_strnlen(const char *s, size_t maxlen);
#endif

