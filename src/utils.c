/*
 * Copyright (c) 2011 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * Licensed under the Apache License, Version 2.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "utils.h"
#define STRING_MAXLEN 1024
char*
buff_addc (char*  buff, char*  buffEnd, int  c)
{
    int  avail = buffEnd - buff;

    if (avail <= 0)  /* already in overflow mode */
        return buff;

    if (avail == 1) {  /* overflowing, the last byte is reserved for zero */
        buff[0] = 0;
        return buff + 1;
    }

    buff[0] = (char) c;  /* add char and terminating zero */
    buff[1] = 0;
    return buff + 1;
}

char*
buff_adds (char*  buff, char*  buffEnd, const char*  s)
{
    int  slen = strlen(s);

    return buff_addb(buff, buffEnd, s, slen);
}

char*
buff_addb (char*  buff, char*  buffEnd, const void*  data, int  len)
{
    int  avail = (buffEnd - buff);

    if (avail <= 0 || len <= 0)  /* already overflowing */
        return buff;

    if (len > avail)
        len = avail;

    memcpy(buff, data, len);

    buff += len;

    /* ensure there is a terminating zero */
    if (buff >= buffEnd) {  /* overflow */
        buff[-1] = 0;
    } else
        buff[0] = 0;

    return buff;
}

char*
buff_add  (char*  buff, char*  buffEnd, const char*  format, ... )
{
    int      avail;

    avail = (buffEnd - buff);

    if (avail > 0) {
        va_list  args;
        int      nn;

        va_start(args, format);
        nn = vsnprintf( buff, avail, format, args);
        va_end(args);

        if (nn < 0) {
            /* some C libraries return -1 in case of overflow,
             * but they will also do that if the format spec is
             * invalid. We assume SDB is not buggy enough to
             * trigger that last case. */
            nn = avail;
        }
        else if (nn > avail) {
            nn = avail;
        }

        buff += nn;

        /* ensure that there is a terminating zero */
        if (buff >= buffEnd)
            buff[-1] = 0;
        else
            buff[0] = 0;
    }
    return buff;
}

char *str_trim(const char* string)
{
    const char* s = string;
    const char* e = string + (strlen(string) - 1);
    char* ret;

    while(*s == ' ' || *s == '\t') { // ltrim
        s++;
    }
    while(*e == ' ' || *e == '\t') { // rtrim
        e--;
    }

    ret = strdup(s);
    ret[e - s + 1] = 0;

    return  ret;
}

/**
 * The standard strncpy() function does not guarantee that the resulting string is null terminated.
 * char ntbs[NTBS_SIZE];
 * strncpy(ntbs, source, sizeof(ntbs)-1);
 * ntbs[sizeof(ntbs)-1] = '\0'
 */
char *s_strncpy(char *dest, const char *source, size_t n) {
  char *start = dest;

  while (n && (*dest++ = *source++)) {
      n--;
  }
  if (n) {
      while (--n) {
          *dest++ = '\0';
      }
  }
  return start;
}
