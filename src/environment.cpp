#include "sass.hpp"
#include "ast.hpp"
#include "environment.hpp"


namespace Sass {

  sass::string pluralize(const sass::string& singular, size_t size, const sass::string& plural)
  {
	  if (size == 1) return singular;
	  else if (!plural.empty()) return plural;
	  else return singular + "s";
  }

  sass::string toSentence(const EnvKeyFlatMap<ValueObj>& names, const sass::string& conjunction)
  {
    sass::string buffer;
	  size_t L = names.size(), i = 0;
	  auto it = names.begin();
	  while (i < L) {
		  // add conjugation
		  if (i > 0) {
			  if (i < L - 1) {
          buffer += ", ";
			  }
			  else {
          buffer += " ";
          buffer += conjunction;
          buffer += " ";
			  }
		  }
      buffer += it->first.orig();
      it++;
		  i++;
	  }
	  return buffer;
  }


  sass::string toSentence(const EnvKeyFlatMap<ExpressionObj>& names, const sass::string& conjunction)
  {
    sass::string buffer;
    size_t L = names.size(), i = 0;
    auto it = names.begin();
    while (i < L) {
      // add conjugation
      if (i > 0) {
        if (i < L - 1) {
          buffer += ", ";
        }
        else {
          buffer += " ";
          buffer += conjunction;
          buffer += " ";
        }
      }
      buffer += it->first.orig();
      it++;
      i++;
    }
    return buffer;
  }


  sass::string toSentence(const EnvKeySet& names, const sass::string& conjunction)
  {
    sass::string buffer;
    size_t L = names.size(), i = 0;
    auto it = names.begin();
    while (i < L) {
      // add conjugation
      if (i > 0) {
        if (i < L - 1) {
          buffer += ", ";
        }
        else {
          buffer += " ";
          buffer += conjunction;
          buffer += " ";
        }
      }
      buffer += it->orig();
      it++;
      i++;
    }
    return buffer;
  }

}
