#include "../src/util_string.hpp"
#include "assert.hpp"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

bool testEqualsLiteral() {
  ASSERT_TRUE(Sass::Util::equalsLiteral("moz", "moz"));
  ASSERT_TRUE(Sass::Util::equalsLiteral(":moz", ":moz"));
  ASSERT_FALSE(Sass::Util::equalsLiteral("moz", ":moz"));
  ASSERT_FALSE(Sass::Util::equalsLiteral(":moz", "moz"));
  ASSERT_TRUE(Sass::Util::equalsLiteral("moz-foo", "MOZ-foo"));
  ASSERT_FALSE(Sass::Util::equalsLiteral("moz-foo", "moz_foo"));
  ASSERT_TRUE(Sass::Util::equalsLiteral("moz-foo", "MOZ-FOOS"));
  ASSERT_FALSE(Sass::Util::equalsLiteral("moz-foos", "moz-foo"));
  ASSERT_FALSE(Sass::Util::equalsLiteral("-moz-foo", "moz-foo"));
  return true;

}

bool TestUnvendor() {
  // Generated by using dart sass
  ASSERT_STR_EQ("moz", Sass::Util::unvendor("moz"));
  ASSERT_STR_EQ(":moz", Sass::Util::unvendor(":moz"));
  ASSERT_STR_EQ("-moz", Sass::Util::unvendor("-moz"));
  ASSERT_STR_EQ("--moz", Sass::Util::unvendor("--moz"));
  ASSERT_STR_EQ("moz-bar", Sass::Util::unvendor("moz-bar"));
  ASSERT_STR_EQ("bar", Sass::Util::unvendor("-moz-bar"));
  ASSERT_STR_EQ("bar-", Sass::Util::unvendor("-moz-bar-"));
  ASSERT_STR_EQ("--moz-bar", Sass::Util::unvendor("--moz-bar"));
  ASSERT_STR_EQ("-bar", Sass::Util::unvendor("-moz--bar"));
  ASSERT_STR_EQ("any", Sass::Util::unvendor("-s-any"));
  ASSERT_STR_EQ("any-more", Sass::Util::unvendor("-s-any-more"));
  ASSERT_STR_EQ("any--more", Sass::Util::unvendor("-s-any--more"));
  ASSERT_STR_EQ("--s-any--more", Sass::Util::unvendor("--s-any--more"));
  ASSERT_STR_EQ("s-any--more", Sass::Util::unvendor("s-any--more"));
  ASSERT_STR_EQ("_s_any_more", Sass::Util::unvendor("_s_any_more"));
  ASSERT_STR_EQ("more", Sass::Util::unvendor("-s_any-more"));
  ASSERT_STR_EQ("any_more", Sass::Util::unvendor("-s-any_more"));
  ASSERT_STR_EQ("_s_any_more", Sass::Util::unvendor("_s_any_more"));
  return true;
}

bool TestSplitString1() {
  Sass::sass::vector<Sass::sass::string> list =
    Sass::Util::split_string("a,b,c", ',');
  ASSERT_NR_EQ(3, list.size());
  ASSERT_STR_EQ("a", list[0]);
  ASSERT_STR_EQ("b", list[1]);
  ASSERT_STR_EQ("c", list[2]);
  return true;
}

bool TestSplitString2() {
  Sass::sass::vector<Sass::sass::string> list =
    Sass::Util::split_string("a,b,", ',');
  ASSERT_NR_EQ(3, list.size());
  ASSERT_STR_EQ("a", list[0]);
  ASSERT_STR_EQ("b", list[1]);
  ASSERT_STR_EQ("", list[2]);
  return true;
}

bool TestSplitStringEmpty() {
  Sass::sass::vector<Sass::sass::string> list =
    Sass::Util::split_string("", ',');
  ASSERT_NR_EQ(0, list.size());
  return true;
}

bool Test_ascii_str_to_lower() {
  Sass::sass::string str = "A B";
  Sass::Util::ascii_str_tolower(&str);
  ASSERT_STR_EQ("a b", str);
  return true;
}

bool Test_ascii_str_to_upper() {
  Sass::sass::string str = "a b";
  ASSERT_STR_EQ("A B", Sass::Util::ascii_str_toupper(str));
  Sass::Util::ascii_str_toupper(&str);
  ASSERT_STR_EQ("A B", str);
  return true;
}

bool Test_ascii_isalpha() {
  ASSERT_TRUE(Sass::Util::ascii_isalpha('a'));
  ASSERT_FALSE(Sass::Util::ascii_isalpha('3'));
  return true;
}

bool Test_ascii_isxdigit() {
  ASSERT_TRUE(Sass::Util::ascii_isxdigit('a'));
  ASSERT_TRUE(Sass::Util::ascii_isxdigit('F'));
  ASSERT_TRUE(Sass::Util::ascii_isxdigit('3'));
  ASSERT_FALSE(Sass::Util::ascii_isxdigit('G'));
  return true;
}

bool Test_ascii_isspace() {
  ASSERT_TRUE(Sass::Util::ascii_isspace(' '));
  ASSERT_TRUE(Sass::Util::ascii_isspace('\t'));
  ASSERT_TRUE(Sass::Util::ascii_isspace('\v'));
  ASSERT_TRUE(Sass::Util::ascii_isspace('\f'));
  ASSERT_TRUE(Sass::Util::ascii_isspace('\r'));
  ASSERT_TRUE(Sass::Util::ascii_isspace('\n'));
  ASSERT_FALSE(Sass::Util::ascii_isspace('G'));
  return true;
}

bool TestEqualsIgnoreSeparator() {
  ASSERT_TRUE(Sass::Util::ascii_str_equals_ignore_separator("fOo", "FoO"));
  ASSERT_TRUE(Sass::Util::ascii_str_equals_ignore_separator("fOo-BaR", "FoO_bAr"));
  ASSERT_TRUE(Sass::Util::ascii_str_equals_ignore_separator("FoO_bAr", "fOo-BaR"));
  return true;
}

bool TestHashIgnoreSeparator() {
  ASSERT_TRUE(Sass::Util::hash_ignore_separator("fOo") == Sass::Util::hash_ignore_separator("FoO"));
  ASSERT_TRUE(Sass::Util::hash_ignore_separator("fOo-BaR") == Sass::Util::hash_ignore_separator("FoO_bAr"));
  ASSERT_TRUE(Sass::Util::hash_ignore_separator("FoO_bAr") == Sass::Util::hash_ignore_separator("fOo-BaR"));
  return true;
}

}  // namespace

int main(int argc, char **argv) {
  INIT_TEST_RESULTS;
  TEST(testEqualsLiteral);
  TEST(TestUnvendor);
  TEST(TestSplitStringEmpty);
  TEST(TestSplitString1);
  TEST(TestSplitString2);
  TEST(Test_ascii_str_to_lower);
  TEST(Test_ascii_str_to_upper);
  TEST(Test_ascii_isalpha);
  TEST(Test_ascii_isxdigit);
  TEST(Test_ascii_isspace);
  TEST(TestEqualsIgnoreSeparator);
  // TEST(TestHashIgnoreSeparator);
  REPORT_TEST_RESULTS;
}
