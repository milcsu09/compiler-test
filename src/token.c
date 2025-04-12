#include "string.h"
#include "token.h"
#include <stdio.h>
#include <stdlib.h>

static const char *const TOKEN_TYPE_STRING[] = {
  "nothing",
  "error",
  ";",
  "identifier",
  "number",
  "(",
  ")",
  "{",
  "}",
  "=",
  ",",
  "...",
  "if-keyword",
  "then-keyword",
  "else-keyword",
  // "define",
  // "extern"
};

const char *
token_type_string (enum token_type type)
{
  return TOKEN_TYPE_STRING[type];
}

struct token
token_create (enum token_type type, struct location location)
{
  struct token token;

  token.type = type;
  token.location = location;

  return token;
}

struct token
token_create_e (struct error error, struct location location)
{
  struct token token;

  token = token_create (TOKEN_ERROR, location);

  token.value.error = error;

  return token;
}

struct token
token_create_f (double f, enum token_type type, struct location location)
{
  struct token token;

  token = token_create (type, location);

  token.value.f = f;

  return token;
}

struct token
token_create_s (char *s, enum token_type type, struct location location)
{
  struct token token;

  token = token_create (type, location);

  token.value.s = s;

  return token;
}

struct token
token_copy (struct token token, struct arena *arena)
{
  struct token copy;

  switch (token.type)
    {
    case TOKEN_IDENTIFIER:
      copy.value.s = string_copy (token.value.s, arena);
      break;
    case TOKEN_NUMBER:
      copy.value.f = token.value.f;
      break;
    default:
      break;
    }

  copy.type = token.type;
  copy.location = token.location;

  return copy;
}

void
token_destroy (struct token token)
{
  switch (token.type)
    {
    case TOKEN_IDENTIFIER:
      free (token.value.s);
      break;
    default:
      break;
    }
}

int
token_match (struct token token, enum token_type type)
{
  return token.type == type;
}

int
token_match_error (struct token token)
{
  return token_match (token, TOKEN_ERROR);
}

void
token_debug_print (struct token token)
{
  switch (token.type)
    {
    case TOKEN_IDENTIFIER:
      printf ("%s", token.value.s);
      break;
    case TOKEN_NUMBER:
      printf ("%g", token.value.f);
      break;
    default:
      printf ("%s", token_type_string (token.type));
      break;
    }
}

