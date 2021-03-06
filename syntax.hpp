#ifndef SYNTAX__HPP
#define SYNTAX__HPP

#include <algorithm>

#include "gc.hpp"
#include "util.hpp"
#include "symbol_table.hpp"
#include "source_str.hpp"

struct Replacements;
struct ParseInfo;

struct PEG;
struct CacheData;
struct ParseInfo {
  const PEG * peg;
  CacheData * cache;
  explicit ParseInfo(const PEG * p = NULL, CacheData * c = NULL)
    : peg(p), cache(c) {}
};

namespace syntax_ns {

  inline void stop() {}

  using std::copy;
  using std::copy_backward;
  using ast::SymbolName;

  // This namespace is to hide lots of internally used symbols, do _not_
  // import. The important symbols are exported at the end of the header file

  struct SyntaxBase;
  typedef const SyntaxBase Syntax;

  struct NoParts;
  struct NumPartsInlined;
  struct PartsInlined;
  struct PartsSeparate;
  struct Expandable;
  typedef Expandable MutableSyntax;
  struct SemiMutable;
  typedef SemiMutable SemiMutableSyntax;
  struct Leaf;
  typedef Leaf SyntaxLeaf;
  struct Reparse;
  typedef Reparse ReparseSyntax;
  struct SynEntity;

  struct PrintFlags {
    unsigned indent;
    PrintFlags(unsigned i = 0) : indent(i) {}
  };

  struct ReparseInfo {
    const Syntax * orig;
    SourceStr str;
    const Replacements * repl;
    ParseInfo parse_info;
    ReparseInfo(const Syntax * o, const SourceStr & s, const Replacements * r, ParseInfo pi)
      : orig(o), str(s), repl(r), parse_info(pi) {}
  };

  // Class inheritance:
  //   SyntaxBase <= <NumPartsInlined> <= NoParts <= Leaf*
  //                                              <= SynEntity*
  //              <= <NoParts> <= Reparse*
  //              <= SemiMutable <= <NumPartsInlined> <= PartsInlined* 
  //                             <= [1] <= PartsSeparate* <= [2] <= Expandable*
  // Notes:
  //   Classes without a * are considered abstract
  //   [1] = ExternParts<...>
  //   [2] = MutableHooks <= PartsExpandable<...> <= MutableExternParts<...> 
  //   <NumPartsInlined> is not a real type
  //   Reparse does not inherit from the NoParts class but logically it has
  //     no parts.  It has special behavior when it is destructured.
  
  // type info layout (16 bits max):
  //   4: num parts
  //   4: num flags
  //   6: type id
  //     2: parts layout
  //       1: <NumPartsInlined>
  //       1: PartsInlined
  //       001 = NoParts
  //       011 = PartsInlined
  //       000 = PartsSeparate
  //     1: Expandable (may add parts or flags) -- imples PartsSeparate
  //     1: Leaf (ie simple) -- imples NoParts
  //     2: extra info -- implies NoParts unless <normal>
  //       00 = <normal> 
  //       01 = SynEntity
  //       10 = Reparse
  //   1: first part simple
  //  [1: mutable -- unused]
  //  [1: w_sourcestr -- unused]

  static const unsigned COPY_THRESHOLD = 8; // See SyntaxBuilder

  static const unsigned INLINE_MAX_PARTS = 15;
  static const unsigned INLINE_MAX_FLAGS = 15;
  
  static const unsigned NUM_PARTS_MASK  = 0xF;
  static const unsigned NUM_FLAGS_SHIFT = 4;
  static const unsigned NUM_FLAGS_MASK  = 0xF << NUM_FLAGS_SHIFT;
 
  static const unsigned TYPE_ID_SHIFT = 8;
  static const unsigned TYPE_ID_MASK  = ((1 << 6) - 1) << TYPE_ID_SHIFT;

  static const unsigned PARTS_LAYOUT_SHIFT = 8;
  static const unsigned PARTS_LAYOUT_MASK = 0x3 << PARTS_LAYOUT_SHIFT;
  static const unsigned   NUM_PARTS_INLINED = 1 << 8;
  static const unsigned   PARTS_INLINED     = 1 << 9;
  static const unsigned PARTS_SEPARATE = 0;
  static const unsigned EXPANDABLE       = 1 << 10;
  static const unsigned SIMPLE           = 1 << 11;
  static const unsigned EXTRA_INFO_SHIFT = 12;
  static const unsigned EXTRA_INFO_MASK = 0x3 << EXTRA_INFO_SHIFT;

  static const unsigned IS_NO_PARTS = NUM_PARTS_INLINED;
  static const unsigned    NO_PARTS_MASK = PARTS_LAYOUT_MASK;
  static const unsigned IS_LEAF = IS_NO_PARTS | SIMPLE;
  static const unsigned IS_SYN_ENTITY  = (0x1 << EXTRA_INFO_SHIFT) | IS_NO_PARTS;
  static const unsigned IS_PARTS_INLINED = NUM_PARTS_INLINED | PARTS_INLINED;
  static const unsigned IS_REPARSE     = (0x2 << EXTRA_INFO_SHIFT) | IS_NO_PARTS;
  static const unsigned IS_PARTS_SEPARATE = PARTS_SEPARATE;
  static const unsigned    PARTS_SEPARATE_MASK = PARTS_LAYOUT_MASK;
  static const unsigned IS_EXPANDABLE = IS_PARTS_SEPARATE | EXPANDABLE;

  static const unsigned FIRST_PART_SIMPLE = 1 << 14;

  static const unsigned LEAF_TI = 1 | IS_LEAF | FIRST_PART_SIMPLE;
  static const unsigned SYN_ENTITY_TI = 1 | IS_SYN_ENTITY;
  static const unsigned REPARSE_TI = 1 | IS_REPARSE;

  static const bool SPECIAL_OK = true;

  typedef Syntax * const * parts_iterator;
  typedef Syntax * const * flags_iterator;

  typedef Syntax * * mutable_parts_iterator;
  typedef Syntax * * mutable_flags_iterator;

  struct MapSourceRes {
    bool stop;
    const SourceInfo * source;
    const Replacements * repl;
    explicit MapSourceRes()
      : stop(true), source(NULL), repl(NULL) {}
    explicit MapSourceRes(const SourceInfo * s, const Replacements * repl = NULL) 
      : stop(false), source(s), repl(repl) {}
  };

  struct SyntaxBase {
    unsigned type_inf; // "type_info" a reserved word
    mutable SourceStr str_;

    void dump_type_info();

    inline bool simple() const;
    inline bool first_part_simple() const;

    bool is_leaf() const {return type_inf & SIMPLE;}
    bool no_parts() const {return (type_inf & NO_PARTS_MASK) == IS_NO_PARTS;}
    bool have_parts() const {return !no_parts();}
    bool num_parts_inlined() const {return type_inf & NUM_PARTS_INLINED;}
    
    // parts_inlined/separate does not test for the most deried type,
    // for that use is_parts_inlined/separate
    bool parts_inlined() const {return type_inf & PARTS_INLINED;}
    bool is_parts_inlined() const {return (type_inf & TYPE_ID_MASK) == IS_PARTS_INLINED;}
    bool parts_separate() const {return (type_inf & PARTS_SEPARATE_MASK) == IS_PARTS_SEPARATE;}
    bool is_parts_separate() const {return (type_inf & TYPE_ID_MASK) == IS_PARTS_SEPARATE;}

    bool expandable() const {return type_inf & EXPANDABLE;}

    bool have_entity() const {return (type_inf & TYPE_ID_MASK) == IS_SYN_ENTITY;}

    bool is_reparse() const {return (type_inf & TYPE_ID_MASK) == IS_REPARSE;}
    inline bool is_reparse(const char *) const;

    inline const NoParts * as_no_parts() const;
    inline const PartsInlined * as_parts_inlined() const;
    inline const PartsSeparate * as_parts_separate() const;
    inline const Expandable * as_expandable() const;
    inline const Leaf * as_leaf() const;
    inline const SynEntity * as_syn_entity() const;
    inline const Reparse * as_reparse() const;
    inline const Reparse * maybe_reparse() const;

    inline PartsInlined * as_parts_inlined();
    inline PartsSeparate * as_parts_separate();
    inline Expandable * as_expandable();
    //inline SemiMutable * as_semi_mutable();

    inline SemiMutable * clone() const;
    inline SyntaxBase * shallow_clone() const;
    template <typename F>
    inline const SyntaxBase * map_source(F & f) const;

    inline unsigned num_parts() const;
    inline unsigned num_flags() const;
    inline bool have_flags() const;
    inline Syntax * first_part() const;
    inline Syntax * part(unsigned i) const;
    inline parts_iterator parts_begin() const;
    inline parts_iterator parts_end() const;
    inline flags_iterator flags_begin() const;
    inline flags_iterator flags_end() const;
    inline const SymbolName & what(bool special_ok = false) const;
    inline const SymbolName * what_if_normal() const;

    template <typename T> inline T * entity() const;

    inline unsigned num_args() const;
    inline Syntax * arg(unsigned i) const;
    inline parts_iterator args_begin() const;
    inline parts_iterator args_end()   const;
    inline Syntax * flag(SymbolName n) const;

    String as_string() const;
    inline const SymbolName & as_symbol_name() const;
    inline operator String () const;
    inline operator const SymbolName & () const;
    inline const char * operator ~ () const;
    inline SymbolName string_if_simple() const;
    inline const SourceStr & str() const;

    // Note: these are only valid for Reparse type
    inline const SymbolName & rwhat() const;
    inline const Syntax * what_part() const;
    inline ReparseInfo inner() const;
    inline ReparseInfo outer() const;

    void print() const;
    void to_string(OStream & o, PrintFlags f = PrintFlags(), SyntaxGather * = 0) const;
    String to_string() const;
    inline bool eq(const char * n) const;
    inline bool eq(const char * n1, const char * n2) const;
    inline bool eq(const char * n1, const char * n2, const char * n3) const;
    inline bool eq(const char * const *, unsigned len) const;
    inline bool ne(const char * n) const;
    inline bool ne(const char * n1, const char * n2) const;
    inline bool ne(const char * n1, const char * n2, const char * n3) const;
    inline bool is_a(const char * n) const;
    inline bool is_a(String n) const;
    inline bool is_a(SymbolName n) const;
    inline bool is_a(const char * n, const char * p) const;
    inline bool operator==(const char * str) const {return is_a(str);}

    void sample_w_loc(OStream & o, unsigned max_len = 20, unsigned syn_len = 0) const;
    String sample_w_loc(unsigned max_len = 20, unsigned syn_lan = 0) const;

    SourceStr get_inner_src(const SourceStr & s = SourceStr()) const;
    void set_src_from_parts() const;

  protected:
    SyntaxBase(unsigned tinf, const SourceStr & s = SourceStr()) 
      : type_inf(tinf), str_(s) {}
  };

  struct SemiMutable : public SyntaxBase {
    inline Syntax * & part(unsigned i);
    inline mutable_parts_iterator parts_begin();
    inline mutable_parts_iterator parts_end();
    inline mutable_flags_iterator flags_begin();
    inline mutable_flags_iterator flags_end();

    inline Syntax * & arg(unsigned i);
    inline mutable_parts_iterator args_begin();
    inline mutable_parts_iterator args_end();
    inline Syntax * & flag(SymbolName n);

    inline void finalize();
  protected:
    SemiMutable(unsigned tinf, const SourceStr & str = SourceStr())
      : SyntaxBase(tinf, str) {}
  };

  template <typename T>
  inline Syntax * find_flag(const T * syn, SymbolName n)
  {
    // FIXME: Handle marks correctly rather than ignoring them
    for (flags_iterator i = syn->flags_begin(), e = syn->flags_end(); i != e; ++i) 
      if ((*i)->what().name == n.name) return *i;
    return NULL;
  }

  struct NoParts : public SyntaxBase {
    mutable Syntax * self; // hack see parts_begin()

    Syntax * first_part() const {return this;}
    Syntax * part(unsigned i) const {return this;}
    parts_iterator parts_begin() const {return &(self = this);}
    parts_iterator parts_end()   const {return &(self = this) + 1;}
    flags_iterator flags_begin() const {return NULL;}
    flags_iterator flags_end()   const {return NULL;}
  protected:
    NoParts(unsigned tinf, const SourceStr & str = SourceStr())
      : SyntaxBase(tinf, str) {}
  };

  struct PartsInlined : public SemiMutable {
    PartsInlined(const SourceStr & str = SourceStr())
      : SemiMutable(IS_PARTS_INLINED, str) {}
    Syntax * parts_[1]; // actual size may be larger than one

    Syntax * first_part() const {return *parts_;}
    Syntax *   part(unsigned i) const {return parts_[i];}
    Syntax * & part(unsigned i)       {return parts_[i];}
    parts_iterator         parts_begin() const {return parts_;}
    mutable_parts_iterator parts_begin()       {return parts_;}
    parts_iterator         parts_end() const {return parts_ + num_parts();}
    mutable_parts_iterator parts_end()       {return parts_ + num_parts();}
    flags_iterator         flags_begin() const {return parts_end();}
    mutable_flags_iterator flags_begin()       {return parts_end();}
    flags_iterator         flags_end() const {return flags_begin() + num_flags();}
    mutable_flags_iterator flags_end()       {return flags_begin() + num_flags();}

    void copy_in(parts_iterator p, parts_iterator pe, flags_iterator f, flags_iterator fe) {
      unsigned parts_sz = pe - p;
      unsigned flags_sz = fe - f;
      copy(p, pe, parts_);
      copy(f, fe, parts_ + parts_sz);
      type_inf |= parts_sz | (flags_sz << NUM_FLAGS_SHIFT);
      finalize();
    }

    inline PartsInlined * clone() const;
    template <typename F>
    const PartsInlined * map_source(F & f) const;

  protected:
    PartsInlined(unsigned tinf, const SourceStr & str = SourceStr())
      : SemiMutable(tinf, str) {}
    // PartsInlined is a variable length structure, thus should not
    // call copy constructor directly unless you know what your doing
    PartsInlined(const PartsInlined & other) : SemiMutable(other) {}
  };

  // 
  static inline PartsInlined * new_parts_inlined(const SourceStr & str, 
                                                 unsigned sz)
  {
    PartsInlined * syn = (PartsInlined *)GC_MALLOC(sizeof(PartsInlined) + (sz - 1)*sizeof(void *));
    new (syn) PartsInlined(str);
    return syn;
  }

  static inline PartsInlined * new_parts_inlined(const SourceStr & str, 
                                                 parts_iterator p, parts_iterator pe, 
                                                 flags_iterator f, flags_iterator fe) 
  {
    PartsInlined * syn = new_parts_inlined(str, (pe - p) + (fe - f));
    syn->copy_in(p, pe, f, fe);
    return syn;
  }

  inline PartsInlined * PartsInlined::clone() const {
    unsigned sz = num_parts() + num_flags();
    PartsInlined * syn = (PartsInlined *)GC_MALLOC(sizeof(PartsInlined) + (sz - 1)*sizeof(void *));
    new (syn) PartsInlined(*this);
    copy(parts_, parts_ + sz, syn->parts_);
    return syn;
  }

  template <typename F>
  inline void map_source_copy_parts(F & f, const Syntax * * dest, const Syntax * const * src, unsigned sz) {
    for (unsigned i = 0; i != sz; ++i)
      dest[i] = src[i]->map_source(f);
  }

  template <typename F>
  const PartsInlined * PartsInlined::map_source(F & f) const {
    MapSourceRes res = f(this);
    if (res.stop) return this;
    unsigned sz = num_parts() + num_flags();
    PartsInlined * syn = (PartsInlined *)GC_MALLOC(sizeof(PartsInlined) + (sz - 1)*sizeof(void *));
    new (syn) PartsInlined(*this);
    syn->str_.source = res.source;
    map_source_copy_parts(f, syn->parts_, parts_, sz);
    return syn;
  }

  struct DummyBase {
    DummyBase(unsigned, const SourceStr &) {}
    void finalize() {}
  };

  // This class is a mixin
  template <typename T>
  struct ExternParts : public T {
    ExternParts(unsigned tinf = 0, const SourceStr & str = SourceStr()) 
      : T(tinf, str), parts_(NULL), parts_end_(NULL), flags_(NULL), flags_end_(NULL) {}

    Syntax * * parts_;
    Syntax * * parts_end_;
    Syntax * * flags_;
    Syntax * * flags_end_;

    void allocate(unsigned sz) {
      parts_ = parts_end_ = (Syntax * *)GC_MALLOC((sz) * sizeof(void *));
      flags_ = flags_end_ = parts_ + sz;
    }

    void copy_in(parts_iterator p, parts_iterator pe, flags_iterator f, flags_iterator fe) {
      allocate((pe - p) + (fe - f));
      parts_end_ = parts_ + (pe - p);
      flags_ = flags_end_ - (fe - f); 
      copy(p, pe, parts_);
      copy(f, fe, flags_);
      this->finalize();
    }

    void direct_copy(const ExternParts & other) {
      allocate(other.flags_end_ - other.parts_);
      parts_end_ = parts_ + other.num_parts();
      flags_     = flags_ - other.num_flags();
      copy(other.parts_, other.parts_end_, parts_);
      copy(other.flags_, other.flags_end_, flags_);
      this->finalize();
    }

    template <typename F>
    void map_source_copy_in(F & f, const ExternParts & other);

    ExternParts(unsigned tinf, const SourceStr & str, 
                Syntax * * p, Syntax * * pe, Syntax * * f, Syntax * * fe)
      : T(tinf, str)
      , parts_(p), parts_end_(pe), flags_(f), flags_end_(fe) {}
    ExternParts(unsigned tinf, const SourceStr & str, unsigned max_sz)
      : T(tinf, str) {allocate(max_sz);}
    ExternParts(Syntax * * p, Syntax * * pe, Syntax * * f, Syntax * * fe)
      : parts_(p), parts_end_(pe), flags_(f), flags_end_(fe) {}
    // it is up to the derived class to make sure the parts are copied correctly
    ExternParts(const ExternParts & other)
      : T(other), parts_(NULL), parts_end_(NULL), flags_(NULL), flags_end_(NULL) {}

    bool empty() const {return parts_ == parts_end_ && flags_ == flags_end_;}
    unsigned num_parts() const {return parts_end_ - parts_;}
    unsigned num_flags() const {return flags_end_ - flags_;}
    unsigned have_flags() const {return flags_end_ != flags_;}
    Syntax * first_part() const {return *parts_;}
    Syntax *   part(unsigned i) const {return parts_[i];}
    Syntax * & part(unsigned i)       {return parts_[i];}
    parts_iterator         parts_begin() const {return parts_;}
    mutable_parts_iterator parts_begin()       {return parts_;}
    parts_iterator         parts_end()   const {return parts_end_;}
    mutable_parts_iterator parts_end()         {return parts_end_;}
    //unsigned num_args()         const {return num_parts() - 1;}
    //Syntax *  arg(unsigned i)   const {return part(i+1);}
    //Syntax * & arg(unsigned i)        {return part(i+1);}
    //parts_iterator args_begin() const {return parts_begin() + 1;}
    //parts_iterator args_begin()       {return parts_begin() + 1;}
    //parts_iterator args_end()   const {return parts_end();}
    //parts_iterator args_end()         {return parts_end();}
    flags_iterator         flags_begin() const {return flags_;}
    mutable_flags_iterator flags_begin()       {return flags_;}
    flags_iterator         flags_end()   const {return flags_end_;}
    mutable_flags_iterator flags_end()         {return flags_end_;}
    
    unsigned alloc_size() const {return flags_end_ - parts_;}
  };

  template <typename T>
  template <typename F>
  void ExternParts<T>::map_source_copy_in(F & f, const ExternParts & other) {
    allocate(other.flags_end_ - other.parts_);
    parts_end_ = parts_ + other.num_parts();
    flags_     = flags_ - other.num_flags();
    map_source_copy_parts(f, parts_, other.parts_, other.num_parts());
    map_source_copy_parts(f, flags_, other.flags_, other.num_flags());
    this->finalize();
  }

  
  // "T" must inherate from Entity, it must also define add_part_hook
  // and add_parts_hook, and insure_space
  template <class T>
  struct MutableExternParts : public T {
    MutableExternParts(unsigned tinf = 0, const SourceStr & str = SourceStr()) : T(tinf, str) {}
    
    using T::parts_;
    using T::parts_end_;
    using T::flags_;
    using T::flags_end_;
    using T::insure_space;

    void truncate_parts(unsigned sz) {
      assert(sz <= this->num_parts());
      parts_end_ = parts_ + sz;
    }
    void truncate_flags(unsigned sz) {
      assert(sz <= this->num_flags());
      flags_ = flags_end_ - sz;
    }

    void invalidate() {
      parts_ = parts_end_ = NULL;
      flags_ = flags_end_ = NULL;
    };

    void clear() {
      parts_end_ = parts_;
      flags_ = flags_end_;
    }

    void add_part(Syntax * p) {
      this->add_part_hook(p);
      insure_space(1);
      *parts_end_ = p;
      ++parts_end_;
    }
    void add_args(Syntax * p) {
      add_part(p);
    }
    void add_parts(parts_iterator i, parts_iterator e) {
      if (i >= e) return;
      this->add_parts_hook(i, e);
      unsigned sz = e - i;
      insure_space(sz);
      copy(i, e, parts_end_);
      parts_end_ += sz;
    }

    void add_flag(Syntax * p) {
      if (find_flag(this, p->what())) return;
      insure_space(1);
      flags_--;
      *flags_ = p;
    }

    void merge_flags(flags_iterator i, flags_iterator e) {
      if (i >= e) return;
      for (; i != e; ++i) {
        add_flag(*i);
      }
    }

    void merge_flags(Syntax * p) {
      merge_flags(p->flags_begin(), p->flags_end());
    }

    void set_flags(flags_iterator i, flags_iterator e) {
      unsigned sz = e - i;
      insure_space((e - i) - (flags_end_ - flags_));
      flags_ = flags_end_ - sz;
      copy(i, e, flags_);
    }

    void set_flags(Syntax * p) {
      set_flags(p->flags_begin(), p->flags_end());
    }

  };

  // "T" must inherate from ???
  template <typename T>
  struct PartsFixed : public T {
    PartsFixed(unsigned tinf = 0, const SourceStr & str = SourceStr()) : T(tinf, str) {}

    void insure_space(unsigned need) {
      unsigned have = this->flags_ - this->parts_end_;
      assert(have >= need);
    }
  };

  template <typename T>
  struct PartsExpandable : public T {
    PartsExpandable(unsigned tinf = 0, const SourceStr & str = SourceStr()) : T(tinf, str) {}
    PartsExpandable(Syntax * * p, Syntax * * pe, Syntax * * f, Syntax * * fe) : T(p, pe, f, fe) {}

    using T::parts_;
    using T::parts_end_;
    using T::flags_;
    using T::flags_end_;

    void insure_space(unsigned need);
  };
  
  template <class T>
  void PartsExpandable<T>::insure_space(unsigned need) {
    unsigned have = flags_ - parts_end_;
    if (have < need) {
      unsigned alloc_sz = flags_end_ - parts_;
      assert(alloc_sz > 0);
      unsigned new_size = alloc_sz * 2;
      while (new_size - alloc_sz + have < need)
        new_size *= 2;
      assert(new_size >= need); // sanity check against overflow
      Syntax * * buf = (Syntax * *)GC_MALLOC(new_size * sizeof(void *));
      //printf("XXX %u+%u=%u %u %u\n", 
      //       this->num_parts(), this->num_flags(), 
      //       this->num_parts() + this->num_flags(), need, new_size);
      unsigned parts_sz = parts_end_ - parts_;
      unsigned flags_sz = flags_end_ - flags_;
      copy(parts_, parts_end_, buf);
      copy(flags_, flags_end_, buf + new_size - flags_sz);
      parts_ = buf;
      parts_end_ = buf + parts_sz;
      flags_ = buf + new_size - flags_sz;
      flags_end_ = buf + new_size;
    }
  }

  struct PartsSeparate : public ExternParts<SemiMutable> {
    typedef ExternParts<SemiMutable> Base;

    PartsSeparate(const SourceStr & str) 
      : Base(IS_PARTS_SEPARATE, str) {}
    PartsSeparate(const SourceStr & str, 
                  Syntax * * p, Syntax * * pe, Syntax * * f, Syntax * * fe)
      : Base(IS_PARTS_SEPARATE, str,p,pe,f,fe) {finalize();}
    PartsSeparate(const SourceStr & str, unsigned max_sz) 
      : Base(IS_PARTS_SEPARATE, str, max_sz) {}
    PartsSeparate(const PartsSeparate & other) : Base(other) {direct_copy(other);}
    PartsSeparate * clone() const {return new PartsSeparate(*this);}
    template <typename F>
    const PartsSeparate * map_source(F & f) const {
      MapSourceRes res = f(this);
      if (res.stop) return this;
      PartsSeparate * syn = new PartsSeparate(SourceStr(res.source, str_.begin, str_.end));
      syn->map_source_copy_in(f, *this);
      return syn;
    }
  protected:
    PartsSeparate(unsigned tinf, const SourceStr & str) 
      : Base(tinf, str) {}
  };
  
  struct MutableHooks : public PartsSeparate {
    MutableHooks(unsigned tinf = 0, const SourceStr & str = SourceStr()) : PartsSeparate(tinf, str) {}
    void add_part_hook(Syntax * p) {
      if (num_parts() == 0 && p->simple())
        type_inf |= FIRST_PART_SIMPLE;
    }
    void add_parts_hook(parts_iterator i, parts_iterator e) {
      if (num_parts() == 0 && i != e && (*i)->simple())
        type_inf |= FIRST_PART_SIMPLE;
    }
  };
  
  struct Expandable : public MutableExternParts<PartsExpandable<MutableHooks> > {
    typedef MutableExternParts<PartsExpandable<MutableHooks> > Base;
    
    Expandable(const SourceStr & str = SourceStr()) : Base(IS_EXPANDABLE, str) {allocate(COPY_THRESHOLD);}

    Expandable * clone() const {return new Expandable(*this);}
    template <typename F>
    const Expandable * map_source(F & f) const {
      MapSourceRes res = f(this);
      if (res.stop) return this;
      return new Expandable(SourceStr(res.source,str_.begin, str_.end), *this, f);
    }
  private:
    template <typename F>
    Expandable(const SourceStr & str, const Expandable & other, F & f) 
      : Base(IS_EXPANDABLE, str) {map_source_copy_in(f, other);}
  };

  struct Leaf : public NoParts {
    SymbolName what_;
    
    explicit Leaf(const char * n) : NoParts(LEAF_TI), what_(n){}
    explicit Leaf(String n) : NoParts(LEAF_TI), what_(n) {}
    explicit Leaf(SymbolName n) : NoParts(LEAF_TI), what_(n) {}
    explicit Leaf(const SourceStr & str) : NoParts(LEAF_TI, str) {}
    Leaf(const char * n, const SourceStr & s) 
      : NoParts(LEAF_TI, s), what_(n){}
    Leaf(String n, const SourceStr & s) 
      : NoParts(LEAF_TI, s), what_(n) {}
    Leaf(SymbolName n, const SourceStr & s) 
      : NoParts(LEAF_TI, s), what_(n) {}
    Leaf(SymbolName n, const SourceStr & s, const char * e)
      : NoParts(LEAF_TI, SourceStr(s.source, s.begin, e)), what_(n) {}
    Leaf(SymbolName n, const SourceStr & s, const char * b, const char * e)
      : NoParts(LEAF_TI, SourceStr(s.source, b, e)), what_(n) {}

    Leaf * clone() const {
      return new Leaf(*this);
    }
    template <typename F>
    const Leaf * map_source(F & f) const {
      MapSourceRes res = f(this);
      if (res.stop || res.source == str_.source)
        return this;
      else
        return new Leaf(what_, SourceStr(res.source, str_.begin, str_.end));
    }
  };

  struct SynEntity : public NoParts {
    static SymbolName WHAT;
    struct Data {
      unsigned type_id;
      void * data;
      Data() : type_id(), data() {}
      bool empty() const {return type_id == 0;}
      template <typename T, bool base> struct Set;
      template <typename T> struct Set<T, false> {
        static void f(Data & ths, T * d) {
          ths.type_id = T::TypeInfo::id;
          ths.data = static_cast<typename T::TypeInfo::type *>(d);
        }
      };
      template <typename T> struct Set<T, true> {
        static void f(Data & ths, T * d) {
          d->set_syntax_data(ths);
        }
      };
      template <typename T> void set(T * d) {
        Set<T, (T::TypeInfo::id & 0xFFu) == 0>::f(*this, d);
      }
      template <typename T> Data(T * d) {set(d);}
      template <typename T> Data(const T * d) {set(const_cast<T *>(d));}
      template <typename T> Data & operator=(T * d) {
        set(d);
        return *this;
      }
      template <typename T> bool is_a() const {
        return type_id == T::TypeInfo::id || (type_id & ~0xFFu) == T::TypeInfo::id;
      }
      template <typename T> operator T * () const {
        if (is_a<T>()) 
          return static_cast<typename T::TypeInfo::type *>(data);
        else 
          return NULL;
      }
    };
    Data d;
    template <typename T> T * entity() const {return d;}
    template <typename T> SynEntity(const T * e) : NoParts(SYN_ENTITY_TI, e->source_str()), d(e) {}
    template <typename T> SynEntity(const SourceStr & s, const T * e) : NoParts(SYN_ENTITY_TI, s), d(e) {}

    SynEntity(const SourceStr & s, const Data & d) : NoParts(SYN_ENTITY_TI, s), d(d) {}

    void desc(OStream & o) const;
    SynEntity * clone() const {return new SynEntity(*this);}
    template <typename F>
    const SynEntity * map_source(F & f) const {
      MapSourceRes res = f(this);
      if (res.stop || res.source == str_.source)
        return this;
      else
        return new SynEntity(SourceStr(res.source, str_.begin, str_.end), d);
    }
  };

  struct Reparse : public SyntaxBase {
    Syntax * what_;
    const Replacements * repl;
    SourceStr outer_;
    SourceStr inner_;
    ParseInfo parse_info;
    String parse_as;
    String origin;
    mutable Syntax * cached_val;
    const SymbolName & rwhat() const {return what_->what();}
    const Syntax * what_part() const {return what_;}
    String desc() const {return rwhat().name;}
    ReparseInfo outer() const {return ReparseInfo(this, outer_, repl, parse_info);}
    ReparseInfo inner() const {return ReparseInfo(this, inner_, repl, parse_info);}
    Reparse(const SourceStr & str, const PEG * peg) 
      : SyntaxBase(REPARSE_TI), what_(), repl(), inner_(str), 
        parse_info(peg), cached_val() {}
    Reparse(const Syntax * p, const Replacements * r, ParseInfo pi, String pa, String ogn, 
            const SourceStr & o, const SourceStr & i, const SourceStr * str = NULL)
      : SyntaxBase(REPARSE_TI, str ? *str : o), what_(p), repl(r), 
        outer_(o), inner_(i), parse_info(pi), parse_as(pa), origin(ogn), cached_val() {}
    Reparse(const Reparse & other) 
      : SyntaxBase(other), what_(other.what_), repl(other.repl), outer_(other.outer_), inner_(other.inner_), 
        parse_info(other.parse_info), parse_as(other.parse_as), origin(other.origin), cached_val() {}
    Reparse * clone() const {return new Reparse(*this);}
    template <typename F>
    const Reparse * map_source(F & f) const {
      MapSourceRes res = f(this, repl);
      if (res.stop) return this;
      SourceStr new_str = str_;
      new_str.source = res.source;
      return new Reparse(what_->map_source(f), res.repl, parse_info, 
                         parse_as, origin, outer_, inner_, &new_str);
    }
    Syntax * instantiate(bool no_throw = false) const {
      if (!cached_val) do_instantiate(no_throw);
      return cached_val;
    }
    Syntax * instantiate_no_throw() const {
      return instantiate(true);
    }
    void do_instantiate(bool no_throw) const;
  };
  
  extern SymbolName UNKNOWN_WHAT;

  inline const NoParts * SyntaxBase::as_no_parts() const {
    assert(!is_reparse());
    return static_cast<const NoParts *>(this);
  }
  inline const PartsInlined * SyntaxBase::as_parts_inlined() const {
    return static_cast<const PartsInlined *>(this);
  }
  inline const PartsSeparate * SyntaxBase::as_parts_separate() const {
    return static_cast<const PartsSeparate *>(this);
  }
  inline const Expandable * SyntaxBase::as_expandable() const {
    return static_cast<const Expandable *>(this);
  }
  inline const Leaf * SyntaxBase::as_leaf() const {
    return static_cast<const Leaf *>(this);
  }
  inline const SynEntity * SyntaxBase::as_syn_entity() const {
    return static_cast<const SynEntity *>(this);
  }
  inline const Reparse * SyntaxBase::as_reparse() const {
    assert(is_reparse());
    return static_cast<const Reparse *>(this);
  }
  inline const Reparse * SyntaxBase::maybe_reparse() const {
    if (is_reparse())
      return static_cast<const Reparse *>(this);
    else
      return NULL;
  }

  inline PartsInlined * SyntaxBase::as_parts_inlined() {
    return static_cast<PartsInlined *>(this);
  }
  inline PartsSeparate * SyntaxBase::as_parts_separate() {
    return static_cast<PartsSeparate *>(this);
  }
  inline Expandable * SyntaxBase::as_expandable() {
    return static_cast<Expandable *>(this);
  }

  inline SemiMutable * SyntaxBase::clone() const {
    if (is_parts_inlined()) return as_parts_inlined()->clone();
    if (expandable()) return as_expandable()->clone();
    if (is_parts_separate()) return as_parts_separate()->clone();
    abort();
  }

  inline SyntaxBase * SyntaxBase::shallow_clone() const {
    if (is_parts_inlined()) return as_parts_inlined()->clone();
    if (expandable()) return as_expandable()->clone();
    if (is_parts_separate()) return as_parts_separate()->clone();
    if (is_leaf()) return as_leaf()->clone();
    if (have_entity()) return as_syn_entity()->clone();
    if (is_reparse()) return as_reparse()->clone();
    abort();
  }

  template <typename F>
  inline const SyntaxBase * SyntaxBase::map_source(F & f) const {
    if (is_parts_inlined()) return as_parts_inlined()->map_source(f);
    if (expandable()) return as_expandable()->map_source(f);
    if (is_parts_separate()) return as_parts_separate()->map_source(f);
    if (is_leaf()) return as_leaf()->map_source(f);
    if (have_entity()) return as_syn_entity()->map_source(f);
    if (is_reparse()) return as_reparse()->map_source(f);
    abort();
  }

  inline bool SyntaxBase::simple() const {
    if (const Reparse * r = maybe_reparse()) {
      const Syntax * p = r->instantiate_no_throw();
      if (p) return p->simple();
      else {/*printf("NOT SIMPLE YET REPARSE %s\n", ~to_string());*/ return false;}
    }
    return type_inf & SIMPLE;
  }

  inline bool SyntaxBase::first_part_simple() const {
    if (const Reparse * r = maybe_reparse()) return r->instantiate()->first_part_simple();
    return type_inf & FIRST_PART_SIMPLE;
  }

  inline unsigned SyntaxBase::num_parts() const {
    if (const Reparse * r = maybe_reparse()) return r->instantiate()->num_parts();
    if (num_parts_inlined()) return type_inf & NUM_PARTS_MASK;
    return as_parts_separate()->num_parts();
  }
  inline unsigned SyntaxBase::num_flags() const {
    if (const Reparse * r = maybe_reparse()) return r->instantiate()->num_flags();
    if (num_parts_inlined()) return (type_inf & NUM_FLAGS_MASK) >> NUM_FLAGS_SHIFT;
    return as_parts_separate()->num_flags();
  }
  inline bool SyntaxBase::have_flags() const {
    if (const Reparse * r = maybe_reparse()) return r->instantiate()->have_flags();
    if (num_parts_inlined()) return type_inf & NUM_FLAGS_MASK;
    return as_parts_separate()->have_flags();
  }
  inline Syntax * SyntaxBase::first_part() const {
    if (const Reparse * r = maybe_reparse()) return r->instantiate()->first_part();
    if (parts_inlined()) return as_parts_inlined()->first_part();
    if (parts_separate()) return as_parts_separate()->first_part();
    return as_no_parts()->first_part();
  }
  inline Syntax * SyntaxBase::part(unsigned i) const {
    if (const Reparse * r = maybe_reparse()) return r->instantiate()->part(i);
    if (parts_inlined()) return as_parts_inlined()->part(i);
    if (parts_separate()) return as_parts_separate()->part(i);
    return as_no_parts()->part(i);
  }
  inline parts_iterator SyntaxBase::parts_begin() const {
    if (const Reparse * r = maybe_reparse()) return r->instantiate()->parts_begin();
    if (parts_inlined()) return as_parts_inlined()->parts_begin();
    if (parts_separate()) return as_parts_separate()->parts_begin();
    return as_no_parts()->parts_begin();
  }
  inline parts_iterator SyntaxBase::parts_end() const {
    if (const Reparse * r = maybe_reparse()) return r->instantiate()->parts_end(); 
    if (parts_inlined()) return as_parts_inlined()->parts_end();
    if (parts_separate()) return as_parts_separate()->parts_end();
    return as_no_parts()->parts_end();
  }
  inline flags_iterator SyntaxBase::flags_begin() const {
    if (const Reparse * r = maybe_reparse()) return r->instantiate()->flags_begin(); 
    if (parts_inlined()) return as_parts_inlined()->flags_begin();
    if (parts_separate()) return as_parts_separate()->flags_begin();
    return as_no_parts()->flags_begin();
  }
  inline flags_iterator SyntaxBase::flags_end() const {
    if (const Reparse * r = maybe_reparse()) return r->instantiate()->flags_end();
    if (parts_inlined()) return as_parts_inlined()->flags_end();
    if (parts_separate()) return as_parts_separate()->flags_end();
    return as_no_parts()->flags_end();
  }

  inline const SymbolName * SyntaxBase::what_if_normal() const {
    if (is_leaf()) return &as_leaf()->what_;
    if (const Reparse * r = maybe_reparse()) {
      if (Syntax * r2 = r->instantiate_no_throw())
        return r2->what_if_normal();
      return NULL;
    } else if (first_part_simple()) {
      if (parts_inlined()) return &as_parts_inlined()->first_part()->as_leaf()->what_;
      else return &as_parts_separate()->first_part()->as_leaf()->what_;
    } else if (have_parts()) {
      return &UNKNOWN_WHAT;
    }
    return NULL;
  }

  inline const SymbolName & SyntaxBase::what(bool special_ok) const {
    const SymbolName * w = what_if_normal();
    if (w) return *w;
    if (special_ok) {
      if (have_entity()) return SynEntity::WHAT;
      if (is_reparse()) return as_reparse()->rwhat();
      return UNKNOWN_WHAT;
    } else {
      fprintf(stderr, "Attempting to take symbol name of special syntax object, which is a %s\n", ~what(SPECIAL_OK).name);
      abort();
    }
  }

  template <typename T> 
  inline T * SyntaxBase::entity() const {
    if (have_entity()) return as_syn_entity()->entity<T>();
    else return NULL;
  }
  
  inline unsigned SyntaxBase::num_args() const {
    return num_parts() - 1;
  }
  inline Syntax * SyntaxBase::arg(unsigned i) const {
    return part(i+1);
  }
  inline parts_iterator SyntaxBase::args_begin() const {
    return parts_begin() + 1;
  }
  inline parts_iterator SyntaxBase::args_end()   const {
    return parts_end();
  }
  inline Syntax * SyntaxBase::flag(SymbolName n) const {
    return find_flag(this, n);
  }

  inline String SyntaxBase::as_string() const {assert(simple()); return what();}
  inline const SymbolName & SyntaxBase::as_symbol_name() const {assert(simple()); return what();}
  inline SyntaxBase::operator String () const {return as_string();}
  inline SyntaxBase::operator const SymbolName & () const {return as_symbol_name();}
  inline const char * SyntaxBase::operator ~ () const {return ~as_string();}
  inline SymbolName SyntaxBase::string_if_simple() const {return simple() ? what() : SymbolName();}

  inline const SourceStr & SyntaxBase::str() const {
    if (have_parts() && str_.empty()) set_src_from_parts(); 
    return str_;
  }

  inline const SymbolName & SyntaxBase::rwhat() const {return as_reparse()->rwhat();}
  inline Syntax * SyntaxBase::what_part() const {return as_reparse()->what_part();}
  inline ReparseInfo SyntaxBase::inner() const {return as_reparse()->inner();}
  inline ReparseInfo SyntaxBase::outer() const {return as_reparse()->outer();}
  
  inline bool SyntaxBase::eq(const char * n) const {return simple() && as_leaf()->what_.name == n;}
  inline bool SyntaxBase::eq(const char * n1, const char * n2) const 
    {return simple() && (as_leaf()->what_.name == n1 || as_leaf()->what_.name == n2);}
  inline bool SyntaxBase::eq(const char * n1, const char * n2, const char * n3) const 
    {return simple() && (as_leaf()->what_.name == n1 || as_leaf()->what_.name == n2 || as_leaf()->what_.name == n3);}
  inline bool SyntaxBase::eq(const char * const * ns, unsigned len) const {
    if (!simple()) return false;
    for (unsigned i = 0; i != len; ++i) if (as_leaf()->what_.name == ns[i]) return true;
    return false;
  }
  inline bool SyntaxBase::ne(const char * n) const {return !eq(n);}
  inline bool SyntaxBase::ne(const char * n1, const char * n2) const {return !eq(n1,n2);}
  inline bool SyntaxBase::ne(const char * n1, const char * n2, const char * n3) const 
    {return !eq(n1,n2,n3);}
  inline bool SyntaxBase::is_a(const char * n) const {return what(SPECIAL_OK).name == n;}
  inline bool SyntaxBase::is_a(String n) const {return what(SPECIAL_OK).name == n;}
  inline bool SyntaxBase::is_a(SymbolName n) const {return what(SPECIAL_OK) == n;}
  inline bool SyntaxBase::is_a(const char * n, const char * p) const {return what().name == n && num_args() > 0 && arg(0)->is_a(p);}

  inline bool SyntaxBase::is_reparse(const char * n) const {
    if ((type_inf & TYPE_ID_MASK) == IS_REPARSE)
      return as_reparse()->rwhat().name == n;
    else
      return false;
  }
  
  //
  //
  //

  inline Syntax * & SemiMutable::part(unsigned i) {
    if (parts_inlined()) return as_parts_inlined()->part(i);
    else return as_parts_separate()->part(i);
  }
  inline mutable_parts_iterator SemiMutable::parts_begin() {
    if (parts_inlined()) return as_parts_inlined()->parts_begin();
    else return as_parts_separate()->parts_begin();
  }
  inline mutable_parts_iterator SemiMutable::parts_end() {
    if (parts_inlined()) return as_parts_inlined()->parts_end();
    else return as_parts_separate()->parts_end();
  }
  inline mutable_flags_iterator SemiMutable::flags_begin() {
    if (parts_inlined()) return as_parts_inlined()->flags_begin();
    else return as_parts_separate()->flags_begin();
  }
  inline mutable_flags_iterator SemiMutable::flags_end() {
    if (parts_inlined()) return as_parts_inlined()->flags_end();
    return as_parts_separate()->flags_end();
  }
  inline Syntax * & SemiMutable::arg(unsigned i) {
    return part(i+1);
  }
  inline mutable_parts_iterator SemiMutable::args_begin() {
    return parts_begin() + 1;
  }
  inline mutable_parts_iterator SemiMutable::args_end() {
    return parts_end();
  }
  inline Syntax * & SemiMutable::flag(SymbolName n) {
    abort(); // FIXME: Write me
  }
  inline void SemiMutable::finalize() {
    if (num_parts() > 0 && part(0)->simple()) type_inf |= FIRST_PART_SIMPLE;
  }

  //
  //
  //

  template <unsigned SIZE>
  struct PartsArray {
    Syntax * d[SIZE];
    typedef Syntax * const * const_iterator;
    typedef Syntax * const * iterator;
    const_iterator begin() const {return d;}
    const_iterator end() const {return d + SIZE;}
    unsigned size() const {return SIZE;}
    bool empty() const {return SIZE == 0;}
    typedef void PartsList;
  };

  template <typename Itr>
  struct PartsRange {
    Itr begin_;
    Itr end_;
    PartsRange(const Itr & b, const Itr & e) : begin_(b), end_(e) {}
    typedef Itr const_iterator;
    typedef Itr iterator;
    const_iterator begin() const {return begin_;}
    const_iterator end() const {return end_;}
    unsigned size() const {return end_ - begin_;}
    bool empty() const {return begin_ != end_;}
    typedef void PartsList;
  };


  static inline PartsArray<0> mk_pt_flg() 
  {
    return PartsArray<0>();
  }

  static inline PartsArray<1> mk_pt_flg(Syntax * a) 
  {
    PartsArray<1> val = {{a}};
    return val;
  }

  static inline PartsArray<2> mk_pt_flg(Syntax * a, Syntax * b) 
  {
    PartsArray<2> val = {{a, b}};
    return val;
  }

  static inline PartsArray<3> mk_pt_flg(Syntax * a, Syntax * b, 
                                      Syntax * c) 
  {
    PartsArray<3> val = {{a, b, c}};
    return val;
  }

  static inline PartsArray<4> mk_pt_flg(Syntax * a, Syntax * b, 
                                        Syntax * c, Syntax * d) 
  {
    PartsArray<4> val = {{a, b, c, d}};
    return val;
  }
  
  static inline PartsRange<parts_iterator> 
  mk_pt_flg(parts_iterator b, parts_iterator e) 
  {
    return PartsRange<parts_iterator>(b,e);
  }

  //static inline PartsRange<flags_iterator> 
  //mk_pt_flg(flags_iterator b, flags_iterator e) 
  //{
  //  return PartsRange<flags_iterator>(b,e);
  //}

#define PARTS mk_pt_flg
#define FLAGS mk_pt_flg

  extern const Leaf SYN_AT;
  extern const Leaf SYN_DOT;
  extern const Leaf SYN_ATB;
  extern const Leaf SYN_ID;

  extern const Syntax * const NO_MATCH;

  static inline const Leaf * new_leaf(const char * n) {

    const Leaf * r = NULL;

    if (n[0] == '@') {
      if (!n[1])
        r = &SYN_AT;
      else if (strcmp(n+1, "{}") == 0)
        r = &SYN_ATB;
    } else if (strcmp(n, ".") == 0) {
      //return new Leaf(n);
      r = &SYN_DOT;
    } else if (strcmp(n, "id") == 0) {
      //return new Leaf(n);
      r = &SYN_ID;
    }
    if (r) {
      assert(r->eq(n));
      return r;
    } else {
      return new Leaf(n);
    }
  }

  static inline const Leaf * new_syntax(const char * n) {return new_leaf(n);}
  static inline const Leaf * new_syntax(String n) {return new Leaf(n);}
  static inline Leaf * new_syntax(SymbolName n) {return new Leaf(n);}

  static inline Leaf * new_syntax(const char * n, const SourceStr & str) {return new Leaf(n, str);}
  static inline Leaf * new_syntax(String n, const SourceStr & str) {return new Leaf(n, str);}

  static inline Leaf * new_syntax(SymbolName n, const SourceStr & str) {return new Leaf(n, str);}
  static inline Leaf * new_syntax(SymbolName n, const SourceStr & s, const char * e) {return new Leaf(n,s,e);}
  static inline Leaf * new_syntax(SymbolName n, const SourceStr & s, const char * b, const char * e) {return new Leaf(n,s,b,e);}

  //static inline Syntax * new_syntax(Syntax & other) {
  //  return new Syntax(other);}
  //static inline Syntax * new_syntax(const SourceStr & s, Syntax & other) {
  //  return new Syntax(s, other);}

  template <typename T>
  inline Syntax * new_syntax(const T * e, typename T::TypeInfo::type * = 0) {
    return new SynEntity(e);}

  template <typename T>
  inline Syntax * new_syntax(const SourceStr & str, const T * e, typename T::TypeInfo::type * = 0) {
    return new SynEntity(str, e);}

  static inline Expandable * new_syntax() {
    return new Expandable();}
  static inline Expandable * new_syntax(const SourceStr & str) {
    return new Expandable(str);}
  
  template <typename T> 
  struct RequireType {
    RequireType(int) {}
  };
  
  template <typename PartsA, typename FlagsA>
  inline SemiMutable * new_syntax(const SourceStr & str,
                                  const PartsA & parts, 
                                  const FlagsA & flags,
                                  RequireType<typename PartsA::PartsList> = 0) 
  {
    if (parts.size() <= INLINE_MAX_PARTS && flags.size() <= INLINE_MAX_FLAGS) {
      return new_parts_inlined(str, parts.begin(), parts.end(), flags.begin(), flags.end());
    } else {
      PartsSeparate * syn = new PartsSeparate(str);
      syn->copy_in(parts.begin(), parts.end(), flags.begin(), flags.end());
      return syn;
    }
  }

  template <typename PartsA, typename FlagsA>
  inline SemiMutable * new_syntax(const PartsA & parts, 
                                  const FlagsA & flags, 
                                  RequireType<typename PartsA::PartsList> = 0) 
  {
    return new_syntax(SourceStr(), parts, flags);
  }

  template <typename PartsA>
  inline SemiMutable * new_syntax(const PartsA & parts, 
                                  RequireType<typename PartsA::PartsList> = 0) 
  {
    return new_syntax(parts, PartsArray<0>());
  }
  template <typename PartsA>
  inline SemiMutable * new_syntax(const SourceStr & str, 
                                  const PartsA & parts,
                                  RequireType<typename PartsA::PartsList> = 0) 
  {
    return new_syntax(str, parts, PartsArray<0>());
  }

  static inline SemiMutable * new_syntax(Syntax * x) {
    return new_syntax(PARTS(x));}
  static inline SemiMutable * new_syntax(Syntax * x, Syntax * y) {
    return new_syntax(PARTS(x, y));}
  static inline SemiMutable * new_syntax(Syntax * x, Syntax * y, Syntax * z) {
    return new_syntax(PARTS(x, y, z));}
  static inline SemiMutable * new_syntax(Syntax * x, Syntax * y, 
                                    Syntax * z ,Syntax * a) {
    return new_syntax(PARTS(x, y, z, a));}

  static inline SemiMutable * new_syntax(const SourceStr & str, Syntax * x) {
    return new_syntax(str, PARTS(x));}
  static inline SemiMutable * new_syntax(const SourceStr & str, 
                                         Syntax * x, Syntax * y) {
    return new_syntax(str, PARTS(x, y));}
  static inline SemiMutable * new_syntax(const SourceStr & str, 
                                         Syntax * x, Syntax * y, Syntax * z) {
    return new_syntax(str, PARTS(x, y, z));}
  static inline SemiMutable * new_syntax(const SourceStr & str, 
                                         Syntax * x, Syntax * y, 
                                         Syntax * z ,Syntax * a) {
    return new_syntax(str, PARTS(x, y, z, a));}
  
  static inline Leaf * new_syntax(const Syntax * o, const ast::Marks * m) 
  {
    assert(o->simple());
    return new_syntax(SymbolName(o->what().name, m), o->str_);
  }
  
  static inline Leaf * new_syntax(const Syntax * o, const ast::Mark * m, const SourceInfo * s)
  {
    assert(o->simple());
    return new_syntax(ast::add_mark(o->what(), m), SourceStr(s, o->str_.begin, o->str_.end));
  }


#define SYN new_syntax

  // SyntaxBuilderDirect can be used when the number of parts and the
  // maxium number of flags is known ahead of time.  It is sligtly
  // more effecent since it directly populates the syntax object

  struct NoOpHooks : public ExternParts<DummyBase> {
    NoOpHooks(unsigned tinf = PARTS_SEPARATE) : ExternParts<DummyBase>(tinf) {}
    NoOpHooks(unsigned tinf, const SourceStr & str) : ExternParts<DummyBase>(tinf, str) {}

    void add_part_hook(Syntax * p) {}
    void add_parts_hook(parts_iterator i, parts_iterator e) {}
  };

  struct SyntaxBuilderDirect : public MutableExternParts<PartsFixed<NoOpHooks> > {

    typedef MutableExternParts<PartsFixed<NoOpHooks> > Base;

    SemiMutable * syn;
    bool inlined;

    void init(const SourceStr & str, unsigned n_parts, unsigned max_flags) {
      if (n_parts <= INLINE_MAX_PARTS && max_flags <= INLINE_MAX_FLAGS) {
        PartsInlined * s = new_parts_inlined(str, n_parts + max_flags);
        parts_ = parts_end_ = s->parts_;
        flags_ = flags_end_ = s->parts_ + n_parts + max_flags;
        inlined = true;
        syn = s;
      } else {
        PartsSeparate * s = new PartsSeparate(str, n_parts + max_flags);
        parts_ = s->parts_;
        parts_end_ = s->parts_end_;
        flags_ = s->flags_;
        flags_end_ = s->flags_end_;
        inlined = false;
        syn = s;
      }
    }
    
    SyntaxBuilderDirect(const SourceStr & str, unsigned n_parts, unsigned max_flags) {
      init(str, n_parts, max_flags);
    }
    SyntaxBuilderDirect(const SyntaxBase & other, unsigned n_parts, unsigned max_flags) {
      init(other.str(), other.num_parts() + n_parts, other.num_flags() + max_flags);
      add_parts(other.parts_begin(), other.parts_end());
      set_flags(other.flags_begin(), other.flags_end());
    }

    SemiMutable * build() {
      if (inlined) {
        unsigned parts_sz = parts_end_ - parts_;
        unsigned flags_sz = flags_end_ - flags_;
        if (parts_end_ != flags_ && flags_ != flags_end_) {
          // need to move some of the flags so they are right after
          // the parts with no gap in between.
          unsigned gap = flags_ - parts_end_;
          Syntax * * from = flags_end_ - gap;
          do {
            *parts_end_ = *from;
            *from = NULL;
            ++parts_end_;
            ++from;
          } while (parts_end_ != flags_);
          flags_end_ = flags_end_ - gap;
        }
        syn->type_inf |= parts_sz | (flags_sz << NUM_FLAGS_SHIFT);
      } else {
        PartsSeparate * psyn = syn->as_parts_separate();
        psyn->parts_ = parts_;
        psyn->parts_end_ = parts_end_;
        psyn->flags_ = flags_;
        psyn->flags_end_ = flags_end_;
      }
      syn->finalize();
      return syn;
    }
  private:
    SyntaxBuilderDirect(const SyntaxBuilderDirect & other);
  };

  template <typename PartsA, typename FlagsA>
  inline SemiMutable * new_syntax(const SourceStr & str,
                                  Syntax * first,
                                  const PartsA & parts, const FlagsA & flags,
                                  RequireType<typename PartsA::PartsList> = 0)
  {
    SyntaxBuilderDirect tmp(str, parts.size() + 1, flags.size());
    tmp.add_part(first);
    tmp.add_parts(parts.begin(), parts.end());
    tmp.set_flags(flags.begin(), flags.end());
    return tmp.build();
  }

  template <typename PartsA>
  inline SemiMutable * new_syntax(const SourceStr & str,
                                  Syntax * first,
                                  const PartsA & parts, 
                                  RequireType<typename PartsA::PartsList> = 0)
  {
    return new_syntax(str, first, parts, FLAGS());
  }

  template <typename PartsA>
  inline SemiMutable * new_syntax(Syntax * first,
                                  const PartsA & parts, 
                                  RequireType<typename PartsA::PartsList> = 0)
  {
    return new_syntax(SourceStr(), first, parts, FLAGS());
  }

  template <typename PartsA, typename FlagsA>
  inline SemiMutable * new_syntax(Syntax * first,
                                  const PartsA & parts, const FlagsA & flags,
                                  RequireType<typename PartsA::PartsList> = 0)
  {
    return new_syntax(SourceStr(), first, parts, flags);
  }

  struct SyntaxBuilderBase : public MutableExternParts<PartsExpandable<NoOpHooks > > {

    typedef MutableExternParts<PartsExpandable<NoOpHooks > > Base;

    typedef ::TypeInfo<SyntaxBuilderBase> TypeInfo;

    bool single_part() const {return num_parts() == 1 && num_flags() == 0;}
    bool empty() const {return num_parts() == 0 && num_flags() == 0;}

    void make_flags_parts() {
      if (num_flags() > 0) {
        add_part(SYN(SYN("@"), PARTS(), FLAGS(flags_, flags_end_)));
        flags_ = flags_end_;
      }
    }

    // after build is called this object should be considered frozen
    Syntax * build(const SourceStr & str = SourceStr()) const {
      if (alloc_size() <= COPY_THRESHOLD) {
        return new_syntax(str, PARTS(parts_, parts_end_), FLAGS(flags_, flags_end_));
      } else {
        // note, this doesn't actually copy anything
        return new PartsSeparate(str, parts_, parts_end_, flags_, flags_end_);
      }
    }

    // after build is called this object should be considered frozen
    Syntax * build(const SourceStr & str, Syntax * syn) {
      if (alloc_size() <= COPY_THRESHOLD) {
        return new_syntax(str, syn, PARTS(parts_, parts_end_), FLAGS(flags_, flags_end_));
      } else {
        if (flags_ - parts_end_ >= 1) {
          copy_backward(parts_, parts_end_, parts_end_ + 1);
          parts_[0] = syn;
          return new PartsSeparate(str, parts_, parts_end_, flags_, flags_end_);
        } else {
          unsigned new_size = alloc_size() + 1;
          Syntax * * buf = (Syntax * *)GC_MALLOC(new_size * sizeof(void *));
          unsigned parts_sz = parts_end_ - parts_;
          unsigned flags_sz = flags_end_ - flags_;
          copy(parts_, parts_end_, buf + 1); // leave space for new item
          copy(flags_, flags_end_, buf + new_size - flags_sz);
          buf[0] = syn;
          return new PartsSeparate(str, 
                                   buf, buf + parts_sz + 1, 
                                   buf + new_size - flags_sz, buf + new_size);
        }
      }
    }

    const Reparse * part_as_reparse() {
      if (!single_part()) return NULL;
      Syntax * p = first_part();
      if (!p->is_reparse()) return NULL;
      return p->as_reparse();
    }
  };

  template <unsigned SZ = COPY_THRESHOLD>
  struct SyntaxBuilderN : public SyntaxBuilderBase {

    typedef SyntaxBuilderBase Base;

    static const unsigned INIT_SIZE = SZ;

    SyntaxBuilderN(Syntax * x = 0) {
      assert(INIT_SIZE <= COPY_THRESHOLD);
      parts_ = parts_end_ = data;
      flags_ = flags_end_ = data + INIT_SIZE;
      if (x) add_part(x);
    }

    // FIXME: This is horribly inefficient and for the most part
    // unnecessary.  Eliminate the need!
    SyntaxBuilderN(const SyntaxBuilderN & other) 
      : Base(other)
      {
        if (other.parts_ == other.data) {
          copy(other.data, other.data + INIT_SIZE, data);
          //parts_ = parts_end_ = data;
          //flags_ = flags_end_ = data + INIT_SIZE;
          parts_ = data;
          parts_end_ = parts_ + other.num_parts();
          flags_end_ = data + INIT_SIZE;
          flags_ = flags_end_ - other.num_flags();
        } else {
          direct_copy(other);
        }
      }

    Syntax * data[INIT_SIZE];
    
    //SyntaxBuilder() : ExternParts(data, data, data+INIT_SIZE, data + INIT_SIZE) {}
    //SyntaxBuilder(Syntax * x) {syn = new Expandable; syn->add_part(x);}
  };

  typedef SyntaxBuilderN<> SyntaxBuilder; 

  template <typename PartsA, typename FlagsA>
  inline SemiMutable * new_syntax(const SyntaxBase & other,
                                  const PartsA & parts, 
                                  const FlagsA & flags,
                                  RequireType<typename PartsA::PartsList> = 0) 
  {
    SyntaxBuilderDirect res(other, parts.size(), flags.size());
    res.add_parts(parts.begin(), parts.end());
    res.merge_flags(flags.begin(), flags.end());
    return res.build();
  }
  
  //
  //
  //
  
}

using syntax_ns::Syntax;
using syntax_ns::SyntaxLeaf;
using syntax_ns::parts_iterator;
using syntax_ns::mutable_parts_iterator;
using syntax_ns::flags_iterator;
using syntax_ns::mutable_flags_iterator;
using syntax_ns::SyntaxBuilderN;
using syntax_ns::SyntaxBuilder;
using syntax_ns::SyntaxBuilderBase;
using syntax_ns::MutableSyntax;
using syntax_ns::SemiMutableSyntax;
using syntax_ns::PrintFlags;
using syntax_ns::SynEntity;
using syntax_ns::ReparseSyntax;
using syntax_ns::ReparseInfo;
using syntax_ns::new_syntax;
using syntax_ns::mk_pt_flg;
using syntax_ns::SPECIAL_OK;
using syntax_ns::MapSourceRes;

static inline Syntax * instantiate(Syntax * syn) {
  if (const ReparseSyntax * r = syn->maybe_reparse())
    return r->instantiate_no_throw();
  else
    return NULL;
}

SourceStr find_reasonable_span(const Syntax * syn);

#endif
