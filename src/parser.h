#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"

struct parser
{
  struct lexer *lexer;
  struct token current;
  struct location location;
  struct arena *arena;
};

struct parser parser_create (struct lexer *lexer, struct arena *);
struct ast *parser_parse (struct parser *);

#endif // PARSER_H

