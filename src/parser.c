#include "ast.h"
#include "parser.h"
#include "string.h"
#include <string.h>

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
  int precedence;
};

static struct precedence PRECEDENCE_TABLE[] = {
  { "+", 35 },
  { "-", 35 },

  { "*", 40 },
  { "/", 40 },

};

static struct precedence
parser_query_precedence_table (struct token token)
{
  if (!token_match (token, TOKEN_IDENTIFIER))
    goto invalid;

  size_t size = sizeof (PRECEDENCE_TABLE) / sizeof (PRECEDENCE_TABLE[0]);

  for (size_t i = 0; i < size; ++i)
    {
      struct precedence current = PRECEDENCE_TABLE[i];

      if (strcmp (current.key, token.value.s) == 0)
        return current;
    }

invalid:
  return (struct precedence) { 0 };
}

/*
static int
parser_is_delimiter (struct token token)
{
  switch (token.type)
    {
    case TOKEN_IDENTIFIER:
      return parser_query_precedence_table (token).precedence > 0;
    case TOKEN_NOTHING:
    case TOKEN_SEMICOLON:
    case TOKEN_RPAREN:
    case TOKEN_COMMA:
      return 1;
    default:
      return 0;
    }
}
*/

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
  return parser_match_error (parser);
}

struct ast *parser_parse_statement (struct parser *);
struct ast *parser_parse_top_level_expression (struct parser *);
struct ast *parser_parse_define (struct parser *);
struct ast *parser_parse_extern (struct parser *);
struct ast *parser_parse_function_prototype (struct parser *);
struct ast *parser_parse_expression (struct parser *);
struct ast *parser_parse_primary (struct parser *);
struct ast *parser_parse_identifier_expression (struct parser *);
struct ast *parser_parse_number_expression (struct parser *);
struct ast *parser_parse_group_expression (struct parser *);

struct ast *parser_parse_identifier (struct parser *);

struct ast *
parser_parse_statement (struct parser *parser)
{
  struct ast *result;

  switch (parser->current.type)
    {
    case TOKEN_DEFINE:
      result = parser_parse_define (parser);
      break;
    case TOKEN_EXTERN:
      result = parser_parse_extern (parser);
      break;
    default:
      result = parser_parse_top_level_expression (parser);
      break;
    }

  if (ast_match_error (result))
    return result;

  if (!parser_match (parser, TOKEN_SEMICOLON))
    return parser_error_expect_token (parser, TOKEN_SEMICOLON);

  if (parser_advance (parser))
    return parser_error_from_current (parser);

  return result;
}

struct ast *
parser_parse_top_level_expression (struct parser *parser)
{
  struct ast *expression = parser_parse_expression (parser);

  if (ast_match_error (expression))
    return expression;

  struct ast *prototype;
  prototype = ast_create (AST_PROTOTYPE, parser->location, parser->arena);

  struct token token = token_create (TOKEN_IDENTIFIER, parser->location);
  token.value.s = string_copy ("__top_level_expression", parser->arena);

  struct ast *identifier;
  identifier = ast_create (AST_IDENTIFIER, parser->location, parser->arena);
  identifier->value.token = token;

  ast_append (prototype, identifier);

  struct ast *function;
  function = ast_create (AST_FUNCTION, parser->location, parser->arena);

  ast_append (function, prototype);
  ast_append (function, expression);

  return function;
}

struct ast *
parser_parse_define (struct parser *parser)
{
  if (!parser_match (parser, TOKEN_DEFINE))
    return parser_error_expect_token (parser, TOKEN_DEFINE);

  if (parser_advance (parser))
    return parser_error_from_current (parser);

  struct ast *prototype;
  struct ast *expression;
  struct ast *result;

  prototype = parser_parse_function_prototype (parser);
  if (ast_match_error (prototype))
    return prototype;

  expression = parser_parse_expression (parser);
  if (ast_match_error (expression))
    return expression;

  result = ast_create (AST_FUNCTION, parser->location, parser->arena);

  ast_append (result, prototype);
  ast_append (result, expression);

  return result;
}

struct ast *
parser_parse_extern (struct parser *parser)
{
  if (!parser_match (parser, TOKEN_EXTERN))
    return parser_error_expect_token (parser, TOKEN_EXTERN);

  if (parser_advance (parser))
    return parser_error_from_current (parser);

  return parser_parse_function_prototype (parser);
}

struct ast *
parser_parse_function_prototype (struct parser *parser)
{
  struct ast *name;
  struct ast *result;

  name = parser_parse_identifier(parser);
  if (ast_match_error (name))
    return name;

  // Match '('
  if (!parser_match (parser, TOKEN_LPAREN))
    return parser_error_expect_token (parser, TOKEN_LPAREN);

  if (parser_advance (parser))
    return parser_error_from_current (parser);

  result = ast_create (AST_PROTOTYPE, parser->location, parser->arena);

  ast_append (result, name);

  if (!parser_match (parser, TOKEN_RPAREN))
    while (1)
      {
        struct ast *argument;

        argument = parser_parse_identifier (parser);
        if (ast_match_error (argument))
          return argument;

        ast_append (result, argument);

        if (parser_match (parser, TOKEN_RPAREN))
          break;

        if (!parser_match (parser, TOKEN_COMMA))
          return parser_error_expect_token (parser, TOKEN_COMMA);

        if (parser_advance (parser))
          return parser_error_from_current (parser);
      }

  // Match ')'
  if (!parser_match (parser, TOKEN_RPAREN))
    return parser_error_expect_token (parser, TOKEN_RPAREN);

  if (parser_advance (parser))
    return parser_error_from_current (parser);

  return result;
}

struct ast *
parser_parse_expression_base (struct parser *parser, int previous)
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
      t = ast_create (AST_BINARY, parser->location, parser->arena);

      ast_append (t, middle);
      ast_append (t, left);
      ast_append (t, right);

      left = t;

      p = parser_query_precedence_table (parser->current);
    }

  return left;

  /*
  struct ast *left;

  left = parser_parse_primary (parser);
  if (ast_match_error (left))
    return left;

  while (1)
    {
      struct precedence p;
      p = parser_query_precedence_table (parser->current);

      if (p.precedence < previous)
        return left;

      struct ast *middle;

      middle = parser_parse_identifier (parser);
      if (ast_match_error (middle))
        return middle;


    }
    */

  // return parser_parse_primary (parser);
}

struct ast *
parser_parse_expression (struct parser *parser)
{
  return parser_parse_expression_base (parser, 0);
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
    default:
      {
        const char *b = token_type_string (parser->current.type);
        return parser_error_expect_base (parser, "expression", b);
      }
    }
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
          return parser_error_expect_token (parser, TOKEN_COMMA);

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

  return result;
}

struct ast *
parser_parse_group_expression (struct parser *parser)
{
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

  return result;
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
parser_parse_program (struct parser *parser)
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

  struct ast *statement;

  statement = parser_parse_program (parser);
  if (ast_match_error (statement))
    return statement;

  if (!parser_match (parser, TOKEN_NOTHING))
    return parser_error_expect_token (parser, TOKEN_NOTHING);

  if (parser_advance (parser))
    return parser_error_from_current (parser);

  return statement;
}

