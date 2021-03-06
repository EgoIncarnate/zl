#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "config.h"

#include "parse.hpp"
#include "ast.hpp"
#include "parse_op.hpp"
#include "peg.hpp"
#include "expand.hpp"

extern "C" {
//#include <gc_backptr.h>
  void GC_dump();
  extern unsigned GC_alloc_count;
}

//#include "symbol_table.hpp"

void parse_maps(ast::Environ & env) {
  SourceFile * code = new_source_file(SOURCE_PREFIX "grammer.ins");
  code->internal = true;
  try {
    SourceStr str(code);
    parse_prod("S_SPACING", str, ParseInfo(env.peg), &env);
    while (!str.empty()) {
      const Syntax * p = parse_prod("SEXP", str, ParseInfo(env.peg), &env);
      parse_prod("S_SPACING", str, ParseInfo(env.peg), &env); 
      read_macro(p, env);
    }
  } catch (Error * err) {
    fprintf(stderr, "%s\n", err->message().c_str());
    exit(1);
  }
}

int main(int argc, const char *argv[])
{
  //GC_disable();
  //GC_free_space_divisor = 32;

  assert(setvbuf(stdin, 0, _IOLBF, 0) == 0); 
  assert(setvbuf(stdout, 0, _IOLBF, 0) == 0);
  parse_exp_->init();
  PEG * peg = NULL;
  try {
    peg = parse_peg(SOURCE_PREFIX "grammer.in");
  } catch (Error * err) {
    fprintf(stderr, "%s\n", err->message().c_str());
    exit(1);
  }
  ast::Environ env(ast::TOPLEVEL, peg);
  SourceFile * code = NULL;
  try {
    unsigned offset = 1;
    bool debug_mode = false;
    bool zls_mode = false;
    bool c_mode = false;
    bool cpp_mode = false;
    bool gcc_abi = false;
    bool for_ct = false;
    bool load_prelude = true;
    bool pp_mode = false;
    if (argc > offset && strcmp(argv[offset], "-d") == 0) {
      debug_mode = true;
      offset++;
    } 
    if (argc > offset && strcmp(argv[offset], "-s") == 0) {
      zls_mode = true;
      offset++;
    }
    if (argc > offset && strcmp(argv[offset], "-C") == 0) {
      for_ct = true;
      offset++;
    }
    if (argc > offset && strcmp(argv[offset], "-xc") == 0) {
      c_mode = true;
      offset++;
    }
    if (argc > offset && strcmp(argv[offset], "-xc++") == 0) {
      cpp_mode = true;
      offset++;
    }
    if (argc > offset && strcmp(argv[offset], "-xg++") == 0) {
      cpp_mode = true;
      gcc_abi = true;
      offset++;
    }
    if (argc > offset && strcmp(argv[offset], "-P") == 0) {
      load_prelude = false;
      offset++;
    }
    if (argc > offset && strcmp(argv[offset], "-pp") == 0) {
      pp_mode = true;
      offset++;
    }
    String base_name;
    String output_fn;
    if (argc > offset) {
      code = new_source_file(argv[offset], NULL, pp_mode);
      const char * dot = strrchr(argv[offset], '.');
      if (!dot) {
          base_name = argv[offset];
          output_fn = base_name;
      } else {
        StringBuf buf;
        buf.append(argv[offset], dot);
        base_name = buf.freeze(); // will also reset buf
        if (strcmp(dot, ".zls") == 0)
          buf.append(argv[offset]);
        else
          buf.append(argv[offset], dot);
        buf.append(".zls");
        output_fn = buf.freeze();
        if (strcmp(dot, ".cpp") == 0)
          cpp_mode = true;
      }
    } else {
        code = new_source_file(STDIN_FILENO);
        output_fn = "a.out.zls";
    }
    //const Syntax * res = parse_str("TEST", SourceStr(code->entity(), code->begin(), code->end()));
    //res->print();
    //printf("\n");
    //exit(0);
    //printf("%d\n%s", ast::MACRO_PRELUDE_END - ast::MACRO_PRELUDE, ast::MACRO_PRELUDE);
    if (zls_mode) {
      printf("ZLS MODE\n");
      env.mangle = false;
      ast::parse_stmts_raw(SourceStr(code), env);
    } else {
      parse_maps(env);
      SourceFile * prelude = new_source_file(SOURCE_PREFIX "prelude.zlh");
      prelude->internal = true;
      ast::parse_stmts(SourceStr(prelude), env);
      if (debug_mode && load_prelude) {
        SourceFile * prelude_body = new_source_file(SOURCE_PREFIX "prelude.zl");
        ast::parse_stmts(SourceStr(prelude_body), env);
        //SourceFile * class_body = new_source_file(SOURCE_PREFIX "class.zl");
        //parse_stmts(parse_str("SLIST", SourceStr(class_body, class_body->begin(), class_body->end())), env);
      } else if (load_prelude) {
        load_macro_lib(SOURCE_PREFIX "prelude-fct.so", env);
      }
      if (load_prelude && !debug_mode /* debug mode doesn't work with new abi stuff yet */ ) {
        SourceFile * prelude_extra = new_source_file(SOURCE_PREFIX "prelude-extra.zlh");
        ast::parse_stmts(SourceStr(prelude_extra), env);
      }
      if (cpp_mode) {
        SourceFile * prelude_cpp = new_source_file(SOURCE_PREFIX "prelude-c++.zlh");
        ast::parse_stmts(SourceStr(prelude_cpp), env);
      }
      //if (load_prelude && !for_ct)
        //ast::import_file(SOURCE_PREFIX "test/class-new_abi.zl", env);
        //ast::include_file(SOURCE_PREFIX "test/class-this_reg.zlh", env);
      if (c_mode)
        env.mangle = false;
      if (gcc_abi) {
        ast::parse_stmts_wrap_abi(SourceStr(code), "gcc", env);
      } else {
        ast::parse_stmts(SourceStr(code), env);
      }
    }
    ast::CompileWriter out;
    out.open(output_fn, "w");
    if (for_ct) 
      out.for_macro_sep_c = new ast::CompileWriter::ForMacroSepC;
    //printf("FORCING COLLECTION\n");
    //GC_gcollect();
    //GC_dump();
    ast::compile(env.top_level_symbols, out);
    //ast::CompileWriter out2(ast::CompileWriter::ZLE);
    //out2.open("a.out.zle", "w");
    //ast::compile(env.top_level_symbols, out2);
    //AST::ExecEnviron env;
    //ast->eval(env);
    if (for_ct) {
      out.close();
      StringBuf buf;
      buf.printf("zls -O -g -fexceptions -shared -fpic -o %s-fct.so %s", ~base_name, ~output_fn);
      String line = buf.freeze();
      printf("%s\n", ~line);
      int res = system(~line);
      if (res == -1) {
        perror("system(\"zls ...\")");
        exit(2);
      } else if (res != 0) {
        exit(1);
      }
    }
    out.for_macro_sep_c = NULL;

    //fprintf(stderr, "---------------------------------------------\n");
    //GC_generate_random_backtrace();
    //fprintf(stderr, "---------------------------------------------\n");
    //GC_generate_random_backtrace();
    //fprintf(stderr, "---------------------------------------------\n");
    //GC_generate_random_backtrace();
    //fprintf(stderr, "---------------------------------------------\n");
    //GC_generate_random_backtrace();
    //fprintf(stderr, "---------------------------------------------\n");
    //GC_generate_random_backtrace();
    //fprintf(stderr, "---------------------------------------------\n");

  } catch (Error * err) {
    //if (!err->source)
    //  err->source = code->entity();
    fprintf(stderr, "%s\n", err->message().c_str());
    exit(2);
  }

  //sleep(600);
}

// int main(int argc, const char *argv[])
// {
//   printf("%u %u %u\n", sizeof(Syntax), sizeof(Syntax::D), sizeof(Syntax::Data));
//   printf("XXX %u\n", sizeof(SymbolName));
//   printf("XXX %u\n", sizeof(SourceStr));
//   Syntax syn;

//   int res = main2(argc,argv);

//   printf("FORCING COLLECTION 2\n");
//   GC_gcollect();

// //   fprintf(stderr, "---------------------------------------------\n");
// //   GC_generate_random_backtrace();
// //   fprintf(stderr, "---------------------------------------------\n");
// //   GC_generate_random_backtrace();
// //   fprintf(stderr, "---------------------------------------------\n");
// //   GC_generate_random_backtrace();
// //   fprintf(stderr, "---------------------------------------------\n");
// //   GC_generate_random_backtrace();
// //   fprintf(stderr, "---------------------------------------------\n");
// //   GC_generate_random_backtrace();
// //   fprintf(stderr, "---------------------------------------------\n");


//   return res;
// }
 

