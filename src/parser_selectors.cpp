// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include "parser.hpp"
#include "file.hpp"
#include "inspect.hpp"
#include "constants.hpp"
#include "util.hpp"
#include "prelexer.hpp"
#include "color_maps.hpp"
#include "sass/functions.h"
#include "error_handling.hpp"
#include "util_string.hpp"

#include <cstdlib>
#include <iostream>
#include <vector>
#include <typeinfo>

namespace Sass {

  using namespace Prelexer;
  using namespace Constants;

  ComplexSelector_Obj Parser::parseComplexSelector(bool chroot)
  {

    NESTING_GUARD(nestings);

    lex < block_comment >();
    advanceToNextToken();

    ComplexSelector_Obj sel = SASS_MEMORY_NEW(ComplexSelector, pstate);

    sel->media_block(last_media_block);

    if (peek < end_of_file >()) return sel;

    while (true) {

      lex < block_comment >();
      advanceToNextToken();

      // check for child (+) combinator
      if (lex < exactly < selector_combinator_child > >()) {
        sel->append(SASS_MEMORY_NEW(SelectorCombinator, pstate, SelectorCombinator::CHILD));
      }
      // check for general sibling (~) combinator
      else if (lex < exactly < selector_combinator_general > >()) {
        sel->append(SASS_MEMORY_NEW(SelectorCombinator, pstate, SelectorCombinator::GENERAL));
      }
      // check for adjecant sibling (+) combinator
      else if (lex < exactly < selector_combinator_adjacent > >()) {
        sel->append(SASS_MEMORY_NEW(SelectorCombinator, pstate, SelectorCombinator::ADJACENT));
      }
      // check if we can parse a compound selector
      else if (CompoundSelector_Obj compound = parseCompoundSelector()) {
        sel->append(compound);
      }
      else {
        break;
      }
    }

    if (sel->empty()) return {};

    // check if we parsed any parent references
    sel->chroots(sel->has_real_parent_ref() || chroot);

    sel->update_pstate(pstate);

    return sel;

  }

  SelectorList_Obj Parser::parseSelectorList(bool chroot)
  {

    bool reloop;
    bool had_linefeed = false;
    NESTING_GUARD(nestings);
    ComplexSelector_Obj sel;
    SelectorList_Obj group = SASS_MEMORY_NEW(SelectorList, pstate);
    group->media_block(last_media_block);

    if (peek_css< alternatives < end_of_file, exactly <'{'>, exactly <','> > >()) {
      css_error("Invalid CSS", " after ", ": expected selector, was ");
    }

    do {
      reloop = false;

      had_linefeed = had_linefeed || peek_newline();

      if (peek_css< alternatives < class_char < selector_list_delims > > >())
        break; // in case there are superfluous commas at the end

      // now parse the complex selector
      sel = parseComplexSelector(chroot);
      // sel = parse_complex_selector(chroot)->toCplxSelector();

      if (!sel) return group.detach();

      sel->has_line_feed(had_linefeed);

      had_linefeed = false;

      while (peek_css< exactly<','> >())
      {
        lex< css_comments >(false);
        // consume everything up and including the comma separator
        reloop = lex< exactly<','> >() != 0;
        // remember line break (also between some commas)
        had_linefeed = had_linefeed || peek_newline();
        // remember line break (also between some commas)
      }
      group->append(sel);
    } while (reloop);
    while (lex_css< kwd_optional >()) {
      group->is_optional(true);
    }
    // update for end position
    group->update_pstate(pstate);

    // ToDo: mutable_last not implemented
    // if (sel) sel->mutable_last()->has_line_break(false);

    return group.detach();
  }




  // parse one compound selector, which is basically
  // a list of simple selectors (directly adjacent)
  // lex them exactly (without skipping white-space)
  CompoundSelector_Obj Parser::parseCompoundSelector()
  {
    // init an empty compound selector wrapper
    CompoundSelector_Obj seq = SASS_MEMORY_NEW(CompoundSelector, pstate);
    seq->media_block(last_media_block);

    // skip initial white-space
    lex < block_comment >();
    advanceToNextToken();

    if (lex< exactly<'&'> >(false))
    {
      // ToDo: check the conditions and try to simplify flag passing
      if (!allow_parent) error("Parent selectors aren't allowed here.");
      // Create and append a new parent selector object
      // seq->append(SASS_MEMORY_NEW(Parent_Selector, pstate));
      seq->hasRealParent(true);
    }

    // parse list
    while (true)
    {
      // remove all block comments
      // leaves trailing white-space
      lex < block_comment >();
      // parse parent selector
      if (lex< exactly<'&'> >(false))
      {
        // parent selector only allowed at start
        // upcoming Sass may allow also trailing
        ParserState state(pstate);
        Simple_Selector_Obj prev = (*seq)[seq->length()-1];
        std::string sel(prev->to_string({ NESTED, 5 }));
        std::string found("&");
        if (lex < identifier >()) { found += std::string(lexed); }
        // ToDo: parser should throw parser exceptions
        error("Invalid CSS after \"" + sel + "\": expected \"{\", was \"" + found + "\"\n\n"
          "\"" + found + "\" may only be used at the beginning of a compound selector.", state);
      }
      // parse functional
      else if (match < re_functional >())
        {
          seq->append(parse_simple_selector());
        }

      // parse type selector
      else if (lex< re_type_selector >(false))
      {
        seq->append(SASS_MEMORY_NEW(Type_Selector, pstate, lexed));
      }
      // peek for abort conditions
      else if (peek< spaces >()) break;
      else if (peek< end_of_file >()) { break; }
      else if (peek_css < class_char < selector_combinator_ops > >()) break;
      else if (peek_css < class_char < complex_selector_delims > >()) break;
      // otherwise parse another simple selector
      else {
        Simple_Selector_Obj sel = parse_simple_selector();
        if (!sel) return {};
        seq->append(sel);
      }
    }

    if (seq && !peek_css<alternatives<end_of_file,exactly<'{'>>>()) {
      seq->has_line_break(peek_newline());
    }

    // We may have set hasRealParent
    if (seq && seq->empty() && !seq->hasRealParent()) return {};

    // EO while true
    return seq;
  }


}
