#include <stdio.h>

#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include <limits.h>

#include "arena.h"
#include "ast.h"
#include "error.h"
#include "lexer.h"
#include "parser.h"
#include "string.h"
#include "token.h"
#include "type.h"

static char *
read_file (const char *filename)
{
  FILE *file = fopen (filename, "r");

  fseek (file, 0, SEEK_END);
  size_t file_size = ftell (file);
  fseek (file, 0, SEEK_SET);

  char *buffer = (char *)malloc (file_size + 1);
  (void)fread (buffer, 1, file_size, file);

  buffer[file_size] = '\0';

  fclose (file);
  return buffer;
}

#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Support.h>

#include <llvm-c/Transforms/Scalar.h>
#include <llvm-c/Transforms/PassManagerBuilder.h>
#include <llvm-c//Transforms/Utils.h>

int has_error = 0;

LLVMContextRef context;
LLVMModuleRef module;
LLVMBuilderRef builder;
LLVMPassManagerRef pass_manager;

LLVMValueRef
generate_error (struct location location, const char *fmt)
{
  location_debug_print (location);
  printf (": fatal-error: %s [SYNTAX]\n", fmt);

  has_error = 1;
  return NULL;
}

LLVMTypeRef
type_kind_to_llvm (enum type_kind kind)
{
  switch (kind)
    {
    case TYPE_VOID:
      return LLVMVoidTypeInContext (context);
    case TYPE_F64:
      return LLVMDoubleTypeInContext (context);
    case TYPE_BOOL:
      return LLVMInt1TypeInContext (context);
    default:
      printf ("Bad type to LLVM\n");
      abort ();
    }
}

enum symbol_type
{
  SYMBOL_TYPE,
  SYMBOL_VALUE,
};

union symbol_value
{
  struct type *type;
  LLVMValueRef value;
};

struct symbol
{
  union symbol_value value;
  char *name;
  enum symbol_type type;
};

struct symbol *
symbol_create (enum symbol_type type, const char *name)
{
  struct symbol *symbol;

  symbol = calloc (1, sizeof (struct symbol));

  symbol->type = type;
  symbol->name = string_copy (name, NULL);

  return symbol;
}

void
symbol_destroy (struct symbol *symbol)
{
  free (symbol->name);
  free (symbol);
}

struct scope
{
  struct scope *parent;
  struct symbol **table;
  size_t table_capacity;
  size_t table_n;
};

struct scope *
scope_create (struct scope *parent)
{
  struct scope *scope;

  scope = calloc (1, sizeof (struct scope));

  scope->parent = parent;
  scope->table_capacity = 4;
  scope->table_n = 0;

  scope->table = calloc (scope->table_capacity, sizeof (struct symbol *));

  return scope;
}

void
scope_clear (struct scope *scope)
{
  for (size_t i = 0; i < scope->table_n; ++i)
    symbol_destroy (scope->table[i]);
  scope->table_n = 0;
}

void
scope_destroy (struct scope *scope)
{
  scope_clear (scope);
  free (scope->table);
  free (scope);
}

void
scope_add (struct scope *scope, struct symbol *symbol)
{
  if (scope->table_n >= scope->table_capacity)
    {
      scope->table_capacity *= 2.0;
      scope->table = realloc (scope->table,
                              scope->table_capacity * sizeof (struct symbol *));
    }

  scope->table[scope->table_n++] = symbol;
}

struct symbol *
scope_find (struct scope *scope, const char *name)
{
  for (size_t i = 0; i < scope->table_n; ++i)
    if (strcmp (scope->table[i]->name, name) == 0)
      return scope->table[i];

  if (scope->parent)
    return scope_find (scope->parent, name);

  return NULL;
}

LLVMValueRef
create_entry_alloca(LLVMValueRef function, const char *name, LLVMTypeRef type)
{
  LLVMBasicBlockRef entry = LLVMGetEntryBasicBlock(function);
  LLVMValueRef first_instr = LLVMGetFirstInstruction(entry);

  LLVMBasicBlockRef curr_bb = LLVMGetInsertBlock(builder);

  if (first_instr)
    LLVMPositionBuilderBefore(builder, first_instr);
  else
    LLVMPositionBuilderAtEnd(builder, entry); // fallback if entry is empty

  LLVMValueRef alloca = LLVMBuildAlloca(builder, type, name);

  LLVMPositionBuilderAtEnd(builder, curr_bb);

  return alloca;
}

/*
LLVMValueRef
create_entry_alloca2 (LLVMValueRef function, const char *name, LLVMTypeRef type)
{
  LLVMBasicBlockRef entryBlock = LLVMGetEntryBasicBlock (function);

  LLVMBuilderRef new_builder = LLVMCreateBuilder ();

  LLVMPositionBuilderAtEnd (new_builder, entryBlock);

  LLVMValueRef allocaInst = LLVMBuildAlloca (new_builder, type, name);

  LLVMDisposeBuilder (new_builder);

  return allocaInst;
}
*/

LLVMValueRef generate (struct ast *node, struct scope *scope);

LLVMValueRef
generate_cast (struct ast *node, struct scope *scope)
{
  enum type_kind t1 = node->child->expr_type->kind;
  enum type_kind t2 = node->expr_type->kind;

  LLVMValueRef v = generate (node->child, scope);

  // printf ("'%s' to '%s'\n",
  //         type_kind_string (t1),
  //         type_kind_string (t2));

  switch (t1)
    {
    case TYPE_F64:
      if (t2 == TYPE_BOOL)
        {
          LLVMValueRef zero = LLVMConstReal (LLVMDoubleTypeInContext (context), 0.0);
          return LLVMBuildFCmp(builder, LLVMRealONE, v, zero, "f64_to_bool");
        }
      break;
    case TYPE_BOOL:
      if (t2 == TYPE_F64)
        {
          LLVMValueRef i32val = LLVMBuildZExt (builder, v, LLVMInt32TypeInContext (context), "bool_zext");
          return LLVMBuildSIToFP (builder, i32val, LLVMDoubleTypeInContext (context), "bool_to_f64");
        }
      break;
    default:
      break;
    }

  return v;
}

LLVMValueRef
generate_identifier (struct ast *node, struct scope *scope)
{
  const char *name = node->value.token.value.s;
  struct symbol *symbol = scope_find (scope, name);

  if (!symbol)
    return generate_error (node->location, "undefined-variable");

  LLVMValueRef value = symbol->value.value;

  LLVMTypeRef t = LLVMGetElementType (LLVMTypeOf (value));
  LLVMValueRef v = LLVMBuildLoad2 (builder, t, value, name);

  return v;
}

LLVMValueRef
generate_number (struct ast *node, struct scope *scope)
{
  return LLVMConstReal (LLVMDoubleTypeInContext (context),
                        node->value.token.value.f);
}

LLVMValueRef
generate_binary (struct ast *node, struct scope *scope)
{
  // LLVMTypeRef type = LLVMDoubleTypeInContext (context);

  const char *operator = node->child->value.token.value.s;

  if (strcmp (operator, "=") == 0)
    {
      struct symbol *symbol =
        scope_find (scope, node->child->next->value.token.value.s);

      LLVMValueRef var = symbol->value.value;

      LLVMValueRef right = generate (node->child->next->next, scope);
      LLVMBuildStore (builder, right, var);
      return right;
    }

  LLVMValueRef left = generate (node->child->next, scope);
  LLVMValueRef right = generate (node->child->next->next, scope);

  if (!left || !right)
    return NULL;

  if (strcmp (operator, "+") == 0)
    return LLVMBuildFAdd (builder, left, right, "");

  if (strcmp (operator, "-") == 0)
    return LLVMBuildFSub (builder, left, right, "");

  if (strcmp (operator, "*") == 0)
    return LLVMBuildFMul (builder, left, right, "");

  if (strcmp (operator, "/") == 0)
    return LLVMBuildFDiv (builder, left, right, "");

  if (strcmp (operator, "<") == 0)
    {
      return LLVMBuildFCmp (builder, LLVMRealOLT, left, right, "");
    }

  if (strcmp (operator, ">") == 0)
    {
      return LLVMBuildFCmp (builder, LLVMRealOGT, left, right, "");
    }

  if (strcmp (operator, "<=") == 0)
    {
      return LLVMBuildFCmp (builder, LLVMRealOLE, left, right, "");
    }

  if (strcmp (operator, ">=") == 0)
    {
      return LLVMBuildFCmp (builder, LLVMRealOGE, left, right, "");
    }

  LLVMValueRef function = LLVMGetNamedFunction (module, operator);
  if (!function)
    return generate_error (node->location, "undefined operator-function");

  LLVMValueRef arguments[2] = { left, right };

  LLVMTypeRef function_type = LLVMGetElementType(LLVMTypeOf(function));

  LLVMValueRef call = LLVMBuildCall2 (builder, function_type, function,
                                      arguments, 2, "");
  return call;
}

LLVMValueRef
generate_conditional (struct ast *node, struct scope *scope)
{
  LLVMValueRef cond_v = generate (node->child, scope);
  if (!cond_v)
    return NULL;

  LLVMTypeRef result_type = type_kind_to_llvm (node->expr_type->kind);

  LLVMBasicBlockRef current_bb = LLVMGetInsertBlock (builder);
  LLVMValueRef fn = LLVMGetBasicBlockParent (current_bb);

  LLVMBasicBlockRef trueBlock = LLVMAppendBasicBlock(fn, "if.true");
  LLVMBasicBlockRef falseBlock = LLVMAppendBasicBlock(fn, "if.false");
  LLVMBasicBlockRef mergeBlock = LLVMAppendBasicBlock(fn, "if.merge");

  LLVMBuildCondBr(builder, cond_v, trueBlock, falseBlock);

  LLVMPositionBuilderAtEnd(builder, trueBlock);
  LLVMValueRef valTrue = generate (node->child->next, scope);
  LLVMBuildBr(builder, mergeBlock);
  trueBlock = LLVMGetInsertBlock (builder);

  LLVMPositionBuilderAtEnd(builder, falseBlock);
  LLVMValueRef valFalse = generate (node->child->next->next, scope);
  LLVMBuildBr(builder, mergeBlock);
  falseBlock = LLVMGetInsertBlock (builder);

  LLVMPositionBuilderAtEnd(builder, mergeBlock);
  if (LLVMGetTypeKind(result_type) == LLVMVoidTypeKind)
    {
      return NULL;
    }
  else
    {
      LLVMValueRef phi = LLVMBuildPhi(builder, result_type, "iftmp");
      LLVMAddIncoming(phi, &valTrue, &trueBlock, 1);
      LLVMAddIncoming(phi, &valFalse, &falseBlock, 1);
      return phi;
    }


  /*
  LLVMPositionBuilderAtEnd(builder, mergeBlock);
  LLVMValueRef phi = LLVMBuildPhi(builder, result_type, "iftmp");

  LLVMAddIncoming(phi, &valTrue, &trueBlock, 1);
  LLVMAddIncoming(phi, &valFalse, &falseBlock, 1);

  return phi;
  */
}

LLVMValueRef
generate_compound (struct ast *node, struct scope *scope)
{
  struct ast *current = node->child;

  struct scope *child = scope_create (scope);

  LLVMValueRef result;

  while (current)
    {
      result = generate (current, child);
      current = current->next;
    }

  scope_destroy (child);

  return result;
}

LLVMValueRef
generate_while (struct ast *node, struct scope *scope)
{
  LLVMBasicBlockRef current_bb = LLVMGetInsertBlock (builder);
  LLVMValueRef fn = LLVMGetBasicBlockParent (current_bb);

  LLVMBasicBlockRef condBlock = LLVMAppendBasicBlock (fn, "while.cond");
  LLVMBasicBlockRef bodyBlock = LLVMAppendBasicBlock (fn, "while.body");
  LLVMBasicBlockRef endBlock  = LLVMAppendBasicBlock (fn, "while.end");

  LLVMBuildBr (builder, condBlock);

  LLVMPositionBuilderAtEnd (builder, condBlock);
  LLVMValueRef condVal = generate (node->child, scope);
  if (!condVal)
    return NULL;

  LLVMBuildCondBr (builder, condVal, bodyBlock, endBlock);

  LLVMPositionBuilderAtEnd (builder, bodyBlock);
  LLVMValueRef bodyVal = generate (node->child->next, scope);
  if (!bodyVal)
    return NULL;

  LLVMBuildBr(builder, condBlock);
  bodyBlock = LLVMGetInsertBlock(builder);

  LLVMPositionBuilderAtEnd (builder, endBlock);

  return LLVMConstNull(LLVMDoubleType());
}

LLVMValueRef
generate_declaration (struct ast *node, struct scope *scope)
{
  const char *name = node->child->value.token.value.s;

  LLVMBasicBlockRef block = LLVMGetInsertBlock (builder);
  LLVMValueRef function = LLVMGetBasicBlockParent (block);

  LLVMTypeRef type = type_kind_to_llvm (node->expr_type->kind);
  LLVMValueRef alloca = create_entry_alloca (function, name, type);

  if (node->child->next != NULL)
    {
      LLVMValueRef value = generate (node->child->next, scope);
      LLVMBuildStore (builder, value, alloca);
    }

  struct symbol *symbol;

  symbol = symbol_create (SYMBOL_VALUE, name);
  symbol->value.value = alloca;

  scope_add (scope, symbol);

  return alloca;
}

LLVMValueRef
generate_call (struct ast *node, struct scope *scope)
{
  const char *name = node->child->value.token.value.s;
  LLVMValueRef function = LLVMGetNamedFunction (module, name);
  if (!function)
    {
      return generate_error (node->location, "undefined-function");
      // printf ("undefined function '%s'\n", name);
      // return NULL;
    }

  LLVMTypeRef type = LLVMGetElementType(LLVMTypeOf(function));
  int variadic = LLVMIsFunctionVarArg (type);

  struct ast *current;
  size_t n = 0;

  current = node->child;
  while (current->next)
    current = current->next, ++n;

  size_t params = LLVMCountParams (function);
  if ((!variadic && n != params) || (variadic && n < params))
    return generate_error (node->location, "argument # mismatch");
    // {
    //   fprintf (stderr, "\n");
    //   return NULL;
    // }

  LLVMValueRef arguments[n];

  current = node->child->next;
  for (size_t i = 0; i < n; ++i)
    {
      arguments[i] = generate (current, scope);
      if (!arguments[i])
        return NULL;

      current = current->next;
    }

  LLVMValueRef call = LLVMBuildCall2 (builder, type, function, arguments, n,
                                      "");
  return call;
}

LLVMValueRef
generate_prototype (struct ast *node, struct scope *scope)
{
  const char *name = node->child->value.token.value.s;

  struct ast *current;
  size_t n = 0;

  current = node->child;
  while (current->next)
    current = current->next, ++n;

  struct type_function t_func = node->expr_type->value.function;

  LLVMTypeRef arguments[n];
  for (size_t i = 0; i < n; ++i)
    {
      arguments[i] = type_kind_to_llvm (t_func.argument_t[i]->kind);
      // arguments[i] = // LLVMDoubleTypeInContext (context);
    }

  LLVMTypeRef type = LLVMFunctionType (type_kind_to_llvm (t_func.return_t->kind),
                                       arguments, n, node->state);

  LLVMValueRef function = LLVMAddFunction(module, name, type);

  LLVMValueRef argument = LLVMGetFirstParam (function);
  current = node->child->next;

  while (argument != NULL && current != NULL)
    {
      LLVMSetValueName (argument, current->value.token.value.s);
      current = current->next;
      argument = LLVMGetNextParam (argument);
    }

  return function;
}

LLVMValueRef
generate_function (struct ast *node, struct scope *scope)
{
  const char *name = node->child->child->value.token.value.s;
  LLVMValueRef function = LLVMGetNamedFunction (module, name);

  if (!function)
    function = generate_prototype (node->child, scope);

  if (!function)
    return NULL;

  LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext (context, function,
                                                        "entry");
  LLVMPositionBuilderAtEnd (builder, bb);

  size_t n = 0;

  scope_clear (scope);
  for (LLVMValueRef argument = LLVMGetFirstParam (function); argument != NULL;
       argument = LLVMGetNextParam (argument), ++n)
    {
      const char *name = LLVMGetValueName (argument);
      LLVMTypeRef type = type_kind_to_llvm (node->child->expr_type->value.function.argument_t[n]->kind);
      LLVMValueRef alloca = create_entry_alloca (function, name, type);

      LLVMBuildStore (builder, argument, alloca);

      struct symbol *symbol;

      symbol = symbol_create (SYMBOL_VALUE, name);
      symbol->value.value = alloca;

      scope_add (scope, symbol);
    }

  LLVMValueRef returnV = generate (node->child->next, scope);

  if (node->child->expr_type->value.function.return_t->kind != TYPE_VOID)
    LLVMBuildRet (builder, returnV);
  else
    LLVMBuildRetVoid (builder);

  LLVMRunFunctionPassManager(pass_manager, function);

  return function;
}

LLVMValueRef
generate_program (struct ast *node, struct scope *scope)
{
  struct ast *current = node->child;

  while (current != NULL)
    {
      generate (current, scope);
      current = current->next;
    }

  return NULL;
}

LLVMValueRef
generate (struct ast *node, struct scope *scope)
{
  switch (node->type)
    {
    case AST_ERROR:
      return NULL;
    case AST_CAST:
      return generate_cast (node, scope);
    case AST_IDENTIFIER:
      return generate_identifier (node, scope);
    case AST_NUMBER:
      return generate_number (node, scope);
    case AST_BINARY:
      return generate_binary (node, scope);
    case AST_CONDITIONAL:
      return generate_conditional (node, scope);
    case AST_COMPOUND:
      return generate_compound (node, scope);
    case AST_WHILE:
      return generate_while (node, scope);
    case AST_DECLARATION:
      return generate_declaration (node, scope);
    case AST_CALL:
      return generate_call (node, scope);
    case AST_PROTOTYPE:
      return generate_prototype (node, scope);
    case AST_FUNCTION:
      return generate_function (node, scope);
    case AST_PROGRAM:
      return generate_program (node, scope);
    }
}


/* -------------------------------------------------------------------------- */

void
ast_type_match (enum type_kind a, enum type_kind b, struct location location)
{
  if (a != b)
    {
      location_debug_print (location);
      printf (": fatal-error: expected %s, got %s\n",
              type_kind_string (b), type_kind_string (a));
      abort ();
    }
}

/*
struct comptime_function
{
  const char *name;
  struct type *type;
};

struct comptime_function functions[256];
size_t functions_n = 0;

struct comptime_variable
{
  const char *name;
  struct type *type;
};

struct comptime_variable variables[256];
size_t variables_n = 0;
*/

struct type *
ast_type_check (struct ast *ast, struct arena *arena, struct scope *fs,
                struct scope *vs)
{
  switch (ast->type)
    {
    case AST_ERROR:
      break;
    case AST_CAST:
      {
        struct type *type = ast_type_check (ast->child, arena, fs, vs);

        if (!type_can_cast (type->kind, ast->expr_type->kind))
          {
            printf ("ERROR: cannot cast %s to %s\n",
                    type_kind_string (type->kind),
                    type_kind_string (ast->expr_type->kind));
            abort ();
          }
        return ast->expr_type;
      }
      break;
    case AST_IDENTIFIER:
      {
        struct symbol *symbol = scope_find (vs, ast->value.token.value.s);
        if (symbol)
          {
            ast->expr_type = symbol->value.type;
            return ast->expr_type;
          }

        printf ("undefined %s\n", ast->value.token.value.s);
        abort();
      }
      break;
    case AST_NUMBER:
      return ast->expr_type;
    case AST_BINARY:
      {
        struct type *left = ast_type_check (ast->child->next, arena, fs, vs);
        struct type *right = ast_type_check (ast->child->next->next, arena, fs, vs);
        struct location l = ast->location;
        const char *operator = ast->child->value.token.value.s;

        if (strcmp (operator, "=") == 0)
          {
            ast_type_match (right->kind, left->kind, l);
            ast->expr_type = left;
            return left;
          }

        if (strcmp (operator, "+") == 0)
          {
            ast_type_match (left->kind, TYPE_F64, l);
            ast_type_match (right->kind, TYPE_F64, l);
            ast->expr_type = left;
            return left;
          }

        if (strcmp (operator, "-") == 0)
          {
            ast_type_match (left->kind, TYPE_F64, l);
            ast_type_match (right->kind, TYPE_F64, l);
            ast->expr_type = left;
            return left;
          }

        if (strcmp (operator, "*") == 0)
          {
            ast_type_match (left->kind, TYPE_F64, l);
            ast_type_match (right->kind, TYPE_F64, l);
            ast->expr_type = left;
            return left;
          }

        if (strcmp (operator, "/") == 0)
          {
            ast_type_match (left->kind, TYPE_F64, l);
            ast_type_match (right->kind, TYPE_F64, l);
            ast->expr_type = left;
            return left;
          }

        if (strcmp (operator, "<") == 0)
          {
            ast_type_match (left->kind, TYPE_F64, l);
            ast_type_match (right->kind, TYPE_F64, l);
            struct type *type = type_create (TYPE_BOOL, arena);
            ast->expr_type = type;
            return type;
          }

        if (strcmp (operator, ">") == 0)
          {
            ast_type_match (left->kind, TYPE_F64, l);
            ast_type_match (right->kind, TYPE_F64, l);
            struct type *type = type_create (TYPE_BOOL, arena);
            ast->expr_type = type;
            return type;
          }

        if (strcmp (operator, "<=") == 0)
          {
            ast_type_match (left->kind, TYPE_F64, l);
            ast_type_match (right->kind, TYPE_F64, l);
            struct type *type = type_create (TYPE_BOOL, arena);
            ast->expr_type = type;
            return type;
          }

        if (strcmp (operator, ">=") == 0)
          {
            ast_type_match (left->kind, TYPE_F64, l);
            ast_type_match (right->kind, TYPE_F64, l);
            struct type *type = type_create (TYPE_BOOL, arena);
            ast->expr_type = type;
            return type;
          }

        struct symbol *symbol = scope_find (fs, operator);

        if (symbol)
          {
            struct type_function function = symbol->value.type->value.function;
            ast_type_match (left->kind, function.argument_t[0]->kind, l);
            ast_type_match (right->kind, function.argument_t[1]->kind, l);
            ast->expr_type = function.return_t;
            return function.return_t;
          }

        printf ("ERROR: undefined operator-function %s\n", operator);
        abort ();
        /* */

      }
      break;
    case AST_CONDITIONAL:
      {
        struct type *cond = ast_type_check (ast->child, arena, fs, vs);

        ast_type_match (cond->kind, TYPE_BOOL, ast->child->location);

        struct type *then_t = ast_type_check (ast->child->next, arena, fs, vs);
        struct type *else_t = ast_type_check (ast->child->next->next, arena, fs, vs);
        ast_type_match (else_t->kind, then_t->kind, ast->child->next->next->location);

        ast->expr_type = then_t;
        return then_t;
      }
      break;
    case AST_COMPOUND:
      {
        struct scope *child = scope_create (vs);

        struct ast *current = ast->child;
        struct type *type;
        while (current != NULL)
          type = ast_type_check (current, arena, fs, child),
          current = current->next;
        ast->expr_type = type;

        scope_destroy (child);
        if (ast->state == 1)
          return type_create (TYPE_VOID, arena);
        return type;
      }
      break;
    case AST_WHILE:
      {
        struct type *cond = ast_type_check (ast->child, arena, fs, vs);
        ast_type_match (cond->kind, TYPE_BOOL, ast->child->location);
        (void)ast_type_check (ast->child->next, arena, fs, vs);
        return cond;
      }
      break;
    case AST_DECLARATION:
      {
        if (ast->child->next)
          {
            struct type *right = ast_type_check (ast->child->next, arena, fs, vs);
            ast_type_match (right->kind, ast->expr_type->kind, ast->location);
          }

        struct symbol *symbol;

        symbol = symbol_create (SYMBOL_TYPE, ast->child->value.token.value.s);
        symbol->value.type = ast->expr_type;

        scope_add (vs, symbol);

        return ast->expr_type;
      }
      break;
    case AST_CALL:
      {
        struct symbol *symbol = scope_find (fs, ast->child->value.token.value.s);

        if (symbol)
          {
            struct ast *current = ast->child->next;
            size_t n = 0;

            struct type_function function = symbol->value.type->value.function;

            while (current != NULL)
              {
                if (n >= function.argument_n)
                  {
                    printf ("ERROR: argument mismatch >\n");
                    abort ();
                  }

                struct type *argument = ast_type_check (current, arena, fs, vs);

                ast_type_match (argument->kind, function.argument_t[n]->kind,
                                current->location);

                current = current->next;
                n++;
              }

            if (n < function.argument_n)
              {
                printf ("ERROR: argument mismatch <\n");
                abort ();
              }

            ast->expr_type = function.return_t;
            return function.return_t;
          }

        printf ("ERROR: call to undefined function\n");
        abort ();
      }
      break;
    case AST_PROTOTYPE:
      {
        struct type *ast_t = ast->expr_type;
        struct type_function function = ast_t->value.function;

        ast_type_match (ast_t->kind, TYPE_FUNCTION, ast->location);

        struct ast *current = ast->child->next;
        size_t n = 0;
        while (current != NULL)
          n++, current = current->next;

        size_t params = function.argument_n;

        if (params != n)
          {
            printf ("type-argument amount mismatch\n");
            abort ();
          }

        struct symbol *new_symbol;

        new_symbol = symbol_create (SYMBOL_TYPE, ast->child->value.token.value.s);
        new_symbol->value.type = ast_t;

        struct symbol *old_symbol = scope_find (fs, new_symbol->name);

        if (old_symbol)
          {
            if (!type_match (new_symbol->value.type, old_symbol->value.type))
              {
                printf ("ERROR: Declared and defined function's types don't match\n");
                printf ("NOTE: '");
                type_debug_print (new_symbol->value.type);
                printf ("' and '");
                type_debug_print (old_symbol->value.type);
                printf ("'\n");

                abort();
              }
          }

        scope_add (fs, new_symbol);

        return ast_t;
      }
      break;
    case AST_FUNCTION:
      {
        struct type *result = ast_type_check (ast->child, arena, fs, vs);

        struct ast *current = ast->child->child->next;
        size_t n = 0;

        while (current != NULL)
          {
            // struct comptime_variable v;
            // v.name = current->value.token.value.s;
            // v.type = result->value.function.argument_t[n];
            // // printf ("variable, %s, ", v.name);
            // // type_debug_print (v.type);
            // // printf ("\n");
            // variables[variables_n++] = v;

            struct symbol *symbol;

            symbol = symbol_create (SYMBOL_TYPE, current->value.token.value.s);
            symbol->value.type = result->value.function.argument_t[n];

            scope_add (vs, symbol);

            current = current->next;
            n++;
          }

        struct type *return_t = ast_type_check (ast->child->next, arena, fs, vs);
        ast_type_match (return_t->kind, result->value.function.return_t->kind,
                        ast->child->next->location);

        return result;
      }
      break;
    case AST_PROGRAM:
      {
        struct ast *current = ast->child;
        while (current != NULL)
          {
            scope_clear (vs);
            ast_type_check (current, arena, fs, vs);
            current = current->next;
          }
        return NULL;
      }
      break;
    }

  printf ("%s Bad Node\n", ast_type_string (ast->type));
  abort ();
}

/* -------------------------------------------------------------------------- */



int
main (int argc, char *argv[])
{
  (void)argc, (void)argv;

  /*
  struct type_function function;
  function.argument_t = calloc (3, sizeof (struct type *));
  function.argument_n = 3;

  function.argument_t[0] = type_create (TYPE_F64, NULL);
  function.argument_t[1] = type_create (TYPE_BOOL, NULL);
  function.argument_t[2] = type_create (TYPE_F64, NULL);

  function.return_t = type_create (TYPE_VOID, NULL);

  function.variadic = 1;

  struct type *type = type_create_f (function, NULL);

  printf ("\n\n");
  type_debug_print (type);
  printf ("\n\n\n");
  */


  if (argc < 2)
    {
      printf ("Usage: %s <file>\n", argv[0]);
      abort ();
    }

  // 1 < 2 || 2 > 3
  // 1 < 2 || 2 > 3 && 2 < 3

  // 1 && 2 == 3

  precedence_table_add ("=", 10);

  precedence_table_add ("<", 80);
  precedence_table_add (">", 80);
  precedence_table_add ("<=", 80);
  precedence_table_add (">=", 80);

  precedence_table_add ("+", 90);
  precedence_table_add ("-", 90);

  precedence_table_add ("*", 100);
  precedence_table_add ("/", 100);


  char path_copy[PATH_MAX];
  char dir_copy[PATH_MAX];
  char base_copy[PATH_MAX];

  // Copy argv[1] to avoid modifying the original string
  strncpy(path_copy, argv[1], PATH_MAX - 1);
  path_copy[PATH_MAX - 1] = '\0';

  // Extract directory name safely
  strncpy(dir_copy, path_copy, PATH_MAX - 1);
  dir_copy[PATH_MAX - 1] = '\0';
  char *dir = dirname(dir_copy);

  // Extract base filename safely
  strncpy(base_copy, argv[1], PATH_MAX - 1);
  base_copy[PATH_MAX - 1] = '\0';
  char *base = basename(base_copy);

  char *dot = strrchr(base, '.');
  if (dot) {
      *dot = '\0';
  }

  // Build the new filenames using snprintf for safety
  char o_file[PATH_MAX];
  char ll_file[PATH_MAX];
  if ((unsigned)snprintf(o_file, sizeof(o_file), "%s/%s.o", dir, base) >= sizeof(o_file) ||
      (unsigned)snprintf(ll_file, sizeof(ll_file), "%s/%s.ll", dir, base) >= sizeof(ll_file)) {
      fprintf(stderr, "Path too long.\n");
      return 1;
  }

  char s_file[PATH_MAX];
  if ((unsigned)snprintf(s_file, sizeof(s_file), "%s/%s.s", dir, base) >= sizeof(s_file)) {
      fprintf(stderr, "Path too long.\n");
      return 1;
  }

  // printf("Output .o file: %s\n", o_file);
  // printf("Output .ll file: %s\n", ll_file);

  context = LLVMContextCreate ();
  module = LLVMModuleCreateWithNameInContext ("Main", context);
  builder = LLVMCreateBuilderInContext (context);

  pass_manager = LLVMCreateFunctionPassManagerForModule(module);
  LLVMAddPromoteMemoryToRegisterPass (pass_manager);  // mem2reg
  LLVMInitializeFunctionPassManager (pass_manager);

  char *buffer = read_file (argv[1]);

  struct arena lexer_arena = {0};
  struct lexer lexer = lexer_create (buffer, argv[1], &lexer_arena);

  struct arena parser_arena = {0};
  struct parser parser = parser_create (&lexer, &parser_arena);

  struct ast *ast = parser_parse (&parser);

  if (ast_match_error (ast))
    {
      location_debug_print (ast->location);
      printf (": fatal-error: %s [SYNTAX]\n", ast->value.error.message);

      arena_destroy (&lexer_arena);
      arena_destroy (&parser_arena);

      exit (1);
    }
  // else
  //   ast_debug_print (ast, 0);

  printf ("\n");

  struct arena type_check_arena = {0};

  ast_type_check (ast, &type_check_arena, scope_create (NULL), scope_create (NULL));
  // printf ("-------------------\n");
  // ast_debug_print (ast, 0);
  // printf ("-------------------\n");

  struct scope *scope;

  scope = scope_create (NULL);

  (void)generate (ast, scope);
  if (has_error)
    exit (1);

  arena_destroy (&lexer_arena);
  arena_destroy (&parser_arena);

  arena_destroy (&type_check_arena);

  arena_destroy (&lexer_arena);

  // LLVMDumpModule (module);

  char *error;
  LLVMPrintModuleToFile (module, ll_file, &error);

  LLVMInitializeAllTargetInfos();
  LLVMInitializeAllTargets();
  LLVMInitializeAllTargetMCs();
  LLVMInitializeAllAsmPrinters();
  LLVMInitializeAllAsmParsers();

  // Get target triple
  char *triple = LLVMGetDefaultTargetTriple();

  LLVMTargetRef target;
  if (LLVMGetTargetFromTriple(triple, &target, &error) != 0) {
      fprintf(stderr, "Error getting target: %s\n", error);
      LLVMDisposeMessage(error);
      exit(1);
  }

  // Create target machine
  LLVMTargetMachineRef target_machine = LLVMCreateTargetMachine(
      target,
      triple,
      "",     // CPU
      "",     // Features
      LLVMCodeGenLevelDefault,
      LLVMRelocDefault,
      LLVMCodeModelDefault
  );

  // Set triple and datalayout on module
  LLVMSetTarget(module, triple);

  LLVMTargetDataRef data_layout = LLVMCreateTargetDataLayout(target_machine);
  char *layout_str = LLVMCopyStringRepOfTargetData(data_layout);
  LLVMSetDataLayout(module, layout_str);
  LLVMDisposeMessage(layout_str);

  // Verify module (optional but useful)
  if (LLVMVerifyModule(module, LLVMAbortProcessAction, &error) != 0) {
      fprintf(stderr, "Module verification failed: %s\n", error);
      LLVMDisposeMessage(error);
      exit(1);
  }

  LLVMDisposeMessage(error);

  // Emit object file
  if (LLVMTargetMachineEmitToFile(
          target_machine,
          module,
          o_file,
          LLVMObjectFile,
          &error) != 0) {
      fprintf(stderr, "Error emitting object file: %s\n", error);
      LLVMDisposeMessage(error);
      exit(1);
  }

  if (LLVMTargetMachineEmitToFile(
        target_machine,
        module,
        s_file,
        LLVMAssemblyFile,
        &error) != 0) {
    fprintf(stderr, "Error emitting assembly file: %s\n", error);
    LLVMDisposeMessage(error);
    exit(1);
  }

  // Clean up
  LLVMDisposeTargetMachine(target_machine);
  LLVMDisposeMessage(triple);
  LLVMDisposeTargetData(data_layout);

  LLVMDisposeBuilder (builder);
  LLVMDisposeModule (module);
  LLVMContextDispose (context);

  precedence_table_destroy ();

  printf ("3 files generated: %s, %s, %s\n", ll_file, s_file, o_file);

  free (buffer);

  return 0;
}

