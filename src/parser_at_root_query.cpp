#include "parser_at_root_query.hpp"

#include "character.hpp"
#include "utf8/checked.h"

namespace Sass {

  // Import some namespaces
  using namespace Charcode;
  using namespace Character;

  AtRootQuery* AtRootQueryParser::parse() {

    Offset start(scanner.offset);
    scanner.expectChar($lparen);
    whitespace();
    bool include = scanIdentifier("with");
    if (!include) expectIdentifier("without", "\"with\" or \"without\"");
    whitespace();
    scanner.expectChar($colon);
    whitespace();

    StringSet atRules;
    do {
      sass::string ident(identifier());
      StringUtils::makeLowerCase(ident);
      atRules.insert(ident);
      whitespace();
    } while (lookingAtIdentifier());
    scanner.expectChar($rparen);
    scanner.expectDone();

    return SASS_MEMORY_NEW(AtRootQuery,
      scanner.rawSpanFrom(start), atRules, include);

  }

}
