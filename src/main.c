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

typedef struct NamedValue {
  char *name;
  LLVMValueRef value;
  struct NamedValue *next;
} NamedValue;


NamedValue *NamedValues = NULL;

// Helper to add a named value.
void addNamedValue(const char *name, LLVMValueRef value) {
  NamedValue *nv = malloc(sizeof(NamedValue));
  nv->name = strdup(name);
  nv->value = value;
  nv->next = NamedValues;
  NamedValues = nv;
}

// Helper to look up a named value.
LLVMValueRef lookupNamedValue(const char *name) {
  for (NamedValue *nv = NamedValues; nv; nv = nv->next)
    if (strcmp(nv->name, name) == 0)
      return nv->value;
  return NULL;
}

// Clear the named values map.
void clearNamedValues() {
  NamedValue *nv = NamedValues;
  while (nv) {
    NamedValue *next = nv->next;
    free(nv->name);
    free(nv);
    nv = next;
  }
  NamedValues = NULL;
}


LLVMValueRef
create_entry_alloca (LLVMValueRef function, const char *name, LLVMTypeRef type)
{
  LLVMBasicBlockRef entryBlock = LLVMGetEntryBasicBlock (function);

  LLVMBuilderRef builder = LLVMCreateBuilder ();

  LLVMPositionBuilderAtEnd (builder, entryBlock);

  LLVMValueRef allocaInst = LLVMBuildAlloca (builder, type, name);

  LLVMDisposeBuilder (builder);

  return allocaInst;
}

LLVMValueRef generate (struct ast *node);

LLVMValueRef
generate_cast (struct ast *node)
{
  enum type_kind t1 = node->child->expr_type->kind;
  enum type_kind t2 = node->expr_type->kind;

  LLVMValueRef v = generate (node->child);

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
generate_identifier (struct ast *node)
{
  const char *name = node->value.token.value.s;
  LLVMValueRef value = lookupNamedValue (name);
  if (!value)
    return generate_error (node->location, "undefined-variable");

  LLVMTypeRef t = LLVMGetElementType (LLVMTypeOf (value));
  LLVMValueRef v = LLVMBuildLoad2 (builder, t, value, name);

  return v;
}

LLVMValueRef
generate_number (struct ast *node)
{
  return LLVMConstReal (LLVMDoubleTypeInContext (context),
                        node->value.token.value.f);
}

LLVMValueRef
generate_binary (struct ast *node)
{
  // LLVMTypeRef type = LLVMDoubleTypeInContext (context);

  const char *operator = node->child->value.token.value.s;

  if (strcmp (operator, "=") == 0)
    {
      // node->child->next;
      LLVMValueRef var = lookupNamedValue (node->child->next->value.token.value.s);

      LLVMValueRef right = generate (node->child->next->next);
      return LLVMBuildStore (builder, right, var);
    }

  LLVMValueRef left = generate (node->child->next);
  LLVMValueRef right = generate (node->child->next->next);

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
generate_conditional (struct ast *node)
{
  LLVMValueRef cond_v = generate (node->child);
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
  LLVMValueRef valTrue = generate (node->child->next);
  LLVMBuildBr(builder, mergeBlock);
  trueBlock = LLVMGetInsertBlock (builder);

  LLVMPositionBuilderAtEnd(builder, falseBlock);
  LLVMValueRef valFalse = generate (node->child->next->next);
  LLVMBuildBr(builder, mergeBlock);
  falseBlock = LLVMGetInsertBlock (builder);

  LLVMPositionBuilderAtEnd(builder, mergeBlock);
  LLVMValueRef phi = LLVMBuildPhi(builder, result_type, "iftmp");

  LLVMAddIncoming(phi, &valTrue, &trueBlock, 1);
  LLVMAddIncoming(phi, &valFalse, &falseBlock, 1);

  return phi;



  /*LLVMTypeRef type = LLVMDoubleTypeInContext (context);
  LLVMValueRef zero = LLVMConstReal (type, 0.0);

  cond_v = LLVMBuildFCmp (builder, LLVMRealONE, cond_v, zero, "cond");

  LLVMBasicBlockRef current_bb = LLVMGetInsertBlock (builder);
  LLVMValueRef function = LLVMGetBasicBlockParent (current_bb);

  LLVMBasicBlockRef then_bb  = LLVMAppendBasicBlockInContext (context, function, "");
  LLVMBasicBlockRef else_bb  = LLVMAppendBasicBlockInContext (context, function, "");
  LLVMBasicBlockRef merge_bb = LLVMAppendBasicBlockInContext (context, function, "");

  LLVMBuildCondBr (builder, cond_v, then_bb, else_bb);

  LLVMPositionBuilderAtEnd (builder, then_bb);
  LLVMValueRef then_v = generate (node->child->next);
  if (!then_v)
    return NULL;

  LLVMBuildBr (builder, merge_bb);
  then_bb = LLVMGetInsertBlock (builder);

  LLVMPositionBuilderAtEnd (builder, else_bb);
  LLVMValueRef else_v = generate (node->child->next->next);
  if (!else_v)
    return NULL;

  LLVMBuildBr (builder, merge_bb);
  else_bb = LLVMGetInsertBlock (builder);

  LLVMPositionBuilderAtEnd (builder, merge_bb);

  LLVMValueRef phi = LLVMBuildPhi (builder, type, "");

  LLVMValueRef in_v[2] = { then_v, else_v };
  LLVMBasicBlockRef in_bb[2] = { then_bb, else_bb };
  LLVMAddIncoming (phi, in_v, in_bb, 2);

  return phi;*/
}

LLVMValueRef
generate_compound (struct ast *node)
{
  struct ast *current = node->child;
  LLVMValueRef result;

  while (current)
    {
      result = generate (current);
      if (result == NULL)
        return NULL;

      current = current->next;
    }

  return result;
}

LLVMValueRef
generate_declaration (struct ast *node)
{
  const char *name = node->child->value.token.value.s;

  LLVMBasicBlockRef block = LLVMGetInsertBlock (builder);
  LLVMValueRef function = LLVMGetBasicBlockParent (block);

  LLVMTypeRef type = type_kind_to_llvm (node->expr_type->kind);
  LLVMValueRef alloca = create_entry_alloca (function, name, type);

  if (node->child->next != NULL)
    {
      LLVMValueRef value = generate (node->child->next);
      LLVMBuildStore (builder, value, alloca);
    }

  addNamedValue (name, alloca);

  return alloca;
}

LLVMValueRef
generate_call (struct ast *node)
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
      arguments[i] = generate (current);
      if (!arguments[i])
        return NULL;

      current = current->next;
    }

  LLVMValueRef call = LLVMBuildCall2 (builder, type, function, arguments, n,
                                      "");
  return call;
}

LLVMValueRef
generate_prototype (struct ast *node)
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
generate_function (struct ast *node)
{
  const char *name = node->child->child->value.token.value.s;
  LLVMValueRef function = LLVMGetNamedFunction (module, name);

  if (!function)
    function = generate_prototype (node->child);

  if (!function)
    return NULL;

  LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext (context, function,
                                                        "entry");
  LLVMPositionBuilderAtEnd (builder, bb);

  // for (auto &Arg : TheFunction->args()) {
  //   // Create an alloca for this variable.
  //   AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, Arg.getName());

  //   // Store the initial value into the alloca.
  //   Builder->CreateStore(&Arg, Alloca);

  //   // Add arguments to variable symbol table.
  //   NamedValues[std::string(Arg.getName())] = Alloca;
  // }

  size_t n = 0;

  clearNamedValues ();
  for (LLVMValueRef argument = LLVMGetFirstParam (function); argument != NULL;
       argument = LLVMGetNextParam (argument), ++n)
    {
      const char *name = LLVMGetValueName (argument);
      LLVMTypeRef type = type_kind_to_llvm (node->child->expr_type->value.function.argument_t[n]->kind);
      LLVMValueRef alloca = create_entry_alloca (function, name, type);

      LLVMBuildStore (builder, argument, alloca);

      addNamedValue (name, alloca);
    }

  LLVMValueRef returnV = generate (node->child->next);

  if (node->child->expr_type->value.function.return_t->kind != TYPE_VOID)
    LLVMBuildRet (builder, returnV);
  else
    LLVMBuildRetVoid (builder);

  return function;
}

LLVMValueRef
generate_program (struct ast *node)
{
  struct ast *current = node->child;

  while (current != NULL)
    {
      if (generate (current) == NULL)
        return NULL;

      current = current->next;
    }

  return NULL;
}

LLVMValueRef
generate (struct ast *node)
{
  switch (node->type)
    {
    case AST_ERROR:
      return NULL;
    case AST_CAST:
      return generate_cast (node);
    case AST_IDENTIFIER:
      return generate_identifier (node);
    case AST_NUMBER:
      return generate_number (node);
    case AST_BINARY:
      return generate_binary (node);
    case AST_CONDITIONAL:
      return generate_conditional (node);
    case AST_COMPOUND:
      return generate_compound (node);
    case AST_DECLARATION:
      return generate_declaration (node);
    case AST_CALL:
      return generate_call (node);
    case AST_PROTOTYPE:
      return generate_prototype (node);
    case AST_FUNCTION:
      return generate_function (node);
    case AST_PROGRAM:
      return generate_program (node);
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

struct type *
ast_type_check (struct ast *ast, struct arena *arena)
{
  switch (ast->type)
    {
    case AST_ERROR:
      break;
    case AST_CAST:
      {
        struct type *type = ast_type_check (ast->child, arena);

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
        for (size_t i = 0; i < variables_n; ++i)
          {
            if (strcmp (ast->value.token.value.s, variables[i].name) == 0)
              {
                ast->expr_type = variables[i].type;
                return ast->expr_type;
              }
          }

        printf ("undefined %s\n", ast->value.token.value.s);
        abort();
      }
      break;
    case AST_NUMBER:
      return ast->expr_type;
    case AST_BINARY:
      {
        struct type *left = ast_type_check (ast->child->next, arena);
        struct type *right = ast_type_check (ast->child->next->next, arena);
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

        for (size_t i = 0; i < functions_n; ++i)
          if (strcmp (operator, functions[i].name) == 0)
            {
              struct type_function function = functions[i].type->value.function;
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
        struct type *cond = ast_type_check (ast->child, arena);

        ast_type_match (cond->kind, TYPE_BOOL, ast->child->location);

        struct type *then_t = ast_type_check (ast->child->next, arena);
        struct type *else_t = ast_type_check (ast->child->next->next, arena);
        ast_type_match (else_t->kind, then_t->kind, ast->child->next->next->location);

        ast->expr_type = then_t;
        return then_t;
      }
      break;
    case AST_COMPOUND:
      {
        struct ast *current = ast->child;
        struct type *type;
        while (current != NULL)
          type = ast_type_check (current, arena),
          current = current->next;
        ast->expr_type = type;
        return type;
      }
      break;
    case AST_DECLARATION:
      {
        struct type *right = ast_type_check (ast->child->next, arena);
        ast_type_match (right->kind, ast->expr_type->kind, ast->location);
        variables[variables_n].name = ast->child->value.token.value.s;
        variables[variables_n].type = right;
        variables_n++;
        return right;
      }
      break;
    case AST_CALL:
      {
        for (size_t i = 0; i < functions_n; ++i)
          if (strcmp (ast->child->value.token.value.s, functions[i].name) == 0)
            {
              struct ast *current = ast->child->next;
              size_t n = 0;

              struct type_function function = functions[i].type->value.function;

              while (current != NULL)
                {
                  if (n >= function.argument_n)
                    {
                      printf ("ERROR: argument mismatch >\n");
                      abort ();
                    }

                  struct type *argument = ast_type_check (current, arena);

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

        struct comptime_function f;
        f.name = ast->child->value.token.value.s;
        f.type = ast_t;

        for (size_t i = 0; i < functions_n; ++i)
          {
            if (strcmp (f.name, functions[i].name) == 0)
              {
                if (type_match (f.type, functions[i].type))
                  {
                    functions[i] = f;
                    break;
                  }
                printf ("ERROR: Declared and defined function's types don't match\n");
                printf ("NOTE: '");
                type_debug_print (f.type);
                printf ("' and '");
                type_debug_print (functions[i].type);
                printf ("'\n");

                abort();
              }
          }

        functions[functions_n++] = f;

        return ast_t;
        // printf ("PROTO\n");
      }
      break;
    case AST_FUNCTION:
      {
        struct type *result = ast_type_check (ast->child, arena);

        struct ast *current = ast->child->child->next;
        size_t n = 0;

        while (current != NULL)
          {
            struct comptime_variable v;
            v.name = current->value.token.value.s;
            v.type = result->value.function.argument_t[n];
            // printf ("variable, %s, ", v.name);
            // type_debug_print (v.type);
            // printf ("\n");
            variables[variables_n++] = v;
            current = current->next;
            n++;
          }

        struct type *return_t = ast_type_check (ast->child->next, arena);
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
            variables_n = 0;
            ast_type_check (current, arena);
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

  ast_type_check (ast, &type_check_arena);
  // printf ("-------------------\n");
  // ast_debug_print (ast, 0);
  // printf ("-------------------\n");

  (void)generate (ast);
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

