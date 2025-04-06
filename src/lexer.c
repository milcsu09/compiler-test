#include "arena.h"
#include "string.h"
#include "lexer.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct lexer
lexer_create (char *const source, const char *const context,
              struct arena *arena)
{
  struct lexer lexer;

  lexer.current = source;

  lexer.location.context = context;
  lexer.location.line = 1;
  lexer.location.column = 1;

  lexer.arena = arena;

  return lexer;
}

static void
lexer_advance (struct lexer *lexer)
{
  char c = *lexer->current;

  if (c == '\0')
    return;

  lexer->current++;
  location_advance (&lexer->location, c);
}

static void
lexer_advance_n (struct lexer *lexer, size_t n)
{
  while (n--)
    lexer_advance (lexer);
}

static struct token
lexer_advance_token (struct lexer *lexer, size_t type)
{
  struct token token;

  token = token_create (type, lexer->location);

  lexer_advance (lexer);

  return token;
}

/*
static struct token
lexer_advance_n_token (struct lexer *lexer, size_t n, size_t type)
{
  struct token token;

  token = token_create (type, lexer->location);

  lexer_advance_n (lexer, n);

  return token;
}
*/

static struct token
lexer_advance_identifier_n (struct lexer *lexer, size_t n)
{
  struct token token;

  char *s = string_copy_n (lexer->current, n, lexer->arena);

  token = token_create_s (s, TOKEN_IDENTIFIER, lexer->location);

  lexer_advance_n (lexer, n);

  return token;
}

/*
static int
lexer_match (struct lexer *lexer, const char *s)
{
  size_t length = strlen (s);
  return strncmp (lexer->current, s, length) == 0;
}
*/

static int
lexer_is_digit_base (char c, int base)
{
  switch (base)
    {
    case 2:
      return c == '0' || c == '1';
    case 8:
      return c >= '0' && c <= '7';
    case 16:
      return isxdigit (c);
    }

  return 0;
}

static struct token
lexer_lex_number_base (struct lexer *lexer, int base)
{
  struct location location = lexer->location;
  const char *start = lexer->current;

  while (lexer_is_digit_base (*lexer->current, base))
    lexer_advance (lexer);

  double f = strtod (start, NULL);

  return token_create_f (f, TOKEN_NUMBER, location);
}

static struct token
lexer_lex_number (struct lexer *lexer)
{
  if (*lexer->current == '0')
    switch (*(lexer->current + 1))
      {
      case 'b':
      case 'B':
        lexer_advance_n (lexer, 2);
        return lexer_lex_number_base (lexer, 2);
      case 'o':
      case 'O':
        lexer_advance_n (lexer, 2);
        return lexer_lex_number_base (lexer, 8);
      case 'x':
      case 'X':
        lexer_advance_n (lexer, 2);
        return lexer_lex_number_base (lexer, 16);
      }

  struct location location = lexer->location;
  const char *start = lexer->current;

  while (isdigit (*lexer->current))
    lexer_advance (lexer);

  if (*lexer->current == '.')
    {
      lexer_advance (lexer);

      while (isdigit (*lexer->current))
        lexer_advance (lexer);
    }

  if (*lexer->current == 'e' || *lexer->current == 'E')
    {
      lexer_advance (lexer);

      if (*lexer->current == '+' || *lexer->current == '-')
        lexer_advance (lexer);

      while (isdigit (*lexer->current))
        lexer_advance (lexer);
    }

  double f = strtod (start, NULL);

  return token_create_f (f, TOKEN_NUMBER, location);
}

static struct token
lexer_lex_identifier (struct lexer *lexer)
{
  struct location location = lexer->location;
  const char *start = lexer->current;

  while (isalnum (*lexer->current) || *lexer->current == '_')
    lexer_advance (lexer);

  if (strncmp (start, "define", 6) == 0)
    return token_create (TOKEN_DEFINE, location);
  if (strncmp (start, "extern", 6) == 0)
    return token_create (TOKEN_EXTERN, location);

  char *s = string_copy_until (start, lexer->current, lexer->arena);

  return token_create_s (s, TOKEN_IDENTIFIER, location);
}

struct token
lexer_next (struct lexer *lexer)
{
  while (isspace (*lexer->current))
    lexer_advance (lexer);

  while (*lexer->current == '#')
    {
      while (*lexer->current && *lexer->current != '\n')
        lexer_advance (lexer);

      while (isspace (*lexer->current))
        lexer_advance (lexer);
    }

  char c = *lexer->current;

  switch (c)
    {
    case ';':
      return lexer_advance_token (lexer, TOKEN_SEMICOLON);
    case '(':
      return lexer_advance_token (lexer, TOKEN_LPAREN);
    case ')':
      return lexer_advance_token (lexer, TOKEN_RPAREN);
    // case '[':
    //   return lexer_advance_token (lexer, TOKEN_LBRACKET);
    // case ']':
    //   return lexer_advance_token (lexer, TOKEN_RBRACKET);
    // case '{':
    //   return lexer_advance_token (lexer, TOKEN_LBRACE);
    // case '}':
    //   return lexer_advance_token (lexer, TOKEN_RBRACE);
    case '=':
      return lexer_advance_token (lexer, TOKEN_EQUAL);
    case ',':
      return lexer_advance_token (lexer, TOKEN_COMMA);
    case '+':
      return lexer_advance_identifier_n (lexer, 1);
    case '-':
      // if (lexer_match (lexer, "->"))
      //   return lexer_advance_n_token (lexer, 2, TOKEN_ARROW);
      return lexer_advance_identifier_n (lexer, 1);
    case '/':
      return lexer_advance_identifier_n (lexer, 1);
    case '*':
      return lexer_advance_identifier_n (lexer, 1);
    case '%':
      return lexer_advance_identifier_n (lexer, 1);
    case '\0':
      return token_create (TOKEN_NOTHING, lexer->location);
    }

  if (isdigit (c))
    return lexer_lex_number (lexer);

  if (isalpha (c) || c == '_')
    return lexer_lex_identifier (lexer);

  return token_create_e (error_create ("unexpected character %c", c),
                         lexer->location);
}

struct token
lexer_peek (struct lexer *lexer)
{
  struct lexer copy = *lexer;
  return lexer_next (&copy);
}

