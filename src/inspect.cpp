// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include <cmath>
#include <string>
#include <iostream>
#include <iomanip>
#include <stdint.h>

#include "charcode.hpp"
#include "character.hpp"

#include "ast.hpp"
#include "util.hpp"
#include "inspect.hpp"
#include "context.hpp"
#include "listize.hpp"
#include "color_maps.hpp"
#include "utf8/checked.h"
// #include "debugger.hpp"

namespace Sass {

  // Import some namespaces
  using namespace Charcode;
  using namespace Character;

  Inspect::Inspect(const Emitter& emi)
  : Emitter(emi), quotes(true)
  { }
  Inspect::~Inspect() { }

  void Inspect::visitBlockStatements(sass::vector<StatementObj> children)
  {
    append_scope_opener();
    for (Statement* stmt : children) {
      stmt->perform(this);
    }
    append_scope_closer();
  }

  void Inspect::visitQuotedString(const sass::string& text, uint8_t quotes)
  {

    // Scan the string first, dart-sass seems to do some fancy
    // trick by calling itself recursively when it encounters a
    // conflicting quote during the output, throwing away buffers.
    bool includesSingleQuote = string_constains(text, $single_quote);
    bool includesDoubleQuote = string_constains(text, $double_quote);

    // If both quotes are encountered
    if (quotes == $nul) {
      if (includesSingleQuote) quotes = $double_quote;
      else if (includesDoubleQuote) quotes = $single_quote;
      else quotes = $double_quote;
    }

    // for (uint8_t chr : text) {
    //   if (chr == $single_quote) includesSingleQuote = true;
    //   else if (chr == $double_quote) includesDoubleQuote = true;
    // }

    append_char(quotes);

    uint8_t chr, next;
    for (size_t i = 0, iL = text.size(); i < iL; i++) {
      chr = text[i];
      switch (chr) {
      case $single_quote:
        if (quotes == $single_quote) {
          append_char($backslash);
        }
        append_char($single_quote);
        break;
      case $double_quote:
        if (quotes == $double_quote) {
          append_char($backslash);
        }
        append_char($double_quote);
        break;
      // Write newline characters and unprintable ASCII characters as escapes.
      case $nul:
      case $soh:
      case $stx:
      case $etx:
      case $eot:
      case $enq:
      case $ack:
      case $bel:
      case $bs:
      case $lf:
      case $vt:
      case $ff:
      case $cr:
      case $so:
      case $si:
      case $dle:
      case $dc1:
      case $dc2:
      case $dc3:
      case $dc4:
      case $nak:
      case $syn:
      case $etb:
      case $can:
      case $em:
      case $sub:
      case $esc:
      case $fs:
      case $gs:
      case $rs:
      case $us:
        append_char($backslash);
        if (chr > 0xF) append_char(hexCharFor(chr >> 4));
        append_char(hexCharFor(chr & 0xF));
        if (iL == i + 1) break;
        next = text[i+1];
        if (isHex(next) || next == $space || next == $tab) {
          append_char($space);
        }
        break;
      case $backslash:
        append_char($backslash);
        append_char($backslash);
        break;
      default:
        append_char(chr);
        break;
      }
    }

    append_char(quotes);

  }
  // EO visitQuotedString

  void Inspect::operator()(Variable* node)
  {
    append_token(node->name().orig(), node);
  }

  void Inspect::operator()(CssString* node)
  {
    append_token(node->text(), node);
  }

  void Inspect::operator()(CssValue* node)
  {
    if (node->value()) {
      node->value()->perform(this);
    }
  }

  void Inspect::visitCssMediaRule(CssMediaRule* node)
  {
    append_indentation();
    append_token("@media", node);
    append_mandatory_space();
    bool joinIt = false;
    for (auto query : node->queries()) {
      if (joinIt) {
        append_comma_separator();
        append_optional_space();
      }
      operator()(query);
      joinIt = true;
    }
    visitBlockStatements(node->elements());
  }
  // EO visitCssMediaRule

  void Inspect::visitCssStyleRule(CssStyleRule* node)
  {
    SelectorListObj s = node->selector();

    if (!s || s->empty()) return;

    if (output_style() == NESTED) {
      indentation += node->tabs();
    }

    if (opt.source_comments) {
      sass::sstream ss;
      append_indentation();
      sass::string path(File::abs2rel(node->pstate().getPath(), ".", File::get_cwd())); // ToDo: optimize
      ss << "/* line " << node->pstate().getLine() << ", " << path << " */";
      append_string(ss.str());
      append_optional_linefeed();
    }

    scheduled_crutch = s;
    if (s) s->perform(this);
    append_scope_opener(node);

    for (size_t i = 0, L = node->length(); i < L; ++i) {
      node->get(i)->perform(this);
    }

    if (output_style() == NESTED) {
      indentation -= node->tabs();
    }
    append_scope_closer(node);
  }
  // EO visitCssStyleRule

  void Inspect::visitCssMediaQuery(CssMediaQuery* query)
  {
    bool joinIt = false;
    if (!query->modifier().empty()) {
      append_string(query->modifier());
      append_mandatory_space();
    }
    if (!query->type().empty()) {
      append_string(query->type());
      joinIt = true;
    }
    for (auto feature : query->features()) {
      if (joinIt) {
        append_mandatory_space();
        append_string("and");
        append_mandatory_space();
      }
      append_string(feature);
      joinIt = true;
    }
  }
  // EO visitCssMediaQuery

  void Inspect::visitCssComment(CssComment* node)
  {
    append_string(node->text());
  }
  // EO visitCssComment

  void Inspect::visitCssDeclaration(CssDeclaration* node)
  {
    LOCAL_FLAG(in_declaration, true);
    LOCAL_FLAG(in_custom_property,
      node->is_custom_property());
    if (output_style() == NESTED)
      indentation += node->tabs();
    append_indentation();
    if (node->name()) {
      node->name()->perform(this);
    }
    append_colon_separator();
    if (node->value()) {
      node->value()->perform(this);
    }
    append_delimiter();
    if (output_style() == NESTED)
      indentation -= node->tabs();
  }
  // EO visitCssDeclaration

  // statements
  void Inspect::operator()(Block* block)
  {
    if (!block->is_root()) {
      add_open_mapping(block);
      append_scope_opener();
    }
    if (output_style() == NESTED) indentation += block->tabs();
    for (size_t i = 0, L = block->length(); i < L; ++i) {
      (*block)[i]->perform(this);
    }
    if (output_style() == NESTED) indentation -= block->tabs();
    if (!block->is_root()) {
      append_scope_closer();
      add_close_mapping(block);
    }

  }

  void Inspect::operator()(CssKeyframeBlock* node)
  {

    if (node->selector()) {

      const sass::vector<sass::string>& selector
        = node->selector()->texts();

      if (!selector.empty()) {
        append_indentation();
        bool addComma = false;
        for (sass::string sel : selector) {
          if (addComma) {
            append_comma_separator();
          }
          append_string(sel);
          addComma = true;
        }
      }

    }
    // StringLiteralObj v2 = node->name2();
    // 
    // if (!v2.isNull()) {
    //   append_indentation();
    //   v2->perform(this);
    // }
    // 
    // append_scope_opener();
    // for (size_t i = 0, L = r->length(); i < L; ++i) {
    //   Statement_Obj stm = r->get(i);
    //   stm->perform(this);
    //   if (i < L - 1) append_special_linefeed();
    // }
    // append_scope_closer();
    if (!node->empty()) {
      append_scope_opener();
      for (Statement* stmt : node->elements()) {
        stmt->perform(this);
      }
      append_scope_closer();
    }
  }

  void Inspect::operator()(CssMediaRule* rule)
  {
    visitCssMediaRule(rule);
  }

  void Inspect::operator()(CssMediaQuery* query)
  {
    visitCssMediaQuery(query);
  }

  void Inspect::visitCssAtRule(CssAtRule* node)
  {
    append_indentation();
    if (node->name()) {
      append_char($at);
      node->name()->perform(this);
    }
    if (node->value()) {
      append_mandatory_space();
      node->value()->perform(this);
    }
    if (!node->isChildless()) {
      if (node->empty()) {
        append_optional_space();
        append_string("{}");
      }
      else {
        visitBlockStatements(node->elements());
      }
    }
    else {
      append_delimiter();
    }
  }


  void Inspect::operator()(CssAtRule* node)
  {
    visitCssAtRule(node);
  }

  void Inspect::operator()(CssDeclaration* dec)
  {
    visitCssDeclaration(dec);
  }

  void Inspect::operator()(Import* import)
  {
    if (!import->urls().empty()) {
      append_token("@import", import);
      append_mandatory_space();
      import->urls().front()->perform(this);
      if (import->urls().size() == 1) {
        for (auto query : import->queries()) {
          append_mandatory_space();
          query->perform(this);
        }
      }
      append_delimiter();
      for (size_t i = 1, S = import->urls().size(); i < S; ++i) {
        append_mandatory_linefeed();
        append_token("@import", import);
        append_mandatory_space();

        import->urls()[i]->perform(this);
        if (import->urls().size() - 1 == i) {
          for (auto query : import->queries()) {
            append_mandatory_space();
            query->perform(this);
          }
        }
        append_delimiter();
      }
    }
  }

  void Inspect::operator()(ImportRule* rule)
  {
    for (ImportBaseObj imp : rule->elements()) {
      if (imp) imp->perform(this);
    }
  }

  void Inspect::operator()(StaticImport* rule)
  {
    append_indentation();
    append_token("@import", rule);
    append_mandatory_space();
    // append_string("url(");
    if (rule->url2()) append_string(rule->url2()->text());
    else if (rule->url()) append_string(rule->url());
    // append_string(")");
    append_delimiter();
  }

  void Inspect::operator()(CssComment* node)
  {
    visitCssComment(node);
  }

  void Inspect::_writeMapElement(Expression* ex)
  {
    if (Value * value = Cast<Value>(ex)) {
      bool needsParens = false;
      if (SassList * list = Cast<SassList>(value)) {
        needsParens = list->separator() == SASS_COMMA;
        if (list->hasBrackets()) needsParens = false;
      }
      if (needsParens) {
        append_char($lparen);
        value->perform(this);
        append_char($rparen);
      }
      else {
        value->perform(this);
      }
    }
    else {
      ex->perform(this);
    }
  }

  void Inspect::operator()(Trace* trace)
  {
    Block* block = trace->block();
    for (Statement* stmt : block->elements()) {
      stmt->perform(this);
    }
  }

  void Inspect::operator()(Map* map)
  {
    if (output_style() == TO_CSS) {
      // should be handle in check_expression
      throw Exception::InvalidValue({}, *map);
    }

    if ((output_style() == TO_SASS/* || output_style() == INSPECT*/) && map->empty()) {
      append_string("()");
      return;
    }
    if (map->empty()) return;
    if (map->is_invisible()) return;
    bool items_output = false;
    append_string("(");
    for (auto kv : map->elements()) {
      if (items_output) append_comma_separator();
      _writeMapElement(kv.first);
      append_colon_separator();
      _writeMapElement(kv.second);
      items_output = true;
    }
    append_string(")");
  }

  void Inspect::visitAttributeSelector(AttributeSelector* attribute)
  {
    append_string("[");
    add_open_mapping(attribute);
    append_token(attribute->ns_name(), attribute);
    if (!attribute->op().empty()) {
      append_string(attribute->op());
      // std::cerr << "serialize " << attribute->isIdentifier() << "\n";
      if (attribute->isIdentifier() && !starts_with(attribute->value(), "--")) {
        append_string(attribute->value());
        if (attribute->modifier() != 0) {
          append_optional_space();
        }
      }
      else {
        // std::cerr << "VisitAttrSel " << attribute->isIdentifier() << "\n";
        visitQuotedString(attribute->value());
        if (attribute->modifier() != 0) {
          append_optional_space();
        }
      }
    }
    add_close_mapping(attribute);
    if (attribute->modifier() != 0) {
      append_mandatory_space();
      append_char(attribute->modifier());
    }
    append_string("]");
  }

  void Inspect::visitClassSelector(ClassSelector* klass)
  {
    append_token(klass->ns_name(), klass);
  }

  void Inspect::visitComplexSelector(ComplexSelector* complex)
  {
    bool many = false;
    if (complex->hasPreLineFeed()) {
      append_optional_linefeed();
    }
    for (SelectorComponentObj& item : complex->elements()) {
      if (many) append_optional_space();
      if (SelectorCombinator* combinator = item->getCombinator()) {
        visitSelectorCombinator(combinator);
      }
      else if (CompoundSelector* compound = item->getCompound()) {
        visitCompoundSelector(compound);
      }
      many = true;
    }
  }

  void Inspect::visitCompoundSelector(CompoundSelector* compound)
  {
    if (compound->hasRealParent()) {
      append_string("&");
    }
    for (SimpleSelectorObj& item : compound->elements()) {
      item->accept(*this);
    }
    // Add the post line break (from ruby sass)
    // Dart sass uses another logic for newlines
    if (compound->hasPostLineBreak()) {
      if (output_style() != COMPACT) {
        append_optional_linefeed();
      }
    }
  }

  void Inspect::visitSelectorCombinator(SelectorCombinator* combinator)
  {
    append_optional_space();
    switch (combinator->combinator()) {
    case SelectorCombinator::Combinator::CHILD: append_string(">"); break;
    case SelectorCombinator::Combinator::GENERAL: append_string("~"); break;
    case SelectorCombinator::Combinator::ADJACENT: append_string("+"); break;
    }
    append_optional_space();
    // Add the post line break (from ruby sass)
    // Dart sass uses another logic for newlines
    // if (combinator->hasPostLineBreak()) {
    //   if (output_style() != COMPACT) {
    //     // append_optional_linefeed();
    //   }
    // }
  }

  void Inspect::visitIDSelector(IDSelector* id)
  {
    append_token(id->ns_name(), id);
  }

  void Inspect::visitPlaceholderSelector(PlaceholderSelector* placeholder)
  {
    append_token(placeholder->name(), placeholder);
  }

  void Inspect::visitPseudoSelector(PseudoSelector* pseudo)
  {
    if (pseudo->name() != "") {
      append_string(":");
      if (pseudo->isSyntacticElement()) {
        append_string(":");
      }
      append_token(pseudo->ns_name(), pseudo);
      // this whole logic can be done simpler!? copy object?
      if (pseudo->selector() || !pseudo->argument().empty()) {
        append_string("(");
        append_string(pseudo->argument());
        if (pseudo->selector() && !pseudo->argument().empty()) {
          if (!pseudo->selector()->empty()) {
            append_mandatory_space();
          }
        }
        bool was_comma_array = in_comma_array;
        in_comma_array = false;
        if (pseudo->selector()) {
          visitSelectorList(pseudo->selector());
        }
        in_comma_array = was_comma_array;
        append_string(")");
      }
    }
  }

  void Inspect::visitSelectorList(SelectorList* list)
  {
    if (list->empty()) {
      if (output_style() == TO_SASS) {
        append_token("()", list);
      }
      return;
    }

    bool was_comma_array = in_comma_array;
    // probably ruby sass eqivalent of element_needs_parens
    if (output_style() == TO_SASS && list->length() == 1 &&
      (
        Cast<SassList>(list->get(0)) &&
        !Cast<SelectorList>(list->get(0)))) {
      append_string("(");
    }
    else if (!in_declaration && in_comma_array) {
      append_string("(");
    }

    if (in_declaration) in_comma_array = true;

    for (size_t i = 0, L = list->length(); i < L; ++i) {

      if (i == 0) append_indentation();
      if ((*list)[i] == nullptr) continue;
      schedule_mapping(list->get(i)->last());
      // add_open_mapping(list->get(i)->last());
      visitComplexSelector(list->get(i));
      // add_close_mapping(list->get(i)->last());
      if (i < L - 1) {
        scheduled_space = 0;
        append_comma_separator();
      }
    }

    in_comma_array = was_comma_array;
    // probably ruby sass eqivalent of element_needs_parens
    if (output_style() == TO_SASS && list->length() == 1 &&
      (
        Cast<SassList>((*list)[0]) &&
        !Cast<SelectorList>(list->get(0)))) {
      append_string(",)");
    }
    else if (!in_declaration && in_comma_array) {
      append_string(")");
    }
  }

  void Inspect::visitTypeSelector(TypeSelector* type)
  {
    append_token(type->ns_name(), type);
  }

  // Returns whether [value] needs parentheses as an
  // element in a list with the given [separator].
  bool _elementNeedsParens(Sass_Separator separator, const Value* value) {
    // std::cerr << "Elements needs parens " << separator << " vs " << value->separator() << "\n";
    if (const SassList * list = Cast<SassList>(value)) {
      if (list->length() < 2) return false;
      if (list->hasBrackets()) return false;
      return separator == SASS_COMMA
        ? list->separator() == SASS_COMMA
        : list->separator() != SASS_UNDEF;
    }
    return false;
  }

  void Inspect::operator()(SassList* list)
  {
    bool inspect = output_style() == INSPECT
      || output_style() == TO_SASS;

    if (list->hasBrackets()) {
      append_char($lbracket);
    }

    else if (list->empty()) {
      if (!inspect) {
        // std::cerr << "() isn't a valid CSS value";
        throw Exception::InvalidValue({}, *list);
      }
      append_char($lparen);
      append_char($rparen);
      return;
    }

    bool singleton = inspect &&
      list->length() == 1 &&
      list->separator() == SASS_COMMA;
    if (singleton && !list->hasBrackets()) {
      append_char($lparen);
    }

    add_open_mapping(list);

    sass::vector<ValueObj> values
      = list->elements();

    bool first = true;
    sass::string joiner =
      list->separator() == SASS_SPACE ? " " :
      output_style() == COMPRESSED ? "," : ", ";
    for (Value* value : values) {
      // Only print `null` when inspecting
      if (!inspect && value->isBlank()) continue;
      if (first == false) {
        append_string(joiner);
      }
      else {
        first = false;
      }
      if (inspect) {
        bool needsParens = _elementNeedsParens(
          list->separator(), value);
        if (needsParens) {
          append_char($lparen);
        }
        value->perform(this);
        if (needsParens) {
          append_char($rparen);
        }
      }
      else {
        value->perform(this);
      }
    }

    add_close_mapping(list);

    if (singleton) {
      append_char($comma);
      if (!list->hasBrackets()) {
        append_char($rparen);
      }
    }

    if (list->hasBrackets()) {
      append_char($rbracket);
    }

  }

  /*
  void Inspect::operator()(ValueExpression* expr)
  {
    expr->value()->perform(this);
  }

  void Inspect::operator()(ParenthesizedExpression* expr)
  {
    append_string("(");
    if (expr->expression()) {
      expr->expression()->perform(this);
    }
    append_string(")");
  }
  */

  void Inspect::operator()(Unary_Expression* expr)
  {
    if (expr->optype() == Unary_Expression::PLUS)       append_string("+");
    else if (expr->optype() == Unary_Expression::SLASH) append_string("/");
    else                                                append_string("-");
    append_string(expr->operand()->to_string());
    // expr->operand()->perform(this);
  }

  // void Inspect::operator()(Variable* var)
  // {
  //   append_token(var->name(), var);
  // }

  void Inspect::operator()(Number* n)
  {

    if (n->lhsAsSlash() && n->rhsAsSlash()) {
      n->lhsAsSlash()->perform(this);
      append_string("/");
      n->rhsAsSlash()->perform(this);
      return;
    }

    // reduce units
    n->reduce();

    if (std::isnan(n->value())) {
      append_string("NaN");
      return;
    }

    if (std::isinf(n->value())) {
      append_string("Infinity");
      return;
    }

    sass::sstream ss;
    ss.precision(opt.precision);
    ss << std::fixed << n->value();

    sass::string res = ss.str();
    size_t s = res.length();

    // delete trailing zeros
    for(s = s - 1; s > 0; --s)
    {
        if(res[s] == '0') {
          res.erase(s, 1);
        }
        else break;
    }

    // delete trailing decimal separator
    if(res[s] == '.') res.erase(s, 1);

    // some final cosmetics
    if (res == "0.0") res = "0";
    else if (res == "") res = "0";
    else if (res == "-0") res = "0";
    else if (res == "-0.0") res = "0";

    // add unit now
    res += n->unit();

    if (opt.output_style == TO_CSS && !n->is_valid_css_unit()) {
      // traces.push_back(Backtrace(nr->pstate()));
      throw Exception::InvalidValue({}, *n);
    }

    // output the final token
    append_token(res, n);
  }

  // helper function for serializing colors
  template <size_t range>
  static double cap_channel(double c) {
    if      (c > range) return range;
    else if (c < 0)     return 0;
    else                return c;
  }

  void Inspect::operator()(Color_RGBA* c)
  {
    // output the final token
    sass::sstream ss;

    // original color name
    // maybe an unknown token
    sass::string name = c->disp();

    // resolved color
    sass::string res_name = name;

    double r = Sass::round(cap_channel<0xff>(c->r()), opt.precision);
    double g = Sass::round(cap_channel<0xff>(c->g()), opt.precision);
    double b = Sass::round(cap_channel<0xff>(c->b()), opt.precision);
    double a = cap_channel<1>   (c->a());

    // get color from given name (if one was given at all)
    if (name != "" && name_to_color(name)) {
      const Color_RGBA* n = name_to_color(name);
      r = Sass::round(cap_channel<0xff>(n->r()), opt.precision);
      g = Sass::round(cap_channel<0xff>(n->g()), opt.precision);
      b = Sass::round(cap_channel<0xff>(n->b()), opt.precision);
      a = cap_channel<1>   (n->a());
    }
    // otherwise get the possible resolved color name
    else {
      double numval = r * 0x10000 + g * 0x100 + b;
      if (color_to_name(numval))
        res_name = color_to_name(numval);
    }

    sass::sstream hexlet;
    // dart sass compressed all colors in regular css always
    // ruby sass and libsass does it only when not delayed
    // since color math is going to be removed, this can go too
    bool compressed = opt.output_style == COMPRESSED;
    hexlet << '#' << std::setw(1) << std::setfill('0');
    // create a short color hexlet if there is any need for it
    if (compressed && is_color_doublet(r, g, b) && a >= 1.0) {
      hexlet << std::hex << std::setw(1) << (static_cast<unsigned long>(r) >> 4);
      hexlet << std::hex << std::setw(1) << (static_cast<unsigned long>(g) >> 4);
      hexlet << std::hex << std::setw(1) << (static_cast<unsigned long>(b) >> 4);
      if (a != 1) hexlet << std::hex << std::setw(1) << (static_cast<unsigned long>(a * 255) >> 4);
    } else {
      hexlet << std::hex << std::setw(2) << static_cast<unsigned long>(r);
      hexlet << std::hex << std::setw(2) << static_cast<unsigned long>(g);
      hexlet << std::hex << std::setw(2) << static_cast<unsigned long>(b);
      if (a != 1) hexlet << std::hex << std::setw(2) << (static_cast<unsigned long>(a * 255) >> 4);
    }

    if (compressed) name = "";
    if (opt.output_style == INSPECT && a >= 1) {
      append_token(hexlet.str(), c);
      return;
    }

    // retain the originally specified color definition if unchanged
    if (name != "") {
      ss << name;
    }
    else if (a >= 1.0) {
      if (res_name != "") {
        if (compressed && hexlet.str().size() < res_name.size()) {
          ss << hexlet.str();
        } else {
          ss << res_name;
        }
      }
      else {
        ss << hexlet.str();
      }
    }

    else {
      ss << "rgba(";
      ss << static_cast<unsigned long>(r) << ",";
      if (!compressed) ss << " ";
      ss << static_cast<unsigned long>(g) << ",";
      if (!compressed) ss << " ";
      ss << static_cast<unsigned long>(b) << ",";
      if (!compressed) ss << " ";
      ss << a << ')';
    }

    append_token(ss.str(), c);

  }

  void Inspect::operator()(Color_HSLA* c)
  {
    Color_RGBA_Obj rgba = c->toRGBA();
    operator()(rgba);
  }

  void Inspect::operator()(Boolean* b)
  {
    // output the final token
    append_token(b->value() ? "true" : "false", b);
  }

  void Inspect::operator()(String_Constant* s)
  {
    if (quotes && s->hasQuotes()) {
      visitQuotedString(s->value());
    }
    else {
      append_token(s->value(), s);
    }
  }

  void Inspect::operator()(StringLiteral* str)
  {
    append_token(str->text(), str);
  }

  void Inspect::operator()(Interpolation* node)
  {
    bool inspect = output_style() == INSPECT
      || output_style() == TO_SASS;
    if (inspect) append_string("#{");
    for (Expression* value : node->elements()) {
      value->perform(this);
    }
    if (inspect) append_string("}");
  }

  void Inspect::operator()(Custom_Error* e)
  {
    append_token(e->message(), e);
  }

  void Inspect::operator()(Custom_Warning* w)
  {
    append_token(w->message(), w);
  }

  void Inspect::operator()(SassFunction* f)
  {
    bool inspect = output_style() == INSPECT
      || output_style() == TO_SASS;

    if (!inspect) {
      throw Exception::InvalidValue({}, *f);
    }
    append_token("get-function", f);
    append_string("(");
    f->callable()->perform(this);
    append_string(")");
  }

  void Inspect::operator()(BuiltInCallable* fn)
  {
    append_string(quote(fn->name().orig(), '"'));
  }

  void Inspect::operator()(BuiltInCallables* fn)
  {
    append_string(quote(fn->name().orig(), '"'));
  }

  void Inspect::operator()(UserDefinedCallable* c)
  {
    append_string(quote(c->declaration()->name().orig(), '"'));
  }

  void Inspect::operator()(Null* n)
  {
    if (output_style() == TO_CSS) return;
    // output the final token
    append_token("null", n);
  }

  void Inspect::operator()(PlaceholderSelector* node)
  {
    if (output_style() == INSPECT) {
      append_token(node->name(), node);
    }
  }

  void Inspect::operator()(SelectorList* g)
  {
    g->accept(*this);
  }
  
  void Inspect::operator()(CompoundSelector* sel)
  {
    // entry point is listize
    visitCompoundSelector(sel);
  }

  void Inspect::operator()(ComplexSelector* sel)
  {
    visitComplexSelector(sel);
  }

  void Inspect::operator()(SelectorCombinator* sel)
  {
    // entry point is listize
    visitSelectorCombinator(sel);
  }

  void Inspect::operator()(AttributeSelector* sel)
  {
    visitAttributeSelector(sel);
  }

  void Inspect::operator()(PseudoSelector* sel)
  {
    visitPseudoSelector(sel);
  }

  void Inspect::operator()(ClassSelector* sel)
  {
    visitClassSelector(sel);
  }

  void Inspect::operator()(TypeSelector* sel)
  {
    visitTypeSelector(sel);
  }

  void Inspect::operator()(IDSelector* sel)
  {
    visitIDSelector(sel);
  }

}
