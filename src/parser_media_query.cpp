#include "parser_media_query.hpp"

#include "ast.hpp"

#include "charcode.hpp"
#include "character.hpp"

namespace Sass {

  // Import some namespaces
  using namespace Charcode;
  using namespace Character;

  // Consume multiple media queries delimited by commas.
  std::vector<CssMediaQueryObj> MediaQueryParser::parse()
  {
    std::vector<CssMediaQueryObj> queries;
    do {
      whitespace();
      queries.push_back(_mediaQuery());
    } while (scanner.scanChar($comma));
    scanner.expectDone();
    return queries;
  }

  // Consumes a single media query.
  CssMediaQuery* MediaQueryParser::_mediaQuery()
  {
    // This is somewhat duplicated in StylesheetParser._mediaQuery.
    std::string type;
    std::string modifier;
    Position start(scanner);
    if (scanner.peekChar() != $lparen) {
      std::string identifier1 = identifier();
      whitespace();

      if (!lookingAtIdentifier()) {
        // For example, "@media screen {"
        CssMediaQuery* query = SASS_MEMORY_NEW(CssMediaQuery,
          scanner.pstate(start));
        query->type(identifier1);
        return query;
      }

      std::string identifier2 = identifier();
      whitespace();

      if (equalsIgnoreCase(identifier2, "and")) {
        // For example, "@media screen and ..."
        type = identifier1;
      }
      else {
        modifier = identifier1;
        type = identifier2;
        if (scanIdentifier("and")) {
          // For example, "@media only screen and ..."
          whitespace();
        }
        else {
          // For example, "@media only screen {"
          CssMediaQuery* query = SASS_MEMORY_NEW(CssMediaQuery,
            scanner.pstate(start));
          query->modifier(modifier);
          query->type(type);
          return query;
        }
      }

    }

    // We've consumed either `IDENTIFIER "and"`, `IDENTIFIER IDENTIFIER "and"`,
    // or no text.

    std::vector<std::string> features;
    do {
      whitespace();
      scanner.expectChar($lparen);
      auto decl = declarationValue();
      features.push_back("(" + decl + ")");
      scanner.expectChar($rparen);
      whitespace();
    } while (scanIdentifier("and"));

    if (type.empty()) {
      auto query = SASS_MEMORY_NEW(CssMediaQuery,
        scanner.pstate(start));
      query->features(features);
      return query;
    }
    else {
      auto query = SASS_MEMORY_NEW(CssMediaQuery,
        scanner.pstate(start));
      query->features(features);
      query->modifier(modifier);
      query->type(type);
      return query;
    }

  }
  // EO _mediaQuery

}
