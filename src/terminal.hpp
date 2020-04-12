#ifndef SASS_TERMINAL_H
#define SASS_TERMINAL_H

#ifdef _WIN32
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <windows.h>
#include <wincon.h>
#endif

#include "constants.hpp"

// Minimal terminal abstraction for cross compatibility.
// Its main purpose is to let us print stuff with colors.
namespace Terminal {

#ifdef _WIN32

  static size_t getWidth(bool error = false)
  {
    DWORD fd = error
      ? STD_ERROR_HANDLE
      : STD_OUTPUT_HANDLE;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    HANDLE handle = GetStdHandle(fd);
    GetConsoleScreenBufferInfo(handle, &csbi);
    return csbi.srWindow.Right - csbi.srWindow.Left;
  }

  static void print(const char* output, bool error = false)
  {

    if (output == 0) return;

    DWORD fd = error
      ? STD_ERROR_HANDLE
      : STD_OUTPUT_HANDLE;

    std::ostream& out = error ?
      std::cerr : std::cout;

    HANDLE handle = GetStdHandle(fd);
    CONSOLE_SCREEN_BUFFER_INFO info = {};
    GetConsoleScreenBufferInfo(handle, &info);
    WORD attribute = info.wAttributes;

    while (output[0] != '\0') {

      if (output[0] == '\x1B' && output[1] == '[') {

        output += 2;
        int fg = 0;
        int bg = 0;

        while (output[0] != '\0' && output[0] != ';' && output[0] != 'm') {
          fg *= 10;
          switch (*output) {
          case '0': fg += 0; break;
          case '1': fg += 1; break;
          case '2': fg += 2; break;
          case '3': fg += 3; break;
          case '4': fg += 4; break;
          case '5': fg += 5; break;
          case '6': fg += 6; break;
          case '7': fg += 7; break;
          case '8': fg += 8; break;
          case '9': fg += 9; break;
          default: break;
          }
          output += 1;
        }

        attribute &= ~(FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED);
        if (fg == 31 || fg == 33 || fg == 35 || fg == 37 || fg == 0) attribute |= FOREGROUND_RED;
        if (fg == 32 || fg == 33 || fg == 36 || fg == 37 || fg == 0) attribute |= FOREGROUND_GREEN;
        if (fg == 34 || fg == 35 || fg == 36 || fg == 37 || fg == 0) attribute |= FOREGROUND_BLUE;
        if (fg != 37 && fg != 0) attribute |= FOREGROUND_INTENSITY;
        if (output[0] == ';') {

          output += 1;

          while (output[0] != '\0' && output[0] != ';' && output[0] != 'm') {
            bg *= 10;
            switch (*output) {
            case '0': bg += 0; break;
            case '1': bg += 1; break;
            case '2': bg += 2; break;
            case '3': bg += 3; break;
            case '4': bg += 4; break;
            case '5': bg += 5; break;
            case '6': bg += 6; break;
            case '7': bg += 7; break;
            case '8': bg += 8; break;
            case '9': bg += 9; break;
            default: break;
            }
            output += 1;
          }

          attribute &= ~(BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_RED);
          if (bg == 31 || bg == 33 || bg == 35 || bg == 37) attribute |= BACKGROUND_RED;
          if (bg == 32 || bg == 33 || bg == 36 || bg == 37) attribute |= BACKGROUND_GREEN;
          if (bg == 34 || bg == 35 || bg == 36 || bg == 37) attribute |= BACKGROUND_BLUE;
          if (fg != 37 && fg != 0) attribute |= BACKGROUND_INTENSITY;

        }

        if (output[0] == 'm') output += 1;

        SetConsoleTextAttribute(handle, attribute);

      }
      else {
        out << output[0];
        output += 1;
      }

    }

  }

#else

  static void print(const char* output, bool error = false)
  {
    if (error) {
      std::cerr << output;
    }
    else {
      std::cout << output;
    }
  }

#endif

}

#endif
