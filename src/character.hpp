#ifndef SASS_CHARACTER_H
#define SASS_CHARACTER_H

#include "charcode.hpp"

namespace Sass {

  namespace Character {

    using namespace Charcode;

    // The difference between upper- and lowercase ASCII letters.
    // `0b100000` can be bitwise-ORed with uppercase ASCII letters
    // to get their lowercase equivalents.
    const uint8_t asciiCaseBit = 0x20;

    // Returns whether [character] is an ASCII newline.
    inline bool isNewline(uint8_t character) {
      return character == $lf
        || character == $cr
        || character == $ff;
    }

    /// Returns whether [character] is a space or a tab character.
    inline bool isSpaceOrTab(uint8_t character) {
      return character == $space
        || character == $tab;
    }

    // Returns whether [character] is an ASCII whitespace character.
    inline bool isWhitespace(uint8_t character) {
      return isSpaceOrTab(character)
        || isNewline(character);
    }

    /// Returns whether [character] is a letter.
    inline bool isAlphabetic(uint8_t character) {
      return (character >= $a && character <= $z)
        || (character >= $A && character <= $Z);
    }

    /// Returns whether [character] is a number.
    inline bool isDigit(uint8_t character) {
      return character != 0
        && character >= $0
        && character <= $9;
    }

    /// Returns whether [character] is a letter or number.
    inline bool isAlphanumeric(uint8_t character) {
      return isAlphabetic(character)
        || isDigit(character);
    }

    /// Returns whether [character] is legal as the start of a Sass identifier.
    inline bool isNameStart(uint8_t character) {
      return character == $_
        || isAlphabetic(character)
        || character >= 0x0080;
    }

    /// Returns whether [character] is legal in the body of a Sass identifier.
    inline bool isName(uint8_t character) {
      return isNameStart(character)
        || isDigit(character)
        || character == $minus;
    }

    /// Returns whether [character] is a hexadeicmal digit.
    inline bool isHex(uint8_t character) {
      if (character == 0) return false;
      if (isDigit(character)) return true;
      if (character >= $a && character <= $f) return true;
      if (character >= $A && character <= $F) return true;
      return false;
    }

    /// Returns whether [character] is the beginning of a UTF-16 surrogate pair.
    // bool isHighSurrogate(uint8_t character) = >
    //   character >= 0xD800 && character <= 0xDBFF;

    // Returns whether [character] can start a simple selector other than a type
    // selector.
    inline bool isSimpleSelectorStart(uint8_t character)
    {
      return character == $asterisk
        || character == $lbracket
        || character == $dot
        || character == $hash
        || character == $percent
        || character == $colon;
    }

    /// Returns the value of [character] as a hex digit.
    ///
    /// Assumes that [character] is a hex digit.
    inline uint8_t asHex(uint8_t character)
    {
      // assert(isHex(character));
      if (character <= $9) return character - $0;
      if (character <= $F) return 10 + character - $A;
      return 10 + character - $a;
    }

    /// Returns the hexadecimal digit for [number].
    ///
    /// Assumes that [number] is less than 16.
    inline uint8_t hexCharFor(uint8_t number)
    {
      // assert(number < 0x10);
      return number < 0xA ? $0 + number
        : $a - 0xA + number;
    }

    /// Returns the value of [character] as a decimal digit.
    ///
    /// Assumes that [character] is a decimal digit.
    inline double asDecimal(uint8_t character)
    {
      // assert(character >= $0 && character <= $9);
      return character - $0;
    }

    /// Returns the decimal digit for [number].
    ///
    /// Assumes that [number] is less than 10.
    inline uint8_t decimalCharFor(uint8_t number)
    {
      // assert(number < 10);
      return $0 + number;
    }

    /// Assumes that [character] is a left-hand brace-like character, and returns
    /// the right-hand version.
    inline uint8_t opposite(uint8_t character)
    {
      switch (character) {
      case $lparen:
        return $rparen;
      case $lbrace:
        return $rbrace;
      case $lbracket:
        return $rbracket;
      default:
        return 0;
      }
    }

    /// Returns [character], converted to upper case if it's an ASCII lowercase
    /// letter.
    inline uint8_t toUpperCase(uint8_t character)
    {
      return (character >= $a && character <= $z)
        ? character & ~asciiCaseBit : character;
    }

    /// Returns [character], converted to lower case if it's an ASCII uppercase
    /// letter.
    inline uint8_t toLowerCase(uint8_t character)
    {
      return (character >= $A && character <= $Z)
        ? character | asciiCaseBit : character;
    }

    /// Returns whether [character1] and [character2] are the same, modulo ASCII case.
    inline bool characterEqualsIgnoreCase(uint8_t character1, uint8_t character2)
    {
      if (character1 == character2) return true;

      // If this check fails, the characters are definitely different. If it
      // succeeds *and* either character is an ASCII letter, they're equivalent.
      if ((character1 ^ character2) != asciiCaseBit) return false;

      // Now we just need to verify that one of the characters is an ASCII letter.
      uint8_t upperCase1 = character1 & ~asciiCaseBit;
      return upperCase1 >= $A && upperCase1 <= $Z;
    }

    /// Like [characterEqualsIgnoreCase], but optimized for the fact that [letter]
    /// is known to be a lowercase ASCII letter.
    inline bool equalsLetterIgnoreCase(uint8_t letter, uint8_t actual)
    {
      // assert(letter >= $a && letter <= $z);
      return (actual | asciiCaseBit) == letter;
    }

  }

}

#endif
