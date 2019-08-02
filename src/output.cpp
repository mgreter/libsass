// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include "ast.hpp"
#include "output.hpp"
#include "util.hpp"
#include "debugger.hpp"
#include "charcode.hpp"
#include "character.hpp"

namespace Sass {

  // Import some namespaces
  using namespace Charcode;
  using namespace Character;

  Output::Output(Sass_Output_Options& opt)
    : Inspect(Emitter(opt)),
    charset(""),
    top_nodes(0)
  {}

  Output::~Output() { }

  void Output::fallback_impl(AST_Node* n)
  {
    return n->perform(this);
  }

  void Output::operator()(Number* n)
  {

    if (n->lhsAsSlash() && n->rhsAsSlash()) {
      n->lhsAsSlash()->perform(this);
      append_string("/");
      n->rhsAsSlash()->perform(this);
      return;
    }

    // check for a valid unit here
    // includes result for reporting
    if (!n->is_valid_css_unit()) {
      // should be handle in check_expression
      throw Exception::InvalidValue({}, *n);
    }
    // use values to_string facility
    sass::string res = n->to_string(opt);
    // output the final token
    append_token(res, n);
  }

  void Output::operator()(Import* imp)
  {
    if (imp->urls().size()) {
      top_nodes.emplace_back(imp);
    }
  }

  void Output::operator()(StaticImport* imp)
  {
    if (imp->outOfOrder()) {
      top_nodes.emplace_back(imp);
    }
    else {
      Inspect::operator()(imp);
    }
  }

  void Output::operator()(Map* m)
  {
    // should be handle in check_expression
    throw Exception::InvalidValue({}, *m);
  }

  OutputBuffer Output::get_buffer(void)
  {

    Emitter emitter(opt);
    Inspect inspect(emitter);

    size_t size_nodes = top_nodes.size();
    for (size_t i = 0; i < size_nodes; i++) {
      top_nodes[i]->perform(&inspect);
      inspect.append_mandatory_linefeed();
    }

    // flush scheduled outputs
    // maybe omit semicolon if possible
    inspect.finalize(wbuf.buffer.size() == 0);
    // prepend buffer on top
    prepend_output(inspect.output());
    // make sure we end with a linefeed
    if (!ends_with(wbuf.buffer, opt.linefeed)) {
      // if the output is not completely empty
      if (!wbuf.buffer.empty()) append_string(opt.linefeed);
    }

    // search for unicode char
    for (const char& chr : wbuf.buffer) {
      // skip all ascii chars
      // static cast to unsigned to handle `char` being signed / unsigned
      if (static_cast<unsigned>(chr) < 128) continue;
      // declare the charset
      if (output_style() != COMPRESSED)
        charset = "@charset \"UTF-8\";"
        + sass::string(opt.linefeed);
      else charset = "\xEF\xBB\xBF";
      // abort search
      break;
    }

    // add charset as first line, before comments and imports
    if (!charset.empty()) prepend_string(charset);

    return wbuf;

  }
  void Output::operator()(CssComment* c)
  {
    bool important = c->isPreserved();
    if (output_style() == COMPRESSED || output_style() == COMPACT) {
      if (!important) return;
    }
    if (output_style() != COMPRESSED || important) {
      if (buffer().size() == 0) {
        top_nodes.emplace_back(c);
      }
      else {
        append_indentation();
        append_string(c->text());
        if (indentation == 0) {
          append_mandatory_linefeed();
        }
        else {
          append_optional_linefeed();
        }
      }
    }
  }

  void Output::operator()(LoudComment* c)
  {
  }

  void Output::operator()(SilentComment* c)
  {
  }

  void Output::operator()(CssStyleRule* r)
  {
    Inspect::visitCssStyleRule(r);
  }

  void Output::operator()(Keyframe_Rule* r)
  {
    // Block_Obj b = r->block();
    StringLiteralObj v2 = r->name2();

    if (!v2.isNull()) {
      append_indentation();
      v2->perform(this);
    }

    append_scope_opener();
    for (size_t i = 0, L = r->length(); i < L; ++i) {
      Statement_Obj stm = r->get(i);
      stm->perform(this);
      if (i < L - 1) append_special_linefeed();
    }
    append_scope_closer();
  }

  void Output::operator()(CssSupportsRule* rule)
  {

    if (output_style() == NESTED) {
      indentation += rule->tabs();
    }

    append_indentation();
    append_token("@supports", rule);
    append_mandatory_space();
    rule->condition()->perform(this);
    append_scope_opener();

    // Block* b = rule->block();
    size_t L = rule->length();
    for (size_t i = 0; i < L; ++i) {
      rule->get(i)->perform(this);
      if (i < L - 1) append_special_linefeed();
    }

    if (output_style() == NESTED) {
      indentation -= rule->tabs();
    }

    append_scope_closer();

  }

  void Output::operator()(SupportsRule* f)
  {
  }

  void Output::operator()(CssMediaRule* rule)
  {
    // Avoid null pointer exception
    if (rule == nullptr) return;
    // Skip empty/invisible rule
    if (rule->isInvisible()) return;
    // Avoid null pointer exception
    // if (rule->block() == nullptr) return;
    // Skip empty/invisible rule
    bool invisible = true;
    for (auto& item : rule->elements()) {
      if (!item->is_invisible()) {
        invisible = false;
        break;
      }
    }
    if (invisible) return;
    // Skip if block is empty/invisible
    if (!rule->isInvisible()) {
      // Let inspect do its magic
      Inspect::visitCssMediaRule(rule);
    }
  }

  sass::string string_trim_trailing_lines(sass::string text)
  {
    auto start = text.begin();
    auto lastlf = text.end();
    auto end = lastlf - 1;
    while (end != start) {
      if (Util::ascii_islinefeed(*end)) {
        lastlf = end;
        end--;
      }
      else if (Util::ascii_iswhitespace(*end)) {
        end--;
      }
      else {
        break;
      }
    }
    if (lastlf != text.end()) {
      auto qwe = sass::string(start, lastlf) + " ";
      // std::cerr << "CHECKING [" << qwe << "]\n";
      return qwe;
    }
    return text;
  }

  void Output::operator()(String_Constant* s)
  {
    sass::string value(s->value());
    if (!in_custom_property) {
      if (s->hasQuotes()) {
        visitQuotedString(s->value());
      }
      else {
        append_token(string_to_output(value), s);
      }
    }
    else {
      append_token(string_trim_trailing_lines(value), s);
    }
  }

  void Output::operator()(StringLiteral* s)
  {
    append_string(s->text());
  }
}
