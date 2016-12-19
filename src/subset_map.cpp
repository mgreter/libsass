#include "sass.hpp"
#include "ast.hpp"
#include "subset_map.hpp"

namespace Sass {

  void Subset_Map::put(const Compound_Selector_Obj& sel, const Subset_Map_Val& value)
  {
    if (sel->empty()) throw "internal error: subset map keys may not be empty";
    size_t index = values_.size();
    values_.push_back(value);
    sel->hash(); // generate once
    for (size_t i = 0, S = sel->length(); i < S; ++i)
    {
      hash_[(*sel)[i]].push_back(std::make_pair(&sel, index));
    }
  }

  typedef std::unordered_set<Simple_Selector_Obj, HashSimpleSelector, EqualSimpleSelector> Subset_Dict;
  std::vector<Subset_Map_Val> Subset_Map::get_kv(const Compound_Selector_Obj& sel)
  {
    std::vector<size_t> indices;
    Subset_Dict dict(sel->begin(), sel->end());
    for (size_t i = 0, S = sel->length(); i < S; ++i) {
      const auto found = hash_.find((*sel)[i]);
      if (found == hash_.end()) continue;
      for (const std::pair<Compound_Selector_Obj, size_t>& item : found->second) {
        bool include = true;
        for (const Simple_Selector_Obj& it : item.first->elements()) {
          auto found = dict.find(it);
          if (found == dict.end()) {
            include = false;
            break;
          }
        }
        if (include) indices.push_back(item.second);
      }
    }
    sort(indices.begin(), indices.end());
    auto end = unique(indices.begin(), indices.end());
    indices.erase( end, indices.end() );

    std::vector<Subset_Map_Val> results;
    for (size_t i = 0, S = indices.size(); i < S; ++i) {
      results.push_back(values_[indices[i]]);
    }
    return results;
  }

  std::vector<Subset_Map_Val> Subset_Map::get_v(const Compound_Selector_Obj& sel)
  {
    return get_kv(sel);
  }

}