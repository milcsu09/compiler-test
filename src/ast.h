#ifndef AST_H
#define AST_H

#include "arena.h"
#include "token.h"
#include "type.h"

union ast_entry
{
  struct error error;
  struct token token;
};

enum ast_type
{
  AST_ERROR,
  AST_CAST,
  AST_IDENTIFIER,
  AST_NUMBER,
  AST_BINARY,
  AST_CONDITIONAL,
  AST_COMPOUND,
  AST_DECLARATION,
  AST_CALL,
  AST_PROTOTYPE,
  AST_FUNCTION,
  AST_PROGRAM,
};

struct ast
{
  union ast_entry value;
  enum ast_type type;
  struct location location;
  struct type *expr_type;
  struct ast *child;
  struct ast *next;
  int state;
};

const char *ast_type_string (enum ast_type);

struct ast *ast_create (enum ast_type, struct location, struct arena *);
struct ast *ast_create_e (struct error, struct location, struct arena *);
struct ast *ast_copy (struct ast *, int, struct arena *);

void ast_destroy (struct ast *);

void ast_append (struct ast *, struct ast *);
void ast_attach (struct ast *, struct ast *);

int ast_match (struct ast *, enum ast_type);
int ast_match_error (struct ast *);

void ast_debug_print (struct ast *, size_t);

#endif // AST_H

