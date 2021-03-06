#ifndef UTIL__HPP
#define UTIL__HPP

#include <assert.h>
#include <stdlib.h>

#include "gc.hpp"
#include "vector.hpp"
#include "parm_string.hpp"
#include "syntax-f.hpp"

#include "hash.hpp"
#include "iostream.hpp"

#undef NPOS
static const unsigned NPOS = UINT_MAX;

//String sample(const char * begin, const char * end, unsigned max_len);

struct StringObj {
  unsigned size;
  char str[];
};

extern const struct StringObj1 EMPTY_STRING_OBJ1;

static const StringObj * const EMPTY_STRING_OBJ =
  reinterpret_cast<const StringObj *>(&EMPTY_STRING_OBJ1);

struct SubStr;

struct String {
private:
  const StringObj * d;
public:
  typedef const char * iterator;
  typedef const char * const_iterator;

  //void dump_if_large() {
  //  if (size() > 256)
  //    printf("LARGE STR (%d): %s\n", size(), ~sample(begin(), end(), 64));
  //}

  void assign(const char * str, unsigned sz) {
    StringObj * d0 = (StringObj *)GC_MALLOC_ATOMIC(sizeof(StringObj) + sz + 1);
    d0->size = sz;
    memmove(d0->str, str, sz);
    d0->str[sz] = '\0';
    d = d0;
    //dump_if_large();
  }
  void assign(const char * str, const char * e) {
    assign(str, e - str);
  }
  inline void assign(const SubStr & str);
  void assign(const char * str) {assign(str, strlen(str));}
  String() : d(EMPTY_STRING_OBJ) {}
  String(const SubStr & str) {assign(str);}
  String(const char * str) {assign(str);}
  String(const StringObj * str) : d(str) {assert(str); /*dump_if_large();*/}
  String(const char * str, const char * e) {assign(str, e);}
  bool defined() const {return d != EMPTY_STRING_OBJ;}
  unsigned size() const {return d->size;}
  bool empty() const {return d->size == 0;}
  const char * begin() const {return d->str;}
  const char * end()   const {return d->str + d->size;}
  const char * data() const {return d->str;}
  const char * pbegin() const {return d->str;}
  const char * pend()   const {return d->str + d->size;}
  char operator[] (unsigned sz) const {return d->str[sz];}
  char operator[] (int sz)      const {return d->str[sz];}
  operator const char * () const {return d->str;}
  const char * c_str() const {return d->str;}
  const char * operator~() const {return d->str;}
  const StringObj * data_obj () const {return d;}
};

inline ParmString::ParmString(String s) 
  : str_(s), size_(s.size()) {}

struct SubStr {
  const char * begin;
  const char * end;
  SubStr() 
    : begin(0), end(0) {}
  SubStr(const char * b, const char * e) : begin(b), end(e) {}
  SubStr(String s) : begin(s.pbegin()), end(s.pend()) {}
  SubStr(String s, unsigned len) : begin(s.pbegin()), end(s.pbegin() + len) {}
  //explicit SubStr(const string & s) : begin(s.c_str()), end(begin + s.size()) {}
  void clear() {begin = end = 0;}
  void assign(const char * b, const char * e) {begin = b; end = e;}
  bool empty() const {return begin == end;}
  unsigned size() const {return end - begin;}
  operator const char * () {return begin;}
  SubStr & operator=(const char * s) {begin = s; return *this;}
  String to_string() {return String(begin,end);}
};

inline void String::assign(const SubStr & str) {assign(str.begin, str.size());}

int cmp(const char * x, unsigned x_sz, const char * y, unsigned y_sz);

#define OP_CMP(T1, T2) \
  static inline bool operator==(T1 x, T2 y) {\
    if (x.size() != y.size()) return false;\
    return memcmp(x, y, x.size()) == 0;\
  }\
  static inline bool operator!=(T1 x, T2 y) {\
    if (x.size() != y.size()) return true;\
    return memcmp(x, y, x.size()) != 0;\
  }\
  static inline int cmp(T1 x, T2 y) {\
    return cmp(x, x.size(), y, y.size()); \
  }\
  static inline bool operator<(T1 x, T2 y) {\
    return cmp(x, x.size(), y, y.size()) < 0;\
  }
OP_CMP(String, String);
OP_CMP(SubStr, SubStr);
OP_CMP(String, SubStr);
OP_CMP(SubStr, String);

#define CSTR_OP_CMP(T1, T2) \
  static inline bool operator==(T1 x, T2 y) {return strcmp(x, y) == 0;}\
  static inline bool operator!=(T1 x, T2 y) {return strcmp(x, y) != 0;}\
  static inline int cmp(T1 x, T2 y) {return strcmp(x,y);} \
  static inline bool operator<(T1 x, T2 y) {return strcmp(x,y) < 0;}\
  
CSTR_OP_CMP(String, const char *);
CSTR_OP_CMP(const char *, String);

static inline bool operator==(SubStr x, const char * y) {
  int res = strncmp(x.begin, y, x.size());
  if (res != 0) return false;
  return y[x.size()] == '\0'; // we know y is as least as long as x
                              // otherwise strncmp will return
                              // non-zero
}
static inline bool operator!=(SubStr x, const char * y) {
  return !operator==(x, y);
}

#endif
