Syntax * foreach (Syntax * syn, Environ * env) {
  Mark * mark = new_mark();
  Match * m = match_f(0, syntax(VAR,WHAT,BODY), syn);
  Syntax * what = match_var(m, syntax WHAT);
  if (!symbol_exists(syntax begin,what,mark,env) || 
      !symbol_exists(syntax end,what,mark,env))
    return error(what,
                 "Container lacks begin or end method.");
  UnmarkedSyntax * repl = syntax {
    typeof(WHAT) & what = WHAT;
    typeof(what.begin()) i = what.begin(), e = what.end();
    for (;i != e; ++i) {typeof(*i) & VAR = *i; BODY;}
  };
  return replace(repl, m, mark);
}
make_macro foreach;

class X {
  int * data;
  size_t sz;
  int * begin() {return data;}
  int * end() {return data + sz;}
};

int main() {
  X x;
  int d[4] = {1, 5, 3, 2};
  x.data = d;
  x.sz = 4;
  foreach(el, x, {printf("%d\n", el);});
  return 0;
}
