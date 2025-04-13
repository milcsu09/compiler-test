#include "ast.h"
#include "parser.h"
#include "string.h"
#include <stdlib.h>
#include <string.h>

#include <stdio.h>

struct parser
parser_create (struct lexer *lexer, struct arena *arena)
{
  struct parser parser;

  parser.lexer = lexer;
  parser.arena = arena;

  return parser;
}

struct precedence
{
  const char *key;
  double precedence;
};

static struct precedence PRECEDENCE_TABLE[128];
size_t precedence_n = 0;

void
precedence_table_add (const char *key, double precedence)
{
  struct precedence p;

  p.key = string_copy (key, NULL);
  p.precedence = precedence;

  PRECEDENCE_TABLE[precedence_n++] = p;
}

void
precedence_table_destroy ()
{
  for (size_t i = 0; i < precedence_n; ++i)
    free ((void *)PRECEDENCE_TABLE[i].key);
}

static struct precedence
parser_query_precedence_table (struct token token)
{
  if (!token_match (token, TOKEN_IDENTIFIER))
    goto invalid;

  for (size_t i = 0; i < precedence_n; ++i)
    {
      struct precedence current = PRECEDENCE_TABLE[i];

      if (strcmp (current.key, token.value.s) == 0)
        return current;
    }

invalid:
  return (struct precedence) { 0 };
}

static int
parser_match (struct parser *parser, enum token_type type)
{
  return token_match (parser->current, type);
}

static int
parser_match_error (struct parser *parser)
{
  return parser_match (parser, TOKEN_ERROR);
}

static struct ast *
parser_error_from_token (struct parser *parser, struct token token,
                         struct location location)
{
  return ast_create_e (token.value.error, location, parser->arena);
}

static struct ast *
parser_error_from_current (struct parser *parser)
{
  return parser_error_from_token (parser, parser->current, parser->location);
}

static struct ast *
parser_error_expect_base (struct parser *parser, const char *a, const char *b)
{
  return ast_create_e (error_create ("expected `%s`, got `%s`", a, b),
                       parser->location, parser->arena);
}

static struct ast *
parser_error_expect_token (struct parser *parser, enum token_type type)
{
  const char *a = token_type_string (type);
  const char *b = token_type_string (parser->current.type);
  return parser_error_expect_base (parser, a, b);
}

static int
parser_advance (struct parser *parser)
{
  /* 'lexer_next' might return token of type 'TOKEN_ERROR'. */
  parser->current = lexer_next (parser->lexer);
  parser->location = parser->current.location;
  // if (parser->current.type == TOKEN_IDENTIFIER)
  //   printf ("'%s'\n", parser->current.value.s);
  return parser_match_error (parser);
}

struct ast *parser_parse_program(struct parser *);
struct ast *parser_parse_statement(struct parser *);
struct ast *parser_parse_top_level_statement(struct parser *);
struct ast *parser_parse_function_prototype(struct parser *);
struct ast *parser_parse_expression(struct parser *);
struct ast *parser_parse_primary(struct parser *);
struct ast *parser_parse_identifier_expression(struct parser *);
struct ast *parser_parse_number_expression(struct parser *);
struct ast *parser_parse_group_expression(struct parser *);
struct ast *parser_parse_compound_expression (struct parser *);
struct ast *parser_parse_conditional_expression (struct parser *);

struct ast *parser_parse_declaration_expression (struct parser *);

// TODO: HANDLE ERRORS!
struct type *
parser_parse_type_primary (struct parser *parser)
{
  switch (parser->current.type)
    {
    case TOKEN_VOID:
      parser_advance (parser);
      return type_create (TYPE_VOID, parser->arena);
    case TOKEN_F64:
      parser_advance (parser);
      return type_create (TYPE_F64, parser->arena);
    case TOKEN_BOOL:
      parser_advance (parser);
      return type_create (TYPE_BOOL, parser->arena);
    default:
      printf ("Expected F64 or Bool\n");
      abort();
    }
}

// TODO: HANDLE ERRORS!
struct type *
parser_parse_type (struct parser *parser)
{
  switch (parser->current.type)
    {
    case TOKEN_LPAREN:
      {
        // (
        parser_advance (parser);

        size_t capacity = 4;

        struct type_function function;
        function.argument_t = calloc (capacity, sizeof (struct type *));
        function.argument_n = 0;

        if (!parser_match (parser, TOKEN_RPAREN))
          while (1)
            {
              struct type *argument;

              argument = parser_parse_type_primary (parser);

              if (function.argument_n >= capacity)
                {
                  capacity *= 2;
                  function.argument_t = realloc (function.argument_t, capacity * sizeof (struct type *));
                }

              function.argument_t[function.argument_n++] = argument;

              if (parser_match (parser, TOKEN_RPAREN))
                break;

              // ,
              parser_advance (parser);
            }

        // )
        parser_advance (parser);

        // ->
        parser_advance (parser);

        function.return_t = parser_parse_type_primary (parser);

        function.variadic = 0;
        return type_create_f (function, parser->arena);
      }
    default:
      return parser_parse_type_primary (parser);
    }
}

struct ast *
parser_parse_identifier (struct parser *parser)
{
  if (!parser_match (parser, TOKEN_IDENTIFIER))
    return parser_error_expect_token (parser, TOKEN_IDENTIFIER);

  struct ast *result;

  result = ast_create (AST_IDENTIFIER, parser->location, parser->arena);
  result->value.token = parser->current;

  if (parser_advance (parser))
    return parser_error_from_current (parser);

  return result;
}

struct ast *
parser_parse_primary (struct parser *parser)
{
  switch (parser->current.type)
    {
    case TOKEN_IDENTIFIER:
      return parser_parse_identifier_expression (parser);
    case TOKEN_NUMBER:
      return parser_parse_number_expression (parser);
    case TOKEN_LPAREN:
      return parser_parse_group_expression (parser);
    case TOKEN_LBRACE:
      return parser_parse_compound_expression (parser);
    case TOKEN_IF:
      return parser_parse_conditional_expression (parser);
    default:
      {
        const char *b = token_type_string (parser->current.type);
        return parser_error_expect_base (parser, "expression", b);
      }
    }
}

struct ast *
parser_parse_conditional_expression (struct parser *parser)
{
  struct location l_result = parser->location;

  if (!parser_match (parser, TOKEN_IF))
    return parser_error_expect_token (parser, TOKEN_IF);

  if (parser_advance (parser))
    return parser_error_from_current (parser);

  struct ast *cond_node;
  struct ast *then_node;
  struct ast *else_node;
  struct ast *result;

  struct location l_cond = parser->location;

  cond_node = parser_parse_expression (parser);
  if (ast_match_error (cond_node))
    return cond_node;

  if (!parser_match (parser, TOKEN_THEN))
    return parser_error_expect_token (parser, TOKEN_THEN);

  if (parser_advance (parser))
    return parser_error_from_current (parser);

  struct location l_then = parser->location;

  then_node = parser_parse_expression (parser);
  if (ast_match_error (then_node))
    return then_node;

  if (!parser_match (parser, TOKEN_ELSE))
    return parser_error_expect_token (parser, TOKEN_ELSE);

  if (parser_advance (parser))
    return parser_error_from_current (parser);

  struct location l_else = parser->location;

  else_node = parser_parse_expression (parser);
  if (ast_match_error (else_node))
    return else_node;

  result = ast_create (AST_CONDITIONAL, parser->location, parser->arena);

  ast_append (result, cond_node);
  ast_append (result, then_node);
  ast_append (result, else_node);

  result->location = l_result;
  cond_node->location = l_cond;
  then_node->location = l_then;
  else_node->location = l_else;

  return result;
}

struct ast *
parser_parse_compound_expression (struct parser *parser)
{
  if (!parser_match (parser, TOKEN_LBRACE))
    return parser_error_expect_token (parser, TOKEN_LBRACE);

  if (parser_advance (parser))
    return parser_error_from_current (parser);

  struct ast *result;

  result = ast_create (AST_COMPOUND, parser->location, parser->arena);

  // Minimum of 1 expression must be inside compound.
  while (1)
    {
      struct ast *expression;

      expression = parser_parse_declaration_expression (parser);
      if (ast_match_error (expression))
        return expression;

      ast_append (result, expression);

      if (parser_match (parser, TOKEN_RBRACE))
        break;

      if (!parser_match (parser, TOKEN_SEMICOLON))
        return parser_error_expect_token (parser, TOKEN_RBRACE);

      if (parser_advance (parser))
        return parser_error_from_current (parser);
    }

  if (!parser_match (parser, TOKEN_RBRACE))
    return parser_error_expect_token (parser, TOKEN_RBRACE);

  if (parser_advance (parser))
    return parser_error_from_current (parser);

  return result;
}


struct ast *
parser_parse_identifier_expression (struct parser *parser)
{
  struct ast *identifier;

  identifier = parser_parse_identifier (parser);
  if (ast_match_error (identifier))
    return identifier;

  if (!parser_match (parser, TOKEN_LPAREN))
    return identifier;

  if (parser_advance (parser))
    return parser_error_from_current (parser);

  struct ast *result;

  result = ast_create (AST_CALL, parser->location, parser->arena);

  ast_append (result, identifier);

  if (!parser_match (parser, TOKEN_RPAREN))
    while (1)
      {
        struct ast *argument;

        argument = parser_parse_expression (parser);
        if (ast_match_error (argument))
          return argument;

        ast_append (result, argument);

        if (parser_match (parser, TOKEN_RPAREN))
          break;

        if (!parser_match (parser, TOKEN_COMMA))
          return parser_error_expect_token (parser, TOKEN_RPAREN);

        if (parser_advance (parser))
          return parser_error_from_current (parser);
      }

  if (!parser_match (parser, TOKEN_RPAREN))
    return parser_error_expect_token (parser, TOKEN_RPAREN);

  if (parser_advance (parser))
    return parser_error_from_current (parser);

  return result;
}

struct ast *
parser_parse_number_expression (struct parser *parser)
{
  if (!parser_match (parser, TOKEN_NUMBER))
    return parser_error_expect_token (parser, TOKEN_NUMBER);

  struct ast *result;

  result = ast_create (AST_NUMBER, parser->location, parser->arena);
  result->value.token = parser->current;

  if (parser_advance (parser))
    return parser_error_from_current (parser);

  result->expr_type = type_create (TYPE_F64, parser->arena);

  return result;
}

struct ast *
parser_parse_group_expression (struct parser *parser)
{
  struct location location = parser->location;

  if (!parser_match (parser, TOKEN_LPAREN))
    return parser_error_expect_token (parser, TOKEN_LPAREN);

  if (parser_advance (parser))
    return parser_error_from_current (parser);

  struct ast *result;

  result = parser_parse_expression (parser);
  if (ast_match_error (result))
    return result;

  if (!parser_match (parser, TOKEN_RPAREN))
    return parser_error_expect_token (parser, TOKEN_RPAREN);

  if (parser_advance (parser))
    return parser_error_from_current (parser);

  result->location = location;
  return result;
}

struct ast *
parser_parse_expression_base (struct parser *parser, double previous)
{
  struct ast *left;

  left = parser_parse_primary (parser);
  if (ast_match_error (left))
    return left;

  struct precedence p;
  p = parser_query_precedence_table (parser->current);

  while (p.precedence > previous)
    {
      struct ast *middle;

      middle = parser_parse_identifier (parser);
      if (ast_match_error (middle))
        return middle;

      struct ast *right;

      right = parser_parse_expression_base (parser, p.precedence);
      if (ast_match_error (right))
        return right;

      struct ast *t;
      t = ast_create (AST_BINARY, left->location, parser->arena);

      ast_append (t, middle);
      ast_append (t, left);
      ast_append (t, right);

      left = t;

      p = parser_query_precedence_table (parser->current);
    }

  return left;
}

struct ast *
parser_parse_expression (struct parser *parser)
{
  struct ast *expression = parser_parse_expression_base (parser, 0.0);
  if (ast_match_error (expression))
    return expression;

  if (!parser_match (parser, TOKEN_AS))
    return expression;

  struct ast *cast = ast_create (AST_CAST, parser->location, parser->arena);

  if (parser_advance (parser))
    return parser_error_from_current (parser);

  struct type *type = parser_parse_type_primary (parser);

  ast_append (cast, expression);

  cast->expr_type = type;

  return cast;
}

struct ast *
parser_parse_declaration_expression (struct parser *parser)
{
  if (parser_match (parser, TOKEN_IDENTIFIER))
    {
      struct token peek = lexer_peek (parser->lexer);

      if (peek.type != TOKEN_COLON)
        return parser_parse_expression (parser);

      struct ast *name = parser_parse_identifier (parser);
      if (ast_match_error (name))
        return name;

      if (parser_advance (parser))
        return parser_error_from_current (parser);

      struct type *type = parser_parse_type_primary (parser);

      struct ast *result = ast_create (AST_DECLARATION, parser->location,
                                       parser->arena);

      result->expr_type = type;
      ast_append (result, name);

      if (!parser_match (parser, TOKEN_IDENTIFIER) ||
          (parser_match (parser, TOKEN_IDENTIFIER) &&
           strcmp (parser->current.value.s, "=") != 0))
        return result;

      if (parser_advance (parser))
        return parser_error_from_current (parser);

      struct ast *value = parser_parse_expression (parser);
      if (ast_match_error (value))
        return value;

      ast_append (result, value);

      return result;
    }

  return parser_parse_expression (parser);
}

struct ast *
parser_parse_function_prototype(struct parser *parser)
{
  struct ast *name;
  struct ast *result;

  int is_operator = 0;
  double precedence = 1.0;

  name = parser_parse_identifier (parser);
  if (ast_match_error (name))
    return name;

  if (parser_match (parser, TOKEN_NUMBER))
    {
      is_operator = 1;
      precedence = parser->current.value.f;

      if (parser_advance (parser))
        return parser_error_from_current (parser);
    }

  if (!parser_match (parser, TOKEN_LPAREN))
    return parser_error_expect_token (parser, TOKEN_LPAREN);

  if (parser_advance (parser))
    return parser_error_from_current (parser);

  result = ast_create (AST_PROTOTYPE, parser->location, parser->arena);

  ast_append (result, name);

  size_t n_arg = 0;
  int variadic = 0;

  if (!parser_match (parser, TOKEN_RPAREN))
    while (1)
      {
        if (parser_match (parser, TOKEN_3DOT))
          {
            variadic = 1;

            if (parser_advance (parser))
              return parser_error_from_current (parser);

            break;
          }

        struct ast *argument;

        argument = parser_parse_identifier (parser);
        if (ast_match_error (argument))
          return argument;

        ast_append (result, argument);
        n_arg++;

        if (parser_match (parser, TOKEN_RPAREN))
          break;

        if (!parser_match (parser, TOKEN_COMMA))
          // Expect TOKEN_RPAREN for better message.
          return parser_error_expect_token (parser, TOKEN_RPAREN);

        if (parser_advance (parser))
          return parser_error_from_current (parser);
      }

  if (!parser_match (parser, TOKEN_RPAREN))
    return parser_error_expect_token (parser, TOKEN_RPAREN);

  if (parser_advance (parser))
    return parser_error_from_current (parser);

  if (is_operator)
    {
      if (n_arg != 2 || variadic)
        {
          struct error e
              = error_create ("binary-operator expects two parameters");
          return ast_create_e (e, name->location, parser->arena);
        }

      precedence_table_add (name->value.token.value.s, precedence);
    }

  if (!parser_match (parser, TOKEN_COLON))
    return parser_error_expect_token (parser, TOKEN_COLON);

  if (parser_advance (parser))
    return parser_error_from_current (parser);

  struct type *type = parser_parse_type (parser);
  type->value.function.variadic = variadic;
  result->expr_type = type;

  result->state = variadic;
  return result;
}

struct ast *
parser_parse_top_level_statement(struct parser *parser)
{
  struct ast *proto;

  proto = parser_parse_function_prototype (parser);
  if (ast_match_error (proto))
    return proto;

  // if (!parser_match (parser, TOKEN_EQUAL))
  //   return proto;

  if (!parser_match (parser, TOKEN_IDENTIFIER) ||
      (parser_match (parser, TOKEN_IDENTIFIER) &&
       strcmp (parser->current.value.s, "=") != 0))
    return proto;

  if (parser_advance (parser))
    return parser_error_from_current (parser);

  struct ast *expression;

  expression = parser_parse_expression (parser);
  if (ast_match_error (expression))
    return expression;

  struct ast *result;

  result = ast_create (AST_FUNCTION, parser->location, parser->arena);

  ast_append (result, proto);
  ast_append (result, expression);

  return result;
}

struct ast *
parser_parse_statement(struct parser *parser)
{
  struct ast *result;

  result = parser_parse_top_level_statement (parser);
  if (ast_match_error (result))
    return result;

  if (!parser_match (parser, TOKEN_SEMICOLON))
    return parser_error_expect_token (parser, TOKEN_SEMICOLON);

  if (parser_advance (parser))
    return parser_error_from_current (parser);

  return result;
}

struct ast *
parser_parse_program(struct parser *parser)
{
  struct ast *result;

  result = ast_create (AST_PROGRAM, parser->location, parser->arena);

  while (1)
    switch (parser->current.type)
      {
      case TOKEN_NOTHING:
        return result;
      default:
        {
          struct ast *statement;

          statement = parser_parse_statement (parser);
          if (ast_match_error (statement))
            return statement;

          ast_append (result, statement);
        }
        break;
      }
}

struct ast *
parser_parse (struct parser *parser)
{
  if (parser_advance (parser))
    return parser_error_from_current (parser);

  struct ast *result;

  result = parser_parse_program (parser);
  if (ast_match_error (result))
    return result;

  if (!parser_match (parser, TOKEN_NOTHING))
    return parser_error_expect_token (parser, TOKEN_NOTHING);

  if (parser_advance (parser))
    return parser_error_from_current (parser);

  return result;
}

