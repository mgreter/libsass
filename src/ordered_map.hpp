#ifndef SASS_ORDERED_MAP_H
#define SASS_ORDERED_MAP_H

#include <unordered_map>

namespace Sass {

  // ##########################################################################
  // Very simple and limited container for insert ordered hash map.
  // Piggy-back implementation on std::unordered_map and std::vector
  // ##########################################################################
  template<
    class Key,
    class T,
    class Hash = std::hash<Key>,
    class KeyEqual = std::equal_to<Key>,
    class Allocator = std::allocator<std::pair<const Key, T>>
  >
  class ordered_map {

  private:

    using map_type = typename std::unordered_map< Key, T, Hash, KeyEqual, Allocator>;
    using map_iterator = typename std::unordered_map< Key, T, Hash, KeyEqual, Allocator>::iterator;
    using map_const_iterator = typename std::unordered_map< Key, T, Hash, KeyEqual, Allocator>::const_iterator;

    // The main unordered map
    map_type _map;

    // Keep insertion order
    std::vector<Key> _keys;
    // That does not makes sense
    // Value might be updated ...
    std::vector<T> _values;

    const KeyEqual _keyEqual;

  public:

    ordered_map() :
      _keyEqual(KeyEqual())
    {
    }

    ordered_map& operator= (const ordered_map& other) {
      _map = other._map;
      _keys = other._keys;
      _values = other._values;
      return *this;
    }

    std::pair<Key, T> front() {
      return std::make_pair(
        _keys.front(),
        _values.front()
      );
    }

    bool empty() const {
      return _keys.empty();
    }

    void insert(const Key& key, const T& val) {
      if (!hasKey(key)) {
        _values.push_back(val);
        _keys.push_back(key);
      }
      _map[key] = val;
    }

    bool hasKey(const Key& key) const {
      return _map.find(key) != _map.end();
    }

    bool erase(const Key& key) {
      _map.erase(key);
      // find the position in the array
      for (size_t i = 0; i < _keys.size(); i += 1) {
        if (_keyEqual(key, _keys[i])) {
          _keys.erase(_keys.begin() + i);
          _values.erase(_values.begin() + i);
          return true;
        }
      }
      return false;
    }

    size_t size() const {
      return _map.size();
    }

    size_t count(const Key& key) const {
      return _map.count(key);
    }

    const std::vector<Key>& keys() const { return _keys; }
    const std::vector<T>& values() const { return _values; }

    map_iterator find(const Key& key) {
      return _map.find(key);
    }

    const T& get(const Key& key) const {
      if (hasKey(key)) {
        return _map.find(key)->second;
      }
      throw std::runtime_error("Key does not exist");
    }

    T& get(const Key& key) {
      if (hasKey(key)) {
        return _map[key];
      }
      throw std::runtime_error("Key does not exist");
    }

    T& operator[](const std::string& key) {
      if (hasKey(key)) {
        return _map[key];
      }
      T& value = _map[key];
      _keys.push_back(key);
      _values.push_back(value);
      return value;
    }

    using iterator = typename std::vector<Key>::iterator;
    using const_iterator = typename std::vector<Key>::const_iterator;
    using reverse_iterator = typename std::vector<Key>::reverse_iterator;
    using const_reverse_iterator = typename std::vector<Key>::const_reverse_iterator;

    typename std::vector<Key>::iterator end() { return _keys.end(); }
    typename std::vector<Key>::iterator begin() { return _keys.begin(); }
    typename std::vector<Key>::reverse_iterator rend() { return _keys.rend(); }
    typename std::vector<Key>::reverse_iterator rbegin() { return _keys.rbegin(); }
    typename std::vector<Key>::const_iterator end() const { return _keys.end(); }
    typename std::vector<Key>::const_iterator begin() const { return _keys.begin(); }
    typename std::vector<Key>::const_reverse_iterator rend() const { return _keys.rend(); }
    typename std::vector<Key>::const_reverse_iterator rbegin() const { return _keys.rbegin(); }

    typename map_type::iterator mapEnd() { return _map.end(); }
    typename map_type::iterator mapBegin() { return _map.begin(); }

  };

}

#endif
