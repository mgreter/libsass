#ifndef SASS_CONSTANTS_H
#define SASS_CONSTANTS_H

namespace Sass {
  namespace Constants {

    // The maximum call stack that can be created
    extern const unsigned long MaxCallStack;

    // https://developer.mozilla.org/en-US/docs/Web/CSS/Specificity
    // The following list of selectors is by increasing specificity:
    extern const unsigned long Specificity_Star;
    extern const unsigned long Specificity_Universal;
    extern const unsigned long Specificity_Element;
    extern const unsigned long Specificity_Base;
    extern const unsigned long Specificity_Class;
    extern const unsigned long Specificity_Attr;
    extern const unsigned long Specificity_Pseudo;
    extern const unsigned long Specificity_ID;

    // Selector unification order;
    extern const int UnificationOrder_Element;
    extern const int UnificationOrder_Id;
    extern const int UnificationOrder_Class;
    extern const int UnificationOrder_Attribute;
    extern const int UnificationOrder_PseudoClass;
    extern const int UnificationOrder_Wrapped;
    extern const int UnificationOrder_PseudoElement;
    extern const int UnificationOrder_Placeholder;

    // byte order marks
    // (taken from http://en.wikipedia.org/wiki/Byte_order_mark)
    extern const unsigned char utf_8_bom[];
    extern const unsigned char utf_16_bom_be[];
    extern const unsigned char utf_16_bom_le[];
    extern const unsigned char utf_32_bom_be[];
    extern const unsigned char utf_32_bom_le[];
    extern const unsigned char utf_7_bom_1[];
    extern const unsigned char utf_7_bom_2[];
    extern const unsigned char utf_7_bom_3[];
    extern const unsigned char utf_7_bom_4[];
    extern const unsigned char utf_7_bom_5[];
    extern const unsigned char utf_1_bom[];
    extern const unsigned char utf_ebcdic_bom[];
    extern const unsigned char scsu_bom[];
    extern const unsigned char bocu_1_bom[];
    extern const unsigned char gb_18030_bom[];

  }
}

#endif
