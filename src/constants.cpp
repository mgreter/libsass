// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include "constants.hpp"

namespace Sass {
  namespace Constants {

    extern const unsigned long MaxCallStack = 1024;

    // https://github.com/sass/libsass/issues/592
    // https://developer.mozilla.org/en-US/docs/Web/CSS/Specificity
    // https://github.com/sass/sass/issues/1495#issuecomment-61189114
    extern const unsigned long Specificity_Star = 0;
    extern const unsigned long Specificity_Universal = 0;
    extern const unsigned long Specificity_Element = 1;
    extern const unsigned long Specificity_Base = 1000;
    extern const unsigned long Specificity_Class = 1000;
    extern const unsigned long Specificity_Attr = 1000;
    extern const unsigned long Specificity_Pseudo = 1000;
    extern const unsigned long Specificity_ID = 1000000;

    extern const int UnificationOrder_Element = 1;
    extern const int UnificationOrder_Id = 2;
    extern const int UnificationOrder_Class = 2;
    extern const int UnificationOrder_Attribute = 3;
    extern const int UnificationOrder_PseudoClass = 4;
    extern const int UnificationOrder_Wrapped = 5;
    extern const int UnificationOrder_PseudoElement = 6;
    extern const int UnificationOrder_Placeholder = 7;

    // byte order marks
    // (taken from http://en.wikipedia.org/wiki/Byte_order_mark)
    extern const unsigned char utf_8_bom[]      = { 0xEF, 0xBB, 0xBF };
    extern const unsigned char utf_16_bom_be[]  = { 0xFE, 0xFF };
    extern const unsigned char utf_16_bom_le[]  = { 0xFF, 0xFE };
    extern const unsigned char utf_32_bom_be[]  = { 0x00, 0x00, 0xFE, 0xFF };
    extern const unsigned char utf_32_bom_le[]  = { 0xFF, 0xFE, 0x00, 0x00 };
    extern const unsigned char utf_7_bom_1[]    = { 0x2B, 0x2F, 0x76, 0x38 };
    extern const unsigned char utf_7_bom_2[]    = { 0x2B, 0x2F, 0x76, 0x39 };
    extern const unsigned char utf_7_bom_3[]    = { 0x2B, 0x2F, 0x76, 0x2B };
    extern const unsigned char utf_7_bom_4[]    = { 0x2B, 0x2F, 0x76, 0x2F };
    extern const unsigned char utf_7_bom_5[]    = { 0x2B, 0x2F, 0x76, 0x38, 0x2D };
    extern const unsigned char utf_1_bom[]      = { 0xF7, 0x64, 0x4C };
    extern const unsigned char utf_ebcdic_bom[] = { 0xDD, 0x73, 0x66, 0x73 };
    extern const unsigned char scsu_bom[]       = { 0x0E, 0xFE, 0xFF };
    extern const unsigned char bocu_1_bom[]     = { 0xFB, 0xEE, 0x28 };
    extern const unsigned char gb_18030_bom[]   = { 0x84, 0x31, 0x95, 0x33 };

  }
}
