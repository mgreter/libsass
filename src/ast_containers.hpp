#ifndef SASS_AST_CONTAINERS_H
#define SASS_AST_CONTAINERS_H

#include "sass.hpp"
#include "ast_helpers.hpp"

namespace Sass {

  /////////////////////////////////////////////////////////////////////////////
  // Base class/container for AST nodes that should behave like vectors.
  /////////////////////////////////////////////////////////////////////////////
  template <typename V>
  class Vectorized {

  protected:


    typedef SharedImpl<V> T;
    sass::vector<T> elements_;

  protected:
    mutable size_t hash_;
    void reset_hash() { hash_ = 0; }
  public:
    Vectorized(size_t s = 0) : hash_(0)
    {
      elements_.reserve(s);
    }
    Vectorized(const Vectorized<V>* vec) :
      elements_(vec->elements_),
      hash_(0)
    {}
    Vectorized(sass::vector<T> vec) :
      elements_(std::move(vec)),
      hash_(0)
    {}
    virtual ~Vectorized() {};
    size_t length() const { return elements_.size(); }
    bool empty() const { return elements_.empty(); }
    void clear() { return elements_.clear(); }


    T& last() { return elements_.back(); }
    T& first() { return elements_.front(); }
    const T& last() const { return elements_.back(); }
    const T& first() const { return elements_.front(); }

    bool operator== (const Vectorized<V>& rhs) const
    {
      // Abort early if sizes do not match
      if (length() != rhs.length()) return false;
      // Otherwise test each node for object equality in order
      return std::equal(begin(), end(), rhs.begin(), ObjEqualityFn<T>);
    }

    bool operator!= (const Vectorized<V>& rhs) const {
      return !(*this == rhs);
    }

    T& operator[](size_t i) { return elements_[i]; }
    virtual const T& at(size_t i) const { return elements_.at(i); }
    virtual T& at(size_t i) { return elements_.at(i); }
    const T& get(size_t i) const { return elements_[i]; }
    // ToDo: might insert am item (update ordered list)
    const T& operator[](size_t i) const { return elements_[i]; }

    // Implicitly get the sass::vector from our object
    // Makes the Vector directly assignable to sass::vector
    // You are responsible to make a copy if needed
    // Note: since this returns the real object, we can't
    // Note: guarantee that the hash will not get out of sync
    operator sass::vector<T>& () { return elements_; }
    operator const sass::vector<T>& () const { return elements_; }

    // Explicitly request all elements as a real sass::vector
    // You are responsible to make a copy if needed
    // Note: since this returns the real object, we can't
    // Note: guarantee that the hash will not get out of sync
    sass::vector<T>& elements() { return elements_; }
    const sass::vector<T>& elements() const { return elements_; }

    // Insert all items from compatible vector
    void concat(const sass::vector<T>& v)
    {
      if (!v.empty()) reset_hash();
      elements().insert(end(), v.begin(), v.end());
    }

    // Insert all items from compatible vector
    void concat(sass::vector<T>&& v)
    {
      if (!v.empty()) reset_hash();
      std::move(v.begin(), v.end(),
        std::back_inserter(elements_));
      // elements().insert(end(), v.begin(), v.end());
    }


    // Synthetic sugar for pointers
    void concat(const Vectorized<V>* v)
    {
      if (v != nullptr) {
        return concat(*v);
      }
    }

    // Insert one item on the front
    void unshift(const T& element)
    {
      reset_hash();
      elements_.insert(begin(), element);
    }

    // Insert one item on the front
    void unshift(T&& element)
    {
      reset_hash();
      elements_.insert(begin(), std::move(element));
    }

    // Remove and return item on the front
    // ToDo: handle empty vectors
    T shift() {
      reset_hash();
      T first = get(0);
      elements_.erase(begin());
      return first;
    }

    // Insert one item on the back
    // ToDo: rename this to push
    void append(const T& element)
    {
      reset_hash();
      elements_.emplace_back(element);
    }

    // Insert one item on the back
    // ToDo: rename this to push
    void append(T&& element)
    {
      reset_hash();
      elements_.emplace_back(std::move(element));
    }

    // Check if an item already exists
    // Uses underlying object `operator==`
    // E.g. compares the actual objects
    bool contains(const T& el) const {
      for (const T& rhs : elements_) {
        // Test the underlying objects for equality
        // A std::find checks for pointer equality
        if (ObjEqualityFn(el, rhs)) {
          return true;
        }
      }
      return false;
    }

    bool contains(const V* el) const {
      for (const T& rhs : elements_) {
        // Test the underlying objects for equality
        // A std::find checks for pointer equality
        if (PtrObjEqualityFn(el, rhs.ptr())) {
          return true;
        }
      }
      return false;
    }

    // This might be better implemented as `operator=`?
    void elements(const sass::vector<T>& e) {
      reset_hash();
      elements_ = e;
    }

    // This might be better implemented as `operator=`?
    void elements(sass::vector<T>&& e) {
      reset_hash();
      elements_ = std::move(e);
    }

    virtual size_t hash() const
    {
      if (hash_ == 0) {
        for (const T& el : elements_) {
          hash_combine(hash_, el->hash());
        }
      }
      return hash_;
    }

    template <typename P>
    typename sass::vector<T>::iterator insert(P position, const T& val) {
      reset_hash();
      return elements_.insert(position, val);
    }

    template <typename P>
    typename sass::vector<T>::iterator insert(P position, T&& val) {
      reset_hash();
      return elements_.insert(position, std::move(val));
    }

    typename sass::vector<T>::iterator end() { return elements_.end(); }
    typename sass::vector<T>::iterator begin() { return elements_.begin(); }
    typename sass::vector<T>::const_iterator end() const { return elements_.end(); }
    typename sass::vector<T>::const_iterator begin() const { return elements_.begin(); }
    typename sass::vector<T>::iterator erase(typename sass::vector<T>::iterator el) { reset_hash(); return elements_.erase(el); }
    typename sass::vector<T>::const_iterator erase(typename sass::vector<T>::const_iterator el) { reset_hash(); return elements_.erase(el); }

  };


  /////////////////////////////////////////////////////////////////////////////
  // Base class/container for AST nodes that should behave like vectors.
  /////////////////////////////////////////////////////////////////////////////
  template <typename V>
  class VectorizedBase {

  private:


    typedef SharedImpl<V> T;
    sass::vector<T> elements_;

  public:
    VectorizedBase(size_t s = 0)
    {
      elements_.reserve(s);
    }
    VectorizedBase(const VectorizedBase<V>* vec) :
      elements_(vec->elements_)
    {}
    VectorizedBase(sass::vector<T> vec) :
      elements_(std::move(vec))
    {}
    virtual ~VectorizedBase() {};
    size_t length() const { return elements_.size(); }
    bool empty() const { return elements_.empty(); }
    void clear() { return elements_.clear(); }


    T& last() { return elements_.back(); }
    T& first() { return elements_.front(); }
    const T& last() const { return elements_.back(); }
    const T& first() const { return elements_.front(); }

    bool operator== (const VectorizedBase<V>& rhs) const
    {
      // Abort early if sizes do not match
      if (length() != rhs.length()) return false;
      // Otherwise test each node for object equality in order
      return std::equal(begin(), end(), rhs.begin(), ObjEqualityFn<T>);
    }

    bool operator!= (const VectorizedBase<V>& rhs) const {
      return !(*this == rhs);
    }

    T& at(size_t i) { return elements_.at(i); }
    T& operator[](size_t i) { return elements_[i]; }
    const T& at(size_t i) const { return elements_.at(i); }
    const T& get(size_t i) const { return elements_[i]; }
    // ToDo: might insert am item (update ordered list)
    const T& operator[](size_t i) const { return elements_[i]; }

    // Implicitly get the sass::vector from our object
    // Makes the Vector directly assignable to sass::vector
    // You are responsible to make a copy if needed
    // Note: since this returns the real object, we can't
    // Note: guarantee that the hash will not get out of sync
    operator sass::vector<T>& () { return elements_; }
    operator const sass::vector<T>& () const { return elements_; }

    // Explicitly request all elements as a real sass::vector
    // You are responsible to make a copy if needed
    // Note: since this returns the real object, we can't
    // Note: guarantee that the hash will not get out of sync
    sass::vector<T>& elements() { return elements_; }
    const sass::vector<T>& elements() const { return elements_; }

    // Insert all items from compatible vector
    void concat(const sass::vector<T>& v)
    {
      elements().insert(end(), v.begin(), v.end());
    }

    // Insert all items from compatible vector
    void concat(sass::vector<T>&& v)
    {
      std::move(v.begin(), v.end(),
        std::back_inserter(elements_));
      // elements().insert(end(), v.begin(), v.end());
    }


    // Synthetic sugar for pointers
    void concat(const VectorizedBase<V>* v)
    {
      if (v != nullptr) {
        return concat(*v);
      }
    }

    // Insert one item on the front
    void unshift(const T& element)
    {
      elements_.insert(begin(), element);
    }

    void unshift(T&& element)
    {
      elements_.insert(begin(), std::move(element));
    }

    // Remove and return item on the front
    // ToDo: handle empty vectors
    // T shift() {
    //   T first = get(0);
    //   elements_.erase(begin());
    //   return first;
    // }

    // Insert one item on the back
    // ToDo: rename this to push
    void append(const T& element)
    {
      elements_.emplace_back(element);
    }
    void append(T&& element)
    {
      elements_.emplace_back(std::move(element));
    }

    // Check if an item already exists
    // Uses underlying object `operator==`
    // E.g. compares the actual objects
    bool contains(const T& el) const {
      for (const T& rhs : elements_) {
        // Test the underlying objects for equality
        // A std::find checks for pointer equality
        if (ObjEqualityFn(el, rhs)) {
          return true;
        }
      }
      return false;
    }

    bool contains(const V* el) const {
      for (const T& rhs : elements_) {
        // Test the underlying objects for equality
        // A std::find checks for pointer equality
        if (PtrObjEqualityFn(el, rhs.ptr())) {
          return true;
        }
      }
      return false;
    }

    // This might be better implemented as `operator=`?
    void elements(const sass::vector<T>& e) {
      elements_ = e;
    }

    void elements26(sass::vector<T>&& e) {
      elements_ = std::move(e);
    }

    template <typename P>
    typename sass::vector<T>::iterator insert(P position, const T& val) {
      return elements_.insert(position, val);
    }

    template <typename P>
    typename sass::vector<T>::iterator insert(P position, T&& val) {
      return elements_.insert(position, std::move(val));
    }

    typename sass::vector<T>::iterator end() { return elements_.end(); }
    typename sass::vector<T>::iterator begin() { return elements_.begin(); }
    typename sass::vector<T>::const_iterator end() const { return elements_.end(); }
    typename sass::vector<T>::const_iterator begin() const { return elements_.begin(); }
    typename sass::vector<T>::iterator erase(typename sass::vector<T>::iterator el) { return elements_.erase(el); }
    typename sass::vector<T>::const_iterator erase(typename sass::vector<T>::const_iterator el) { return elements_.erase(el); }

  };



  /////////////////////////////////////////////////////////////////////////////
// Base class/container for AST nodes that should behave like vectors.
/////////////////////////////////////////////////////////////////////////////
  template <typename V>
  class Vectorized2 {

  private:


    typedef SharedImpl<V> T;
    sass::vector<T> elements_;

  protected:
    mutable size_t hash_;
    void reset_hash() { hash_ = 0; }
  public:
    Vectorized2(const Vectorized2<V>* vec) :
      elements_(vec->elements_),
      hash_(0)
    {}
    Vectorized2(sass::vector<T> vec) :
      elements_(std::move(vec)),
      hash_(0)
    {}

    virtual ~Vectorized2() {};
    size_t length() const { return elements_.size(); }
    bool empty() const { return elements_.empty(); }


    const T& last() const { return elements_.back(); }
    const T& first() const { return elements_.front(); }

    bool operator== (const Vectorized2<V>& rhs) const
    {
      // Abort early if sizes do not match
      if (length() != rhs.length()) return false;
      // Otherwise test each node for object equality in order
      return std::equal(begin(), end(), rhs.begin(), ObjEqualityFn<T>);
    }

    bool operator!= (const Vectorized2<V>& rhs) const {
      return !(*this == rhs);
    }

    const T& get(size_t i) const { return elements_[i]; }
    const T& at(size_t i) const { return elements_.at(i); }
    // ToDo: might insert am item (update ordered list)
    const T& operator[](size_t i) const { return elements_[i]; }

    // Implicitly get the sass::vector from our object
    // Makes the Vector directly assignable to sass::vector
    // You are responsible to make a copy if needed
    // Note: since this returns the real object, we can't
    // Note: guarantee that the hash will not get out of sync
    operator const sass::vector<T>& () const { return elements_; }

    // Explicitly request all elements as a real sass::vector
    // You are responsible to make a copy if needed
    // Note: since this returns the real object, we can't
    // Note: guarantee that the hash will not get out of sync
    const sass::vector<T>& elements() const { return elements_; }

    // Check if an item already exists
    // Uses underlying object `operator==`
    // E.g. compares the actual objects
    bool contains(const T& el) const {
      for (const T& rhs : elements_) {
        // Test the underlying objects for equality
        // A std::find checks for pointer equality
        if (ObjEqualityFn(el, rhs)) {
          return true;
        }
      }
      return false;
    }

    bool contains(const V* el) const {
      for (const T& rhs : elements_) {
        // Test the underlying objects for equality
        // A std::find checks for pointer equality
        if (PtrObjEqualityFn(el, rhs.ptr())) {
          return true;
        }
      }
      return false;
    }

    virtual size_t hash() const
    {
      if (hash_ == 0) {
        for (const T& el : elements_) {
          hash_combine(hash_, el->hash());
        }
      }
      return hash_;
    }

    typename sass::vector<T>::const_iterator end() const { return elements_.end(); }
    typename sass::vector<T>::const_iterator begin() const { return elements_.begin(); }

  };
  /////////////////////////////////////////////////////////////////////////////
  // Mixin class for AST nodes that should behave like a hash table. Uses an
  // extra <std::vector> internally to maintain insertion order for iteration.
  /////////////////////////////////////////////////////////////////////////////
  template <typename K, typename T>
  class Hashed {

  public:
    using ordered_map_type = typename OrderedMap<
      K, T, ObjHash, ObjEquality,
      Sass::Allocator<std::pair<K, T>>,
      sass::vector<std::pair<K, T>>
    >;

  protected:

    ordered_map_type elements_;

    mutable size_t hash_;

    void reset_hash() { hash_ = 0; }

  public:

    Hashed()
      : elements_(),
      hash_(0)
    {
      // elements_.reserve(s);
    }

    // Copy constructor
    Hashed(const Hashed<K, T>& copy) :
      elements_(), hash_(0)
    {
      // this seems to expensive!?
      // elements_.reserve(copy.size());
      elements_ = copy.elements_;
    };

    // Move constructor
    Hashed(Hashed<K, T>&& move) :
      elements_(std::move(move.elements_)),
      hash_(move.hash_) {};

    // Move constructor
    Hashed(ordered_map_type&& values) :
      elements_(std::move(values)),
      hash_(0) {};

    virtual ~Hashed() {}

    size_t size() const { return elements_.size(); }
    bool empty() const { return elements_.empty(); }

    bool has(K k) const {
      return elements_.count(k) == 1;
    }

    void reserve(size_t size)
    {
      elements_.reserve(size);
    }

    T at(K k) const {
      auto it = elements_.find(k);
      if (it == elements_.end()) return {};
      else return it->second;
    }

    bool erase(K key)
    {
      reset_hash();
      return elements_.erase(key) != 0;
    }

    void set(std::pair<K, T>& kv)
    {
      reset_hash();
      elements_[kv.first] = kv.second;
    }

    void insert(K key, T val)
    {
      reset_hash();
      elements_[key] = val;
    }

    void concat(Hashed<K, T> arr)
    {
      reset_hash();
      for (const auto& kv : arr) {
        elements_[kv.first] = kv.second;
      }
      // elements_.append(arr.elements());
    }

    // Return unmodifiable reference
    const ordered_map_type& elements() const {
      return elements_;
    }

    const sass::vector<K> keys() const {
      sass::vector<T> list;
      for (auto kv : elements_) {
        list.emplace_back(kv.first);
      }
      return list;
    }
    const sass::vector<T> values() const {
      sass::vector<T> list;
      for (auto kv : elements_) {
        list.emplace_back(kv.second);
      }
      return list;
    }

    typename ordered_map_type::iterator end() { return elements_.end(); }
    typename ordered_map_type::iterator begin() { return elements_.begin(); }
    typename ordered_map_type::const_iterator end() const { return elements_.end(); }
    typename ordered_map_type::const_iterator begin() const { return elements_.begin(); }

  };

};

#endif
