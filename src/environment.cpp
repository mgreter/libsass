#include "sass.hpp"
#include "ast.hpp"
#include "environment.hpp"


namespace Sass {


  sass::string pluralize(sass::string singular, size_t size, sass::string plural)
  {
	  if (size == 1) return singular;
	  else if (!plural.empty()) return plural;
	  else return singular + "s";
  }

  sass::string toSentence(KeywordMap<ValueObj>& names, const sass::string& conjunction)
  {
    sass::sstream strm;
	  size_t L = names.size(), i = 0;
	  auto it = names.begin();
	  while (i < L) {
		  // add conjugation
		  if (i > 0) {
			  if (i < L - 1) {
				  strm << ", ";
			  }
			  else {
				  strm << " " <<
					  conjunction << " ";
			  }
		  }
      strm << it->first.orig();
      it++;
		  i++;
	  }
	  return strm.str();
  }

}

