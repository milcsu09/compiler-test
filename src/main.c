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
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Analysis.h>

LLVMContextRef context;
LLVMModuleRef module;
LLVMBuilderRef builder;

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

LLVMValueRef generate (struct ast *node);

LLVMValueRef
generate_identifier (struct ast *node)
{
  const char *name = node->value.token.value.s;
  LLVMValueRef value = lookupNamedValue (name);
  if (!value)
    {
      printf ("undefined variable '%s'\n", name);
      return NULL;
    }

  return value;
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
  LLVMValueRef left = generate (node->child->next);
  LLVMValueRef right = generate (node->child->next->next);

  if (!left || !right)
    return NULL;

  const char *operator = node->child->value.token.value.s;

  if (strcmp (operator, "+") == 0)
    return LLVMBuildFAdd (builder, left, right, "");

  if (strcmp (operator, "-") == 0)
    return LLVMBuildFSub (builder, left, right, "");

  if (strcmp (operator, "*") == 0)
    return LLVMBuildFMul (builder, left, right, "");

  if (strcmp (operator, "/") == 0)
    return LLVMBuildFDiv (builder, left, right, "");

  fprintf (stderr, "invalid binary operation '%s'\n", operator);
  abort ();
}

LLVMValueRef
generate_call (struct ast *node)
{
  const char *name = node->child->value.token.value.s;
  LLVMValueRef function = LLVMGetNamedFunction (module, name);
  if (!function)
    {
      printf ("undefined function '%s'\n", name);
      return NULL;
    }

  struct ast *current;
  size_t n = 0;

  current = node->child;
  while (current->next)
    current = current->next, ++n;

  if (LLVMCountParams (function) != n)
    {
      printf ("argument # mismatch\n");
      return NULL;
    }

  LLVMValueRef arguments[n];

  current = node->child->next;
  for (size_t i = 0; i < n; ++i)
    {
      arguments[i] = generate (current);
      if (!arguments[i])
        return NULL;

      current = current->next;
    }

  LLVMTypeRef type = LLVMGetElementType(LLVMTypeOf(function));

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

  LLVMTypeRef arguments[n];
  for (size_t i = 0; i < n; ++i)
    arguments[i] = LLVMDoubleTypeInContext (context);

  LLVMTypeRef type = LLVMFunctionType (LLVMDoubleTypeInContext (context),
                                       arguments, n, 0);

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

  clearNamedValues ();
  for (LLVMValueRef argument = LLVMGetFirstParam (function); argument != NULL;
       argument = LLVMGetNextParam (argument))
    addNamedValue (LLVMGetValueName (argument), argument);

  LLVMValueRef returnV = generate (node->child->next);
  if (returnV)
    {
      LLVMBuildRet (builder, returnV);
      return function;
    }

  LLVMDeleteFunction (function);
  return NULL;
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
    case AST_IDENTIFIER:
      return generate_identifier (node);
    case AST_NUMBER:
      return generate_number (node);
    case AST_BINARY:
      return generate_binary (node);
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

int
main (int argc, char *argv[])
{
  (void)argc, (void)argv;

  if (argc != 2)
    {
      printf ("Usage: %s <file>\n", argv[0]);
      abort ();
    }

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

  LLVMValueRef v = generate (ast);
  (void)v;

  arena_destroy (&lexer_arena);
  arena_destroy (&parser_arena);

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

  // Clean up
  LLVMDisposeTargetMachine(target_machine);
  LLVMDisposeMessage(triple);
  LLVMDisposeTargetData(data_layout);

  LLVMDisposeBuilder (builder);
  LLVMDisposeModule (module);
  LLVMContextDispose (context);

  printf ("%s, %s\n", ll_file, o_file);

  return 0;
}

