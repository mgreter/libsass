#ifndef SASS_AST_DEF_MACROS_H
#define SASS_AST_DEF_MACROS_H

#ifndef MAX_NESTING
// Note that this limit is not an exact science
// it depends on various factors, which some are
// not under our control (compile time or even OS
// dependent settings on the available stack size)
// It should fix most common segfault cases though.
#define MAX_NESTING 512
#endif

// Helper class to switch a flag and revert once we go out of scope
template <class T>
class LocalOption {
  private:
    T* var; // pointer to original variable
    T orig; // copy of the original option
  public:
    LocalOption(const T& var) :
      var(&var), orig(var)
    {
      // this->var = &var;
      // this->orig = var;
    }
    LocalOption(T& var, T orig) :
      var(&var), orig(var)
    {
      *(this->var) = orig;
    }
    void reset()
    {
      *(this->var) = this->orig;
    }
    ~LocalOption() {
      *(this->var) = this->orig;
    }
};

// Helper class to switch a flag and revert once we go out of scope
template <class T>
class LocalOption2 {
private:
  T& var; // reference to reset variable
  T orig; // pointer to original option
public:
  LocalOption2(T& var, T cur) :
    var(var), orig(var)
  {
    this->var = cur;
  }
  void reset()
  {
    this->var = this->orig;
  }
  ~LocalOption2() {
    this->var = this->orig;
  }
};

#define LOCAL_FLAG(name,opt) LocalOption<bool> flag_##name(name, opt)
#define LOCAL_COUNT(name,opt) LocalOption<size_t> cnt_##name(name, opt)
#define LOCAL_STRING(name,opt) LocalOption<sass::string> flag_##name(name, opt)
#define LOCAL_PTR(var,name,opt) LocalOption<var*> flag_##name(name, opt)

#define NESTING_GUARD(name) \
  LocalOption<size_t> cnt_##name(name, name + 1); \
  if (name > MAX_NESTING) throw Exception::RecursionLimitError(scanner.pstate(), {}); \

#define RECURSION_GUARD(name, pstate) \
  LocalOption<size_t> cnt_##name(name, name + 1); \
  if (name > MAX_NESTING) throw Exception::RecursionLimitError(pstate, traces); \

#define ADD_POINTER(type, name)\
protected:\
  type name##_;\
public:\
  type name() const { return name##_; }\
  void name(type name##__) { name##_ = name##__; }\
private:

#define ADD_PROPERTY(type, name)\
protected:\
  type name##_;\
public:\
  const type& name() const { return name##_; }\
  void name(type&& name##__) { name##_ = name##__; }\
  void name(const type& name##__) { name##_ = name##__; }\
private:

#define HASH_PROPERTY(type, name)\
protected:\
  type name##_;\
public:\
  const type& name() const { return name##_; }\
  void name(type&& name##__) { hash_ = 0; name##_ = name##__; }\
  void name(const type& name##__) { hash_ = 0; name##_ = name##__; }\
private:

#define ADD_CONSTREF(type, name) \
protected: \
  type name##_; \
public: \
  const type& name() const { return name##_; } \
  void name(type&& name##__) { name##_ = name##__; } \
  void name(const type& name##__) { name##_ = name##__; } \
private:

#define ADD_REF(type, name) \
protected: \
  type name##_; \
public: \
  type& name() { return name##_; } \
  const type& name() const { return name##_; } \
  void name(type&& name##__) { name##_ = name##__; } \
  void name(const type& name##__) { name##_ = name##__; } \
private:

#define HASH_CONSTREF(type, name) \
protected: \
  type name##_; \
public: \
  const type& name() const { return name##_; } \
  void name(type&& name##__) { hash_ = 0; name##_ = name##__; } \
  void name(const type& name##__) { hash_ = 0; name##_ = name##__; } \
private:

#ifdef DEBUG_SHARED_PTR

#define ATTACH_ABSTRACT_COPY_OPERATIONS(klass) \
  virtual klass* copy(sass::string, size_t) const { \
    throw std::runtime_error("Copy not implemented"); \
  } \
  virtual klass* clone(sass::string, size_t) const { \
    throw std::runtime_error("Clone not implemented"); \
  } \

#define ATTACH_VIRTUAL_COPY_OPERATIONS(klass) \
  klass(const klass* ptr); \
  virtual klass* copy(sass::string, size_t) const override { \
    throw std::runtime_error("Copy not implemented"); \
  } \
  virtual klass* clone(sass::string, size_t) const override { \
    throw std::runtime_error("Clone not implemented"); \
  } \

#define ATTACH_COPY_OPERATIONS(klass) \
  klass(const klass* ptr); \
  virtual klass* copy(sass::string, size_t) const override; \

#define ATTACH_CLONE_OPERATIONS(klass) \
  klass(const klass* ptr); \
  virtual klass* copy(sass::string, size_t) const override; \
  virtual klass* clone(sass::string, size_t) const override; \

#else

#define ATTACH_ABSTRACT_COPY_OPERATIONS(klass) \
  virtual klass* copy() const { \
    throw std::runtime_error("Copy not implemented"); \
  } \
  virtual klass* clone() const { \
    throw std::runtime_error("Clone not implemented"); \
  } \

#define ATTACH_VIRTUAL_COPY_OPERATIONS(klass) \
  klass(const klass* ptr); \
  virtual klass* copy() const override { \
    throw std::runtime_error("Copy not implemented"); \
  } \
  virtual klass* clone() const override { \
    throw std::runtime_error("Clone not implemented"); \
  } \

#define ATTACH_COPY_OPERATIONS(klass) \
  klass(const klass* ptr); \
  virtual klass* copy() const override; \

#define ATTACH_CLONE_OPERATIONS(klass) \
  klass(const klass* ptr); \
  virtual klass* copy() const override; \
  virtual klass* clone() const override; \

#endif

#define ABSTRACT_EQ_OPERATIONS(klass) \
  virtual bool operator==(const klass& rhs) const = 0; \
  virtual bool operator!=(const klass& rhs) const { return !(*this == rhs); }; \

#define OVERRIDE_EQ_OPERATIONS(klass) \
  virtual bool operator==(const klass& rhs) const override; \
  virtual bool operator!=(const klass& rhs) const override { return !(*this == rhs); }; \

#define VIRTUAL_EQ_OPERATIONS(klass) \
  virtual bool operator==(const klass& rhs) const; \
  virtual bool operator!=(const klass& rhs) const { return !(*this == rhs); }; \

#define ATTACH_EQ_OPERATIONS(klass) \
  bool operator==(const klass& rhs) const; \
  bool operator!=(const klass& rhs) const { return !(*this == rhs); }; \

#define VIRTUAL_CMP_OPERATIONS(klass) \
  virtual bool operator<(const klass& rhs) const; \
  virtual bool operator>(const klass& rhs) const { return rhs < *this; }; \
  virtual bool operator<=(const klass& rhs) const { return !(rhs < *this); }; \
  virtual bool operator>=(const klass& rhs) const { return !(*this < rhs); }; \

#define ATTACH_CMP_OPERATIONS(klass) \
  bool operator<(const klass& rhs) const; \
  bool operator>(const klass& rhs) const { return rhs < *this; }; \
  bool operator<=(const klass& rhs) const { return !(rhs < *this); }; \
  bool operator>=(const klass& rhs) const { return !(*this < rhs); }; \

#ifdef DEBUG_SHARED_PTR

  #define IMPLEMENT_COPY_OPERATORS(klass) \
    klass* klass::copy(sass::string file, size_t line) const { \
      klass* cpy = new klass(this); \
      cpy->trace(file, line); \
      return cpy; \
    } \

  #define IMPLEMENT_AST_OPERATORS(klass) \
    klass* klass::copy(sass::string file, size_t line) const { \
      klass* cpy = new klass(this); \
      cpy->trace(file, line); \
      return cpy; \
    } \
    klass* klass::clone(sass::string file, size_t line) const { \
      klass* cpy = copy(file, line); \
      cpy->cloneChildren(); \
      return cpy; \
    } \

#else

  #define IMPLEMENT_COPY_OPERATORS(klass) \
    klass* klass::copy() const { \
      return SASS_MEMORY_NEW3<klass>(this); \
    } \

  #define IMPLEMENT_AST_OPERATORS(klass) \
    klass* klass::copy() const { \
      return SASS_MEMORY_NEW3<klass>(this); \
    } \
    klass* klass::clone() const { \
      klass* cpy = copy(); \
      cpy->cloneChildren(); \
      return cpy; \
    } \

#endif

#endif
