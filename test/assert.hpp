#ifndef ASSERT_HPP

std::string escape_string(const std::string& str) {
  std::string out;
  out.reserve(str.size());
  for (char c : str) {
    switch (c) {
    case '\n':
      out.append("\\n");
      break;
    case '\r':
      out.append("\\r");
      break;
    case '\f':
      out.append("\\f");
      break;
    default:
      out += c;
    }
  }
  return out;
}

#define ASSERT(cond) \
  if (!(cond)) { \
    std::cerr << "Assertion failed: " #cond " at " __FILE__ << ":" << __LINE__ << std::endl; \
    return false; \
  } \

#define ASSERT_TRUE(cond) \
  if (!(cond)) { \
    std::cerr << \
      "Expected condition to be true at " << __FILE__ << ":" << __LINE__ << \
      std::endl; \
    return false; \
  } \

#define ASSERT_FALSE(cond) \
  ASSERT_TRUE(!(cond)) \

#define ASSERT_NR_EQ(a, b) \
  if (a != b) { \
    std::cerr << \
      "Expected LHS == RHS at " << __FILE__ << ":" << __LINE__ << \
      "\n  LHS: [" << a << "]" \
      "\n  RHS: [" << b << "]" << \
      std::endl; \
    return false; \
  } \

#define ASSERT_STR_EQ(a, b) \
  if (a != b) { \
    std::cerr << \
      "Expected LHS == RHS at " << __FILE__ << ":" << __LINE__ << \
      "\n  LHS: [" << escape_string(a) << "]" \
      "\n  RHS: [" << escape_string(b) << "]" << \
      std::endl; \
    return false; \
  } \

#define INIT_TEST_RESULTS \
  std::vector<std::string> passed; \
  std::vector<std::string> failed; \

#define TEST(fn) \
  if (fn()) { \
    passed.push_back(#fn); \
  } else { \
    failed.push_back(#fn); \
    std::cerr << "Failed: " #fn << std::endl; \
  } \

#define REPORT_TEST_RESULTS \
  std::cerr << argv[0] << ": Passed: " << passed.size() \
    << ", failed: " << failed.size() \
    << "." << std::endl; \
  return failed.size(); \



#endif