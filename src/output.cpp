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
    std::string res = n->to_string(opt);
    // output the final token
    append_token(res, n);
  }

  void Output::operator()(Import* imp)
  {
    if (imp->urls().size()) {
      top_nodes.push_back(imp);
    }
  }

  void Output::operator()(StaticImport* imp)
  {
    if (imp->outOfOrder()) {
      top_nodes.push_back(imp);
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
        + std::string(opt.linefeed);
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
        top_nodes.push_back(c);
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
    SelectorListObj s = r->selector();

    if (!s || s->empty()) return;

    if (output_style() == NESTED) {
      indentation += r->tabs();
    }

    if (opt.source_comments) {
      std::stringstream ss;
      append_indentation();
      std::string path(File::abs2rel(r->pstate().path));
      ss << "/* line " << r->pstate().line + 1 << ", " << path << " */";
      append_string(ss.str());
      append_optional_linefeed();
    }

    scheduled_crutch = s;
    if (s) s->perform(this);
    append_scope_opener(r);

    for (size_t i = 0, L = r->block()->length(); i < L; ++i) {
      Statement_Obj stm = r->block()->get(i);
      bool bPrintExpression = true;
      // Check print conditions
      if (Declaration * dec = Cast<Declaration>(stm)) {
        if (const String_Constant * valConst = Cast<String_Constant>(dec->value())) {
          const std::string& val = valConst->value();
          if (const String_Quoted * qstr = Cast<const String_Quoted>(valConst)) {
            if (!qstr->quote_mark() && val.empty()) {
              bPrintExpression = false;
            }
          }
        }
        else if (const StringLiteral * valConst = Cast<StringLiteral>(dec->value())) {
          const std::string& val = valConst->text();
          if (const String_Quoted * qstr = Cast<const String_Quoted>(valConst)) {
            if (!qstr->quote_mark() && val.empty()) {
              bPrintExpression = false;
            }
          }
        }
        else if (List * list = Cast<List>(dec->value())) {
          bool all_invisible = true;
          for (size_t list_i = 0, list_L = list->length(); list_i < list_L; ++list_i) {
            Expression* item = list->get(list_i);
            if (!item->is_invisible()) all_invisible = false;
          }
          if (all_invisible && !list->is_bracketed()) bPrintExpression = false;
        }
      }
      // Print if OK
      if (bPrintExpression) {
        stm->perform(this);
      }
    }

    if (output_style() == NESTED) {
      indentation -= r->tabs();
    }
    append_scope_closer(r);
  }

  void Output::operator()(Keyframe_Rule* r)
  {
    Block_Obj b = r->block();
    Selector_Obj v = r->name();
    StringLiteralObj v2 = r->name2();


    // Disabled, never seen in specs
    // if (!v.isNull()) {
    //   v->perform(this);
    // }

    if (!v2.isNull()) {
      append_indentation();
      v2->perform(this);
    }

    // Disabled, never seen in specs
    // if (!b) {
    //   append_colon_separator();
    //   return;
    // }

    append_scope_opener();
    for (size_t i = 0, L = b->length(); i < L; ++i) {
      Statement_Obj stm = b->get(i);
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

    Block* b = rule->block();
    size_t L = b ? b->length() : 0;
    for (size_t i = 0; i < L; ++i) {
      b->get(i)->perform(this);
      if (i < L - 1) append_special_linefeed();
    }

    if (output_style() == NESTED) {
      indentation -= rule->tabs();
    }

    append_scope_closer();

  }

  void Output::operator()(SupportsRule* f)
  {
    if (f->is_invisible()) return;

    SupportsCondition_Obj c = f->condition();
    Block_Obj b = f->block();

    if (output_style() == NESTED) indentation += f->tabs();
    append_indentation();
    append_token("@supports", f);
    append_mandatory_space();
    c->perform(this);
    append_scope_opener();

    for (size_t i = 0, L = b->length(); i < L; ++i) {
      Statement_Obj stm = b->get(i);
      stm->perform(this);
      if (i < L - 1) append_special_linefeed();
    }

    if (output_style() == NESTED) indentation -= f->tabs();

    append_scope_closer();

  }

  void Output::operator()(CssMediaRule* rule)
  {
    // Avoid null pointer exception
    if (rule == nullptr) return;
    // Skip empty/invisible rule
    if (rule->isInvisible()) return;
    // Avoid null pointer exception
    if (rule->block() == nullptr) return;
    // Skip empty/invisible rule
    if (rule->block()->isInvisible()) return;
    // Skip if block is empty/invisible
    if (!rule->isInvisible()) {
      // Let inspect do its magic
      Inspect::operator()(rule);
    }
  }

  void Output::operator()(String_Quoted* s)
  {
    if (s->quote_mark()) {
      append_css(quote(s->value(), s->quote_mark()), s);
    }
    else {
      append_token(string_to_output(s->value()), s);
    }
  }

  std::string string_trim_trailing_lines(std::string text)
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
      auto qwe = std::string(start, lastlf) + " ";
      // std::cerr << "CHECKING [" << qwe << "]\n";
      return qwe;
    }
    return text;
  }

  void Output::operator()(String_Constant* s)
  {
    std::string value(s->value());
    if (!in_custom_property) {
      append_token(string_to_output(value), s);
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
