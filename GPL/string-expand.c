/*
 * File:    string-expand.c
 * Author:  Ulf Samuelsson <ulf@emagii.com>
   Copyright (C) 1991-2023 Ulf Samuelsson <ulf@emagii.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston,
   MA 02110-1301, USA.  */

#define _GNU_SOURCE
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <libiberty/dyn-string.h>
#include "string-expand.h"

#define	DEBUG 0
#define MAX_STRING 100

#if (DEBUG)
#define BEGIN  printf("%s BEGIN\n", __FUNCTION__)
#define END    printf("%s END\n",   __FUNCTION__)
#define TAG(x) printf("%s c = '%c'\n",   __FUNCTION__, (x))
#define dbg_printf(...)   printf(__VA_ARGS__)
#else
#define BEGIN
#define END
#define TAG(x)
#define dbg_printf(...)
#endif

#ifdef VMS
static char slash = '\0';
static char bad_slash = '\0';
#else
#if defined (_WIN32) && !defined (__CYGWIN32__)
const char slash = '\\';
const char bad_slash = '/';
#else
const char slash = '/';
const char bad_slash = '\\';
#endif
#endif

static char *
replace_slash(char *s)
{
  uint32_t len = strlen(s);
  for (int i = 0; i < len ; i++)
    {
      if (s[i] == bad_slash)
        {
          s[i] = slash;
        }
      if (i > 100) printf("bad i\n");
    }
  return s;
}

#if !defined(_GNU_SOURCE)
#ifdef VMS
#define basename(s) (s)
#else
char *basename(char *s)
{
  size_t len = strlen(s);
  char marker = *slash;

  for (uint32_t i = len-1 ; len > 0 ; len--)
    {
      if (s[i] == marker)
        {
          return &s[len++];
        }
    }
}
#endif
#endif

/*
  In: s: points at writeable string which starts with "$"
  if string is ${XXX}... the function will return getenv(XXX)
  Returns NULL, if not in "${XXX}" format, or XXX is unknown
 */
static char *
parse_environment_variable(char *s, char **next)
{
  size_t len = strlen(s);
  if (s[0] == '$')
    {
      if (s[1] == '{')
	{
	  char   *env;
          for (int i = 2; i < len ; i++)
            {
              char c = s[i];
	      TAG(c);
	      /* c is never '\0' since we iterate over strlen */
              if (c == '}')
		{
		  s[i] = '\0';
		  env = getenv(&s[2]);
		  s[i] = '}';
		  *next = &s[i];
		  return env;
		}
            }
          /* Invalid environment variable - do not translate */          
        }
    }
  *next = s;
  return NULL;
}

char *expand_string(char *str)
{
  dyn_string_t  buf = dyn_string_new(MAX_STRING + 1);
  int len = strlen(str);
  char *p, *endp;
  char *result;

  if ((len == 0) || (buf == NULL))
    {
      result = malloc(1);	/* Normally we free the result afterwards */
      *result = '\0';		/* So we always have to provide dyn mem */
      return result;
    }

  char *s = strdup(str);
  p = s; endp = &s[len];
  for (p = s ; p < endp ; p++)
    {
      char c = *p;
      bool push=true;
      if ((c == '$') && (p[1] == '{'))
	{
	  char *env = parse_environment_variable(p, &p);
          if (env != NULL)
            {
              dyn_string_append_cstr(buf, env);
	      push=false;
            }
	}
      if (push)
	{
	  TAG(c);
	  dyn_string_append_char(buf, c);
        }
     }
   free(s);
   result = dyn_string_release(buf);
   return replace_slash(result);
}

#if defined(__MAIN__)
static
void validate (char *s, char *expected)
{
  char *translated = expand_string(s);
  char *quoted;
  int len = asprintf(&quoted, "\"%s\"", translated);
  if (!strcmp(translated, expected))
    {
      printf("FOUND:    %-40sOK\n", quoted);
      free(quoted);
    }
  else
    {
      printf("FAIL:     %-40sFAIL\n", quoted);
      free(quoted);
      len = asprintf(&quoted, "\"%s\"", expected);
      printf("EXPECTED: %-40s\n", quoted);
      free(quoted);
    }
  free(translated);
}

void main(void)
{
   setenv("RESULT", "Success", 1);

   validate("$RESULT",				"Should fail!");
   validate("The result is ${RESULT}",		"The result is Success");
   validate("The result is ${RESULT",		"The result is ${RESULT");
   validate("${RESULT}", 			"Success");
   validate("$RESULT",				"$RESULT");
   validate("${RESULT} starts this sentence",	"Success starts this sentence");
   validate("", 				"");
   validate("$RESULT",				"$RESULT");
   validate("$RESULT\\allan",			"$RESULT/allan");
}
#endif
