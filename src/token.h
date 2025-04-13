#ifndef TOKEN_H
#define TOKEN_H

#include "error.h"

struct arena;

union token_entry
{
  struct error error;
  // long i;
  double f;
  char *s;
};

enum token_type
{
  TOKEN_NOTHING,
  TOKEN_ERROR,
  TOKEN_SEMICOLON,
  TOKEN_IDENTIFIER,
  TOKEN_NUMBER,
  TOKEN_LPAREN,
  TOKEN_RPAREN,
  TOKEN_LBRACE,
  TOKEN_RBRACE,
  // TOKEN_EQUAL,
  TOKEN_COMMA,
  TOKEN_COLON,
  TOKEN_ARROW,
  TOKEN_3DOT,
  TOKEN_IF,
  TOKEN_THEN,
  TOKEN_ELSE,
  TOKEN_VOID,
  TOKEN_F64,
  TOKEN_BOOL,
  TOKEN_AS,
  TOKEN_WHILE,
  TOKEN_DO,
  // TOKEN_DEFINE,
  // TOKEN_EXTERN
};

struct token
{
  union token_entry value;
  enum token_type type;
  struct location location;
};

const char *token_type_string (enum token_type);

struct token token_create (enum token_type, struct location);
struct token token_create_e (struct error, struct location);
struct token token_create_f (double, enum token_type, struct location);
struct token token_create_s (char *, enum token_type, struct location);

struct token token_copy (struct token, struct arena *);

void token_destroy (struct token);

int token_match (struct token, enum token_type);
int token_match_error (struct token);

void token_debug_print (struct token);

#endif // TOKEN_H

