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
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Support.h>

#include <llvm-c/Transforms/Scalar.h>
#include <llvm-c/Transforms/PassManagerBuilder.h>
#include <llvm-c//Transforms/Utils.h>

int optimize_function = 1;
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
    return generate_error (node->location, "undefined-variable");

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
  LLVMTypeRef type = LLVMDoubleTypeInContext (context);

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

  if (strcmp (operator, "<") == 0)
    {
      LLVMValueRef cmp = LLVMBuildFCmp (builder, LLVMRealOLT, left, right, "");
      return LLVMBuildUIToFP (builder, cmp, type, "");
    }

  if (strcmp (operator, ">") == 0)
    {
      LLVMValueRef cmp = LLVMBuildFCmp (builder, LLVMRealOGT, left, right, "");
      return LLVMBuildUIToFP (builder, cmp, type, "");
    }

  if (strcmp (operator, "<=") == 0)
    {
      LLVMValueRef cmp = LLVMBuildFCmp (builder, LLVMRealOLE, left, right, "");
      return LLVMBuildUIToFP (builder, cmp, type, "");
    }

  if (strcmp (operator, ">=") == 0)
    {
      LLVMValueRef cmp = LLVMBuildFCmp (builder, LLVMRealOGE, left, right, "");
      return LLVMBuildUIToFP (builder, cmp, type, "");
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

  LLVMTypeRef type = LLVMDoubleTypeInContext (context);
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

  return phi;
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

  LLVMTypeRef arguments[n];
  for (size_t i = 0; i < n; ++i)
    arguments[i] = LLVMDoubleTypeInContext (context);

  LLVMTypeRef type = LLVMFunctionType (LLVMDoubleTypeInContext (context),
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

  clearNamedValues ();
  for (LLVMValueRef argument = LLVMGetFirstParam (function); argument != NULL;
       argument = LLVMGetNextParam (argument))
    addNamedValue (LLVMGetValueName (argument), argument);

  LLVMValueRef returnV = generate (node->child->next);
  if (returnV)
    {
      LLVMBuildRet (builder, returnV);

      if (optimize_function)
        {
          LLVMPassManagerRef function_pass_manager = LLVMCreateFunctionPassManagerForModule(module);
          LLVMAddBasicAliasAnalysisPass(function_pass_manager);
          LLVMAddPromoteMemoryToRegisterPass(function_pass_manager);
          LLVMAddInstructionCombiningPass(function_pass_manager);
          LLVMAddReassociatePass(function_pass_manager);
          LLVMAddGVNPass(function_pass_manager);
          LLVMAddCFGSimplificationPass(function_pass_manager);
          LLVMInitializeFunctionPassManager(function_pass_manager);
          LLVMRunFunctionPassManager(function_pass_manager, function);
          LLVMFinalizeFunctionPassManager(function_pass_manager);
          LLVMDisposePassManager(function_pass_manager);
        }

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
    case AST_CONDITIONAL:
      return generate_conditional (node);
    case AST_COMPOUND:
      return generate_compound (node);
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

  if (argc < 2)
    {
      printf ("Usage: %s <file>\n", argv[0]);
      abort ();
    }

  // 1 < 2 || 2 > 3
  // 1 < 2 || 2 > 3 && 2 < 3

  // 1 && 2 == 3

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

  (void)generate (ast);
  if (has_error)
    exit (1);

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

  return 0;
}

