/*
 * File:    string-expand.c
 * Author:  Ulf Samuelsson <ulf@emagii.com>
 *
 * This file is licensed under the MIT License as stated below
 *
 * Copyright (c) 2023 Ulf Samuelsson (ulf@emagii.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#define _GNU_SOURCE
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>

#include "string-expand.h"
#define	DEBUG 0

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

/* String functions */
#define MAX_STRING 100
typedef struct {
  uint32_t	ix;
  char		data[MAX_STRING+1];
} string_t;

static void
append(string_t *s, char c)
{
  uint32_t ix = s->ix;
  if (ix < MAX_STRING)
    {
      char *p = &s->data[ix];
      *p++ = c;
      *p   = '\0';
      s->ix++;
    }
}

static void
clear(string_t *s)
{
  s->ix = 0;
  s->data[0] = '\0';
}

static bool
max_string_size(string_t *s)
{
  return (s->ix >= MAX_STRING);
}

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

static
bool expand(char **result, string_t *buf, char *env)
{
  char *tmp;
  int32_t status;
  char *_result = *result;
  if (_result == NULL)
    _result = "";
  char *_env    = env;
  if (_env == NULL)
    _env = "";
  status = asprintf(&tmp, "%s%s%s", _result, buf->data, _env);

  if (status != -1)
    {
      clear(buf);
      if (*result != NULL)
        free(*result);
      *result = tmp;
      return true;
    }
  return false;
}

char *expand_string(char *str)
{
  string_t  buf;
  int len = strlen(str);
  char *p, *endp;
  char *result;

  if (len == 0)
    {
      result = malloc(1);	/* Normally we free the result afterwards */
      *result = '\0';		/* So we always have to provide dyn mem */
      return result;
    }

  char *s = strdup(str);
  result  = NULL;	/* Result string */
  clear(&buf);
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
	      if (expand(&result, &buf, env))
		{
		  clear(&buf);
		  push=false;
		}
            }
	}
      if (push)
	{
	  TAG(c);
	  append(&buf, c);
          if (max_string_size(&buf))
            {
	      if (expand(&result, &buf, NULL))
	        {
	          clear(&buf);
		}
            }
        }
     }
   free(s);
   if (expand(&result, &buf, NULL))
     {
     	clear(&buf);
     }
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
