#ifndef TYPE__HPP
#define TYPE__HPP

#include <typeinfo>
#include <stdint.h>

#include "util.hpp"
#include "symbol_table.hpp"
#include "string_buf.hpp"
#include "syntax.hpp"

namespace ast {

  // NOTE: In order to avoid expensive compressions between types each
  // type object must only be allocated once, this way a cheap pointer
  // comparison can be used EXCEPT for Tuple which are "special" since
  // they are used as function parameters and have lots of special 
  // properties

  class PrintInst;
  class TypeSymbol;
  class TypeInst;
  typedef TypeInst Type;
  class Tuple;
  struct Environ;

  struct BasicVar;
  typedef BasicVar VarSymbol;

  struct Exp;

  //struct ExpectedType {
  //  Vector<TypeCategory *> expected;
  //  Type * got;
  //};

  struct TypeCategory {
    String name;
    TypeCategory * parent0;
    TypeCategory * parent1;
    TypeCategory(String n, TypeCategory * p0, TypeCategory * p1 = 0) : name(n), parent0(p0), parent1(p1) {}
  };

  extern TypeCategory * const ANY_C;
  extern TypeCategory * const SCALAR_C;
  //extern TypeCategory * const BOOL_C;
  extern TypeCategory * const NUMERIC_C;
  extern TypeCategory * const INT_C;
  extern TypeCategory * const SIGNED_C;
  extern TypeCategory * const UNSIGNED_C;
  extern TypeCategory * const FLOAT_C;
  extern TypeCategory * const POINTER_C;
  extern TypeCategory * const ARRAY_C;
  extern TypeCategory * const FUN_C;
  extern TypeCategory * const UNKNOWN_C;
  extern TypeCategory * const ZERO_C;
  extern TypeCategory * const USER_C;

  struct TypeParm : public gc {
    enum What {NONE, TYPE, INT, TUPLE, EXP, DOTS, UNKNOWN};
    What what;
    union {
      const Type * as_type;
      int          as_int;
      Exp        * as_exp;
    };
    SymbolKey name;
    bool is_type() {return what == TYPE || what == TUPLE;}
    explicit TypeParm() : what(NONE) {}
    explicit TypeParm(const Type * t, SymbolKey n = SymbolKey()) : what(TYPE), as_type(t), name(n) {}
    explicit TypeParm(What w, const Type * t, SymbolKey n = SymbolKey()) : what(w), as_type(t), name(n) {}
    explicit TypeParm(int i, SymbolKey n = SymbolKey()) : what(INT), as_int(i), name(n) {}
    explicit TypeParm(Exp * exp, SymbolKey n = SymbolKey()) : what(EXP), as_exp(exp), name(n) {}
    void to_string(const PrintInst &, StringBuf & buf) const;
    static TypeParm dots() {return TypeParm(DOTS);}
  private:
    explicit TypeParm(What w) : what(w) {}
  };

  struct TypeSymbolTable {
    Environ * env;
    TypeSymbolTable(Environ * s) : env(s) {}
    inline TypeSymbol * find(const SymbolKey & k); // defined in environ.hpp
    inline TypeSymbol * find(const Syntax * p, const InnerNS * ns);
    Type * inst(SymbolKey n, Vector<TypeParm> &);
    Type * inst(const Syntax * n, const InnerNS * ns, Vector<TypeParm> &);
    Type * inst(SymbolKey n) {
      Vector<TypeParm> dummy;
      return inst(n, dummy);
    }
    Type * inst(const Syntax * n, const InnerNS * ns = DEFAULT_NS) {
      Vector<TypeParm> dummy;
      return inst(n, ns, dummy);
    }
    Type * inst(SymbolKey n, const Type * t) {
      Vector<TypeParm> parms;
      parms.push_back(TypeParm(t));
      return inst(n, parms);
    }
    Type * inst(SymbolKey n, const TypeParm & p1) {
      Vector<TypeParm> parms;
      parms.push_back(p1);
      return inst(n, parms);
    }
    Type * inst(SymbolKey n, const TypeParm & p1, const TypeParm & p2) {
      Vector<TypeParm> parms;
      parms.push_back(p1);
      parms.push_back(p2);
      return inst(n, parms);
    }
    Type * ct_const(const Type * t);
    inline SymbolNode * add(const SymbolKey & k, TypeSymbol * t);
    inline SymbolNode * add_internal(const SymbolKey & k, TypeSymbol * t);
    inline SymbolNode * add_alias(const SymbolKey & k, TypeSymbol * t);
  };

  static inline bool operator==(TypeParm lhs, TypeParm rhs) {
    if (lhs.what != rhs.what) return false;
    switch (lhs.what) {
    case TypeParm::NONE:
      return true;
    case TypeParm::TYPE:
    case TypeParm::TUPLE:
      return lhs.as_type == rhs.as_type && lhs.name == rhs.name;
    case TypeParm::INT:
      return lhs.as_int == rhs.as_int;
    case TypeParm::EXP:
      return lhs.as_exp == rhs.as_exp;
    case TypeParm::DOTS:
      return true;
    case TypeParm::UNKNOWN:
      return true;
    }
    abort(); // this should't happen
  }
  
  static inline bool operator!=(TypeParm lhs, TypeParm rhs) {
    return !(lhs == rhs);
  }

  class PrintInst {
  public:
    virtual void to_string(const TypeInst &, StringBuf & buf) const = 0;
    // declaration is needed to handle C types correctly, perhaps this is not 
    // the best place for it
    String to_string(const TypeInst & t) const {
      StringBuf buf;
      to_string(t, buf);
      return buf.freeze();
    }
    virtual void declaration(String var, const TypeInst &, StringBuf & buf) const = 0;
    virtual ~PrintInst() {}
  };

  extern PrintInst const * const generic_print_inst;
  extern PrintInst const * const c_print_inst;
  extern PrintInst const * const zl_print_inst;
  extern PrintInst const * const zls_print_inst;
  extern PrintInst const * const zls_kill_const_print_inst;
  extern PrintInst const * const zle_print_inst;
  extern PrintInst const * const mangle_print_inst;

  struct TypeConv {
    // lower rank is better
    static const unsigned SubRankBits = 4;
    static const unsigned RankMask = ~((1 << SubRankBits) - 1);
    enum {Fail = 0, 
          ExactMatch = 1 << SubRankBits, 
          Promotion  = 2 << SubRankBits, 
          Conversion = 3 << SubRankBits, 
          User       = 4 << SubRankBits, 
          Ellipsis   = 5 << SubRankBits, 
          Other      = 6 << SubRankBits};
    enum {Identity          = ExactMatch | 1, // unused
          LvalueRvalue      = ExactMatch | 2, // unused
          ArrayPointer      = ExactMatch | 3, // unused
          FunctionPointer   = ExactMatch | 4, // unused
          Qualification     = ExactMatch | 5, // unused
          IntProm       = Promotion | 1,
          FloatProm     = Promotion | 2,
          IntConv       = Conversion| 1,
          FloatConv     = Conversion| 2,
          FloatInt      = Conversion| 3,
          PointerConv   = Conversion| 4,
          PointerMember = Conversion| 5,
          BoolConv      = Conversion| 6,
          ToBase        = Conversion| 7};
    unsigned conv;
    unsigned rank() const {return conv & RankMask;}
    Exp * exp;
    operator Exp*() {return exp;}
    TypeConv() {}
    TypeConv(unsigned c, Exp * e)
      : conv(c), exp(e) {}
  };

  class TypeRelation {
  public:
    enum CastType {Implicit, Explicit, Static, Reinterpret, Const, Dynamic};
    enum CheckOnlyType {CheckOnlyFalse, CheckOnly};
    virtual TypeConv resolve_to(Exp * exp, const Type * type, Environ & env, CastType rule = Implicit, CheckOnlyType = CheckOnlyFalse) const = 0;
    enum UnifyRule {Default, Fancy};
    virtual const Type * unify(int rule, const Type *, const Type *, Environ & env) const = 0;
    virtual void resolve_assign(Exp * &, Exp * &, Environ & env) const = 0;
    virtual Exp * to_effective(Exp * exp, Environ & env) const = 0;
    virtual Exp * def_arg_prom(Exp * exp, Environ & env) const = 0;
    const Type * unify(int rule, Exp * &, Exp * &, Environ & env);
    //virtual const Type * promote(Exp * exp) const = 0;
    virtual ~TypeRelation() {}
  };

  class TypeInst;
  
  class TypeSymbol : virtual public TopLevelSymbol {
  public:
    const PrintInst * print_inst;
    TypeSymbol() 
      : TopLevelSymbol(), print_inst(zl_print_inst) {}
    virtual Type * inst(Vector<TypeParm> & d) = 0;
    //virtual Type * inst(const Syntax *) const = 0;
    virtual unsigned required_parms() const = 0;
    virtual TypeParm::What parmt(unsigned i) const = 0; // return TypeParm::NONE if off end
    virtual ~TypeSymbol() {}
  };

  static inline bool category_in(const TypeCategory * x, const TypeCategory * y) {
    if (x == y) return true;
    if (x->parent0 == 0) return false;
    if (category_in(x->parent0, y)) return true;
    if (x->parent1 == 0) return false;
    return category_in(x->parent1, y);
  }

  class TypeInst {
  public:
    typedef ::TypeInfo<TypeInst> TypeInfo;
    TypeCategory * category;
    TypeSymbol * type_symbol;
    virtual unsigned size() const = 0;
    virtual unsigned align() const = 0;
    virtual unsigned storage_size() const {return size();}
    virtual unsigned storage_align() const {return align();}
    virtual String ct_type_name() const {return exact_type->to_string();}
    bool addressable;
    bool read_only;
    bool is_null;
    TypeInst(TypeCategory * c = UNKNOWN_C)
      : category(c), 
        addressable(false), read_only(false), is_null(false)
      , exact_type() {}
    TypeInst(const TypeInst * p) 
      : category(p->category), 
        addressable(p->addressable), read_only(p->read_only), is_null(p->is_null)
      , exact_type() {}
    void to_string(StringBuf & buf, const PrintInst * pi = NULL) const 
      {if (pi == NULL) pi = type_symbol->print_inst; pi->to_string(*this, buf);}
    String to_string(const PrintInst * pi = NULL) const 
      {StringBuf buf; to_string(buf, pi); return buf.freeze();}
    bool is(const TypeCategory * other) const {
      return category_in(category, other);
    }

    const TypeInst * root; // for typedef, the root type, recursively
                           // resolved, this means that if X2 is a
                           // typedef of X than root of "const X2 &"
                           // is "const X &", ie any subtypes are also
                           // resolved to the root type
    const TypeInst * unqualified; // unqualified version of root,
                                  // _not_ recursively resolved
    const TypeInst * effective; // dereferenced version of root

    const TypeInst * exact_type;
    void finalize() // should be called just before it added to the
                    // type symbol cache (or whatever)
    {
      finalize_hook(); 
      root = find_root(); 
      unqualified = root->find_unqualified(); 
      effective = root->find_effective();
      if (!exact_type) exact_type = unqualified->exact_type;
      if (!exact_type) exact_type = this; 
    }
    virtual unsigned num_parms() const = 0;
    virtual TypeParm parm(unsigned i) const = 0;
    virtual const char * tag() const {return NULL;}
    virtual const Type * tprop(const Syntax * p, Environ & env) const {
      if (this == root)
        abort(); /* FIXME: Error Message */
      else
        return root->tprop(p, env);
    }
    virtual SourceStr source_str() const {return SourceStr();}
    virtual Exp * lt_sizeof() const {return NULL;}
    // root_nr: The root type, _not_ recursively resolved.  The result
    //          is guaranteed not to be a typedef, but any inner types 
    //          still be a typedef.
    virtual const TypeInst * root_nr() const {return this;} 
    virtual ~TypeInst() {}
  protected:
    virtual void finalize_hook() {} 
    virtual const TypeInst * find_root() {return this;}
    virtual const TypeInst * find_unqualified() const {return this;}
    virtual const TypeInst * find_effective() const {return this;}
  };

  bool operator==(const TypeInst & lhs, const Vector<TypeParm> & rhs);
  static inline bool operator!=(const TypeInst & lhs, const Vector<TypeParm> & rhs) {
    return !(lhs == rhs);
  }

  bool operator==(const TypeInst & lhs, const TypeInst & rhs);
  static inline bool operator!=(const TypeInst & lhs, const TypeInst & rhs) {
    return !(lhs == rhs);
  }

  //
  //
  //

  class SimpleType : public TypeInst, public TypeSymbol {
  public:
    SimpleType(TypeCategory * c = UNKNOWN_C)  : TypeInst(c) {type_symbol = this;}
    SimpleType(const Type * p) : TypeInst(p) {type_symbol = this;}
    unsigned num_parms() const {return 0;}
    TypeParm parm(unsigned i) const {abort();}
    SimpleType * inst(Vector<TypeParm> & d) {
      assert(d.empty());
      return this;
    }
    unsigned required_parms() const {return 0;}
    TypeParm::What parmt(unsigned i) const {return TypeParm::NONE;}
  };

  //static inline void
  //add_simple_type(TypeSymbolTable sym, SimpleType * t)
  //{
  //  t->finalize();
  //  sym.add(t->name, t);
  //}

  static inline void
  add_internal_type(TypeSymbolTable sym, const char * name, SimpleType * t)
  {
    t->finalize();
    sym.add_internal(name, t);
  }

  static inline SymbolNode *  
  declare_simple_type(TypeSymbolTable sym, SymbolKey name, SimpleType * t, 
                  TopLevelSymbol * where = NULL)
  {
    return sym.add(name, t);
  }

  //static inline SimpleType *  
  static inline SymbolNode *
  add_simple_type(TypeSymbolTable sym, SymbolKey name, SimpleType * t, 
                  TopLevelSymbol * where = NULL)
  {
    t->finalize();
    return declare_simple_type(sym, name, t, where);
  }

  //
  //
  //

  class ParmTypeInst /* Parametrized Type Instance */ : public TypeInst {
  public:
    ParmTypeInst(TypeCategory * c = UNKNOWN_C) : TypeInst(c) {}
    ParmTypeInst(const Type * p) : TypeInst(p) {}
    virtual unsigned num_parms() const = 0;
    virtual TypeParm parm(unsigned i) const = 0;
    const Type * find_root() {
      Vector<TypeParm> parms;
      unsigned sz = num_parms();
      for (unsigned i = 0; i != sz; ++i) {
        TypeParm p = parm(i);
        if (p.is_type())
          parms.push_back(TypeParm(p.what, p.as_type->root));
        else
          parms.push_back(p);
      }
      return type_symbol->inst(parms);
    }
  };

  class InstCache {
  public:
    Vector<ParmTypeInst *> d;
    ParmTypeInst * find(Vector<TypeParm> & to_find) const {
      Vector<ParmTypeInst *>::const_iterator i = d.begin(), e = d.end();
      for (; i != e; ++i) {
        if (**i == to_find) return *i;
      }
      return 0;
    }
    void insert(ParmTypeInst * to_insert) {
      d.push_back(to_insert);
    }
  };

  class ParmTypeSymbol : public TypeSymbol {
  public:
    mutable InstCache inst_cache;
    ParmTypeInst * inst(Vector<TypeParm> & d) {
      ParmTypeInst * r = inst_cache.find(d);
      if (r) return r;
      ParmTypeInst * res = inst_p(d);
      res->type_symbol = this;
      inst_cache.insert(res); // FIXME: hack
      res->finalize();
      return res;
    }
    virtual ParmTypeInst * inst_p(Vector<TypeParm> &) const = 0;
    virtual unsigned required_parms() const = 0;
    virtual TypeParm::What parmt(unsigned i) const = 0; // return TypeParm::NONE if off end
  };

  //
  //
  //

  //
  //
  //

  class ZeroT : public ParmTypeInst {
  public:
    ZeroT(const Type * st) : ParmTypeInst(st), of(st) {
      category = ZERO_C;
      if (of->is(INT_C)) is_null = true;
    }
    const Type * of;
    unsigned size() const {return of->size();}
    unsigned align() const {return of->align();}
    unsigned num_parms() const {return 1;}
    TypeParm parm(unsigned) const {return TypeParm(of);}
  protected:
    const Type * find_root() {
      return of->root;
    }
  };

  class ZeroTypeSymbol : public ParmTypeSymbol {
  public:
    ParmTypeInst * inst_p(Vector<TypeParm> & p) const {
      assert(p.size() == 1);
      assert(p[0].what == TypeParm::TYPE);
      return new ZeroT(p[0].as_type);
    }
    unsigned required_parms() const {return 1;}
    TypeParm::What parmt(unsigned i) const {
      if (i == 0) return TypeParm::TYPE;
      else return TypeParm::NONE;
    }
  };

  //
  //
  //

  class Tuple : public TypeInst {
  public:
    struct Parm {
      const Type * type;
      SymbolKey name;
      mutable const Symbol * sym; // evil I know, but necessary
      Parm() {}
      Parm(const Type * t, SymbolKey n) : type(t), name(n), sym() {}
    };
    typedef Vector<Parm> Parms;
    Parms parms;
    unsigned required;
    int vararg;
    unsigned required_args() const {return required;}
    unsigned max_args() const {return vararg ? UINT_MAX : parms.size();}
    Tuple() : required(), vararg() {}
    virtual unsigned num_parms() const {return parms.size() + vararg;}
    virtual TypeParm parm(unsigned i) const {
      if (i < parms.size())
	return TypeParm(parms[i].type, parms[i].name);
      else if (vararg)
	return TypeParm::dots();
      else
	abort();
    }
    unsigned size() const {return 0;}
    unsigned align() const {return 0;}
  };

  class TupleSymbol : public TypeSymbol {
  public:
    unsigned required_parms() const {return 0;}
    virtual TypeParm::What parmt(unsigned i) const {return TypeParm::TYPE;}
    virtual Type * inst(Vector<TypeParm> & p) {
      Tuple * r = new Tuple();
      for (int i = 0; i != p.size(); ++i) {
	if (p[i].what == TypeParm::DOTS) {
	  r->vararg = true;
	  break;
	} else {
	  assert(p[i].what == TypeParm::TYPE);
	  r->parms.push_back(Tuple::Parm(p[i].as_type, p[i].name));
          ++r->required;
	}
      }
      r->type_symbol = this;
      r->finalize();
      return r;
    }
  };

  //
  //
  //

  Type * parse_type(const Syntax * p, Environ & env);
  void parse_type_parms(parts_iterator i, parts_iterator end, const TypeSymbol *, Vector<TypeParm> & parms, 
                        Environ & env, bool in_tuple = false);

  //
  //
  //

  class Void : public SimpleType {
  public:
    Void() {}
    unsigned size() const {return 0;}
    unsigned align() const {return 1;}
  };

  //
  //
  //

  //template <typename T>
  //static inline T max(T x, T y) {
  //  return x > y ? x : y;
  //}

  //
  //
  //

  class Int : public SimpleType {
  public:
    enum Overflow {UNDEFINED, MODULE, SATURATED, EXCEPTION};
    enum Signed {UNSIGNED = 0, SIGNED = 1};
    Int(int64_t mn, uint64_t mx, Overflow o, unsigned sz)
      : SimpleType(INT_C), size_(sz), min(mn), max(mx), signed_(mn < 0 ? SIGNED : UNSIGNED), overflow(o), rank() {}
    Int(const Int * t) 
      : SimpleType(INT_C), size_(t->size_), min(t->min), max(t->max), signed_(t->signed_), overflow(t->overflow), rank() {exact_type = t;}
    const Int * exact_type_;
    unsigned size_;
    int64_t  min;
    uint64_t max;
    Signed   signed_;
    Overflow overflow;
    int      rank;
    //unsigned calc_size() {
    //  if (min < 0) return ceill(log2l(max)/8);
    //  else return ceill((max(log2l(llabs(min)), log2l(max)) + 1)/8);
    //}
    unsigned size() const {return size_;}
    unsigned align() const {return size_;}
  };
  
  static Int signed_int(unsigned sz) {
    return Int(-(1ll<<(sz*8-1)), (1ull<<(sz*8-1))-1, Int::UNDEFINED, sz);
  }
  static Int unsigned_int(unsigned sz) {
    return Int(0, ((uint64_t)-1)>>(64-sz*8), Int::MODULE, sz);
  }

  static inline Int * new_signed_int(unsigned sz) {
    return new Int(signed_int(sz));
  }
  
  static inline Int * new_unsigned_int(unsigned sz) {
    return new Int(unsigned_int(sz));
  }
 
  class Float : public SimpleType {
  public:
    enum Precision {SINGLE, DOUBLE, LONG} precision;
    unsigned size_;
    unsigned size() const {return size_;}
    unsigned align() const {return size_;}
    Float(Precision p) : SimpleType(FLOAT_C),  size_(p == SINGLE ? 4 : DOUBLE ? 8 : 16) {}
  };

  //
  //
  //

  static const unsigned POINTER_SIZE = sizeof(void *);

  //
  //
  //

  class PointerLike : public ParmTypeInst {
  public:
    PointerLike(TypeCategory * c, const Type * st)
      : ParmTypeInst(c), subtype(st) {}
    const Type * subtype;
    unsigned size() const {return POINTER_SIZE;}
    unsigned align() const {return POINTER_SIZE;}
    const Type * tprop(const Syntax * p, Environ & env) const;
    String ct_type_name() const {return ".ptr";}
  };

  class Pointer : public PointerLike {
  public:
    Pointer(const Type * t) : PointerLike(POINTER_C, t) {}
    unsigned num_parms() const {return 1;}
    TypeParm parm(unsigned) const {return TypeParm(subtype);}
  };

  class PointerSymbol : public ParmTypeSymbol {
  public:
    ParmTypeInst * inst_p(Vector<TypeParm> & p) const {
      assert(p.size() == 1);
      assert(p[0].what == TypeParm::TYPE);
      return new Pointer(p[0].as_type);
    }
    unsigned required_parms() const {return 1;}
    TypeParm::What parmt(unsigned i) const {
      if (i == 0) return TypeParm::TYPE;
      else return TypeParm::NONE;
    }
  };

  //
  //
  //

  class Reference : public ParmTypeInst {
  public:
    Reference(const Type * st)
      : ParmTypeInst(st->category), subtype(st) 
      {addressable = true; /*read_only = st->read_only;*/}
    const Type * subtype;
    const Type * find_effective() const {return subtype;}
    unsigned size() const {return subtype->size();}
    unsigned align() const {return subtype->align();}
    unsigned storage_size() const {return POINTER_SIZE;}
    unsigned storage_align() const {return POINTER_SIZE;}
    unsigned num_parms() const {return 1;}
    TypeParm parm(unsigned) const {return TypeParm(subtype);}
  };

  class ReferenceSymbol : public ParmTypeSymbol {
  public:
    ParmTypeInst * inst_p(Vector<TypeParm> & p) const {
      assert(p.size() == 1);
      assert(p[0].what == TypeParm::TYPE);
      return new Reference(p[0].as_type);
    }
    unsigned required_parms() const {return 1;}
    TypeParm::What parmt(unsigned i) const {
      if (i == 0) return TypeParm::TYPE;
      else return TypeParm::NONE;
    }
  };

  //
  // QualifiedType is a type with one or more qualifiers which restrict there use
  //   when used as an lvalue (const), or control optimizations when the variable is
  //   used.
  //

  class QualifiedType : public ParmTypeInst {
  public:
    enum {CONST = 1, VOLATILE = 2, RESTRICT = 4};
    const Type * subtype;
    unsigned qualifiers; // BIT FIELD
  public:
    QualifiedType(unsigned q, const Type * t) 
      : ParmTypeInst(t->category), subtype(t) {
      qualifiers = q;
      if (q & CONST) read_only = true;
    }
    unsigned num_parms() const {return 2;}
    TypeParm parm(unsigned i) const {return i == 0 ? TypeParm(subtype) : TypeParm(qualifiers);}
    const Type * find_unqualified() const {return subtype;}
    unsigned size() const {return subtype->size();}
    unsigned align() const {return subtype->align();}
  };

  class QualifiedTypeSymbol : public ParmTypeSymbol {
  public:
    ParmTypeInst * inst_p(Vector<TypeParm> & p) const {
      assert(p.size() == 2);
      assert(p[0].what == TypeParm::TYPE);
      assert(p[1].what == TypeParm::INT);
      const Type * subtype = p[0].as_type;
      unsigned qualifiers = p[1].as_int;
      if (const QualifiedType * t = dynamic_cast<const QualifiedType *>(subtype)) {
        qualifiers |= t->qualifiers;
        subtype = t->subtype;
      }
      return new QualifiedType(qualifiers, subtype);
    }
    unsigned required_parms() const {return 2;}
    TypeParm::What parmt(unsigned i) const {
      if (i == 0) return TypeParm::TYPE;
      if (i == 1) return TypeParm::INT;
      else return TypeParm::NONE;
    }
  };

  //
  //
  //
  
  class Array : public PointerLike {
  public:
    Array(const Type * t, unsigned sz) : PointerLike(ARRAY_C, t), length(sz) {}
    unsigned length;
    bool have_size() const {return length != NPOS;}
    unsigned align() const {return subtype->align();}
    unsigned size() const {return have_size() ? subtype->size() * length : NPOS;}
    unsigned num_parms() const {return have_size() ? 2 : 1;}
    TypeParm parm(unsigned i) const {return i == 0 ? TypeParm(subtype) : TypeParm(length);}
  };

  class ArraySymbol : public ParmTypeSymbol {
  public:
    ParmTypeInst * inst_p(Vector<TypeParm> & p) const {
      assert(p.size() >= 1);
      assert(p[0].what == TypeParm::TYPE);
      unsigned sz = NPOS;
      if (p.size() > 1) {
        assert(p[1].what == TypeParm::INT);
        sz = p[1].as_int;
      }
      //assert(p[1].what == TypeParm::EXP);
      return new Array(p[0].as_type, sz);
    }
    unsigned required_parms() const {return 1;}
    TypeParm::What parmt(unsigned i) const {
      switch (i) {
      case 0: return TypeParm::TYPE;
      case 1: return TypeParm::INT;
      //case 1: return TypeParm::EXP;
      default: return TypeParm::NONE;
      }
    }
  };

  //
  //
  //

  class WrapperTypeInst : public TypeInst {
  public:
    WrapperTypeInst(const Type * st, const Syntax * s)
      :TypeInst(st->category), of(st), syn(s) {type_symbol = st->type_symbol;}
    const Type * of;
    const Syntax * syn;
    unsigned size() const {return of->size();}
    unsigned align() const {return of->align();}
    virtual unsigned num_parms() const {return 0;}
    virtual TypeParm parm(unsigned i) const {abort();}
    const TypeInst * root_nr() const {return of->root_nr();}
  protected:
    const Type * find_root() {
      return of->root; 
    }
  };

  class TypeOf : public WrapperTypeInst {
  public:
    inline TypeOf(Exp * a);
    Exp * of_ast;
  };

/*
  class TypeOfSymbol : public TypeSymbol {
  public:
    TypeOfSymbol() {}
    Type * inst(Vector<TypeParm> & d);
    unsigned required_parms() const {return 1;}
    TypeParm::What parmt(unsigned i) const {return i == 0 ? TypeParm::EXP : TypeParm::NONE;}
  };

*/

  //
  //
  //

  class Fun;

  class Function : public ParmTypeInst {
  public:
    Function(const Tuple * p, const Type * r)
      : ParmTypeInst(FUN_C), parms(p), ret(r) {}
    const Tuple * parms;
    const Type * ret;
    unsigned num_parms() const {return 2;}
    TypeParm parm(unsigned i) const 
      {return i == 0 ? TypeParm(TypeParm::TUPLE, parms) : TypeParm(ret);}
//     void to_string_(StringBuf & buf) const {
//       unsigned sz = parms.size();
//       if (sz > 0) {
//         buf += "(";
//         int i = 0;
//         for (;;) {
//           parms[i]->to_string_(buf);
//           ++i;
//           if (i == sz) break;
//           buf += ", ";
//         }
//         buf += ")";
//       } else {
//         buf += "()";
//       }
//       buf += " -> ";
//       ret->to_string_(buf);
//     }
    unsigned size() const {return POINTER_SIZE;}
    unsigned align() const {return POINTER_SIZE;}
    const Type * tprop(const Syntax * p, Environ & env) const;
  };

  class FunctionSymbol : public ParmTypeSymbol {
  public:
    virtual unsigned required_parms() const {return 2;}
    virtual TypeParm::What parmt(unsigned i) const {
      switch (i) {
      case 0: return TypeParm::TUPLE; // parms
      case 1: return TypeParm::TYPE;
      default: return TypeParm::NONE;
      }
    }
    ParmTypeInst * inst_p(Vector<TypeParm> & p) const {
      assert(p.size() == 2);
      assert(p[0].what == TypeParm::TUPLE);
      assert(p[0].as_type);
      assert(p[1].what == TypeParm::TYPE);
      return new Function(static_cast<const Tuple *>(p[0].as_type), p[1].as_type);
    }
    using ParmTypeSymbol::inst;
    Function * inst(TypeSymbolTable types, Fun * f);
  };


  TypeRelation * new_c_type_relation();
  void create_c_types(TypeSymbolTable types);

  static inline bool is_ptr_fun(const Type * t) {
    const Pointer * ptr = dynamic_cast<const Pointer *>(t->root);
    if (!ptr) return false;
    return (ptr->subtype->is(FUN_C));
  }

  extern Type * VOID_T; // FIXME: Hack

}



/*

  pointer: % *
  const:   % const
  array:   %1 [%2]
*/

#endif
