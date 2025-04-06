#ifndef LEXER_H
#define LEXER_H

#include "token.h"

struct lexer
{
  char *current;
  struct location location;
  struct arena *arena;
};

struct lexer lexer_create (char *const, const char *const, struct arena *);
struct token lexer_next (struct lexer *);
struct token lexer_peek (struct lexer *);

#endif // LEXER_H

