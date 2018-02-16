#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "token.hh"
#include "yuck.hh"
#include "module.hh"
#include "program.hh"
#include "expr.hh"
#include "error.hh"
#include "compiler.hh"
#include "rule.hh"
#include <lcdf/clp.h>
#include <cstring>
#include <cctype>

#define DEBUG_NAMESPACE_OPT	300
#define DEBUG_RULESET_OPT	301
#define DEBUG_TARGET_OPT	302
#define DEBUG_LOCATION_OPT	303
#define OUTPUT_OPT		304
#define HEADER_OPT		305
#define DEBUG_NODE_OPT		306
#define OPTIMIZE_OPT		307

Clp_Option options[] = {
    { "dn", 0, DEBUG_NAMESPACE_OPT, Clp_ArgString, Clp_Optional },
    { "dr", 0, DEBUG_RULESET_OPT, Clp_ArgString, Clp_Optional },
    { "dt", 0, DEBUG_TARGET_OPT, Clp_ArgString, Clp_Optional },
    { "dl", 0, DEBUG_LOCATION_OPT, Clp_ArgString, Clp_Optional },
    { "do", 0, DEBUG_NODE_OPT, Clp_ArgString, Clp_Optional },
    { "output", 'o', OUTPUT_OPT, Clp_ArgString, 0 },
    { "optimize", 'O', OPTIMIZE_OPT, Clp_ArgUnsigned, Clp_Optional },
    { "defines", 'd', HEADER_OPT, 0, Clp_Negate },
    { "header", 0, HEADER_OPT, 0, Clp_Negate },
};


static char *
include_protector_symbol(const char *out_name)
{
    if (!out_name) out_name = "prolacout.h";
    char *buf = new char[strlen(out_name) + 12];
    for (int i = 0; out_name[i]; i++)
	if (isalpha(out_name[i]))
	    buf[i] = toupper(out_name[i]);
	else
	    buf[i] = '_';
    strcpy(buf + strlen(out_name), "_INCLUDED");
    return buf;
}

static void
gen_prolac_defines(Writer &out)
{
  out << "/* Prolac #defines */\n\
typedef unsigned long seqint;\n\
typedef int bool;\n\
#define true 1\n\
#define false 0\n\
#define SEQ_LT(x,y)	((int)((x)-(y)) < 0)\n\
#define SEQ_LEQ(x,y)	((int)((x)-(y)) <= 0)\n\
#define SEQ_GT(x,y)	((int)((x)-(y)) > 0)\n\
#define SEQ_GEQ(x,y)	((int)((x)-(y)) >= 0)\n\
\n";
}

int
main(int argc, char **argv)
{
    Clp_Parser *clp = Clp_NewParser(argc, argv, sizeof(options) / sizeof(options[0]), options);
    Clp_SetOptionChar(clp, '-', Clp_Long | Clp_Short);
  
    HashMap<PermString, int> debug_map(0);
    int all_debug = 0;
  
    PermString filename;
    PermString out_name;
    bool make_header = true;
    int max_inline = inlinePath;
  
    while (1) {
	int opt = Clp_Next(clp);
	int debug_type = 0;
	switch (opt) {
	    
	  case DEBUG_NAMESPACE_OPT:
	    debug_type = dtNamespace;
	    goto debug_option;
      
	  case DEBUG_RULESET_OPT:
	    debug_type = dtRuleset;
	    goto debug_option;
      
	  case DEBUG_TARGET_OPT:
	    debug_type = dtTarget;
	    goto debug_option;
      
	  case DEBUG_LOCATION_OPT:
	    debug_type = dtLocation;
	    goto debug_option;
      
	  case DEBUG_NODE_OPT:
	    debug_type = dtNode;
	    goto debug_option;
      
	  debug_option:
	    if (clp->have_arg)
		debug_map.find_force(clp->arg) |= debug_type;
	    else
		all_debug |= debug_type;
	    break;
      
	  case OUTPUT_OPT:
	    out_name = clp->arg;
	    break;

	  case OPTIMIZE_OPT:
	    if (clp->have_arg) {
		if (clp->val.u < 0 || clp->val.u > inlinePath)
		    error(Landmark(), "-O value must be between 0 and 2");
		else
		    max_inline = clp->val.u;
	    } else
		max_inline = inlinePath;
	    break;
      
	  case HEADER_OPT:
	    make_header = !clp->negated;
	    break;

	  case Clp_NotOption:
	    if (filename)
		error(Landmark(), "2 input files (using 2d)");
	    filename = clp->arg;
	    break;
      
	  case Clp_Done:
	    goto done;
      
	  default:
	    break;
	    
	}
    }
  
  done:
  
    if (!out_name && filename && filename != "-") {
	const char *infile = filename.c_str();
	const char *slash = strrchr(infile, '/');
	if (slash)
	    infile = slash + 1;
	if (strlen(infile) > 3
	    && strcmp(infile + strlen(infile) - 3, ".pc") == 0)
	    out_name = permprintf("%*s.c", strlen(infile) - 3, infile);
	else if (strlen(infile) > 4
		 && strcmp(infile + strlen(infile) - 4, ".pci") == 0)
	    out_name = permprintf("%*s.c", strlen(infile) - 4, infile);
	else
	    out_name = permprintf("%s.c", infile);
    }
  
    FILE *f;
    if (!filename || filename == "-") {
	f = stdin;
	filename = "<stdin>";
    } else {
	f = fopen(filename.c_str(), "r");
	if (!f)
	    return 1;
    }
  
    FILE *out_c;
    if (!out_name || out_name == "-")
	out_c = stdout;
    else {
	out_c = fopen(out_name.c_str(), "w");
	if (!out_c)
	    return 1;
    }

    FILE *out_structs = 0;
    PermString out_structs_name;
    if (make_header) {
	if (!out_name || out_name == "-") {
	    out_structs_name = "<stdout>";
	    out_structs = stdout;
	} else {
	    if (out_name.length() > 2
		&& strcmp(out_name.c_str() + out_name.length() - 2, ".c") == 0)
		out_structs_name = permprintf("%*p.h", out_name.length() - 2, out_name.capsule());
	    else
		out_structs_name = permprintf("%p.h", out_name.capsule());
	    out_structs = fopen(out_structs_name.c_str(), "w");
	}
	if (!out_structs)
	    return 1;
    } else
	out_structs = fopen("/dev/null", "w");
  
    Tokenizer tize(f, filename);
  
    Program prog;
    Yuck y(&tize, &prog);
  
    while (y.ydefinition())
	;
    if (!feof(f)) {
	Token t = y.lex();
	error(t, "unexpected token `%s' ends processing", t.print_string().c_str());
    }
    
    prog.resolve_names();
    prog.debug_print(debug_map, all_debug);
    prog.analyze_exports();
    prog.resolve_code();

    const char *include_protector = include_protector_symbol(out_name.c_str());
    if (!out_name)
	out_name = "<stdout>";
  
    Writer wout_c(out_c);
    Writer wout_structs(out_structs);
    Compiler compiler(wout_c, wout_structs, max_inline);
  
    if (make_header) {
	wout_structs << "/* Generated by the Prolac compiler */\n"
		     << "#ifndef " << include_protector
		     << "\n#define " << include_protector << "\n";
	gen_prolac_defines(wout_structs);
	prog.compile_structs(wout_structs);
	wout_structs << "/* prototypes for methods used by exported methods */\n";
    }
    wout_c["filename"] = (void *)out_name.c_str();
    if (make_header)
	wout_c << "#include \"" << out_structs_name << "\"\n";
    wout_c << "#include <assert.h>\n";
  
    prog.compile_exports(&compiler, debug_map, all_debug);

    if (make_header)
	wout_structs << "#endif /* " << include_protector << " */\n";
    if (num_errors)
	return 1;
    else
	return 0;
}
