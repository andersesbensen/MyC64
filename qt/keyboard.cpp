/*
 * keyboard.cpp
 *
 *  Created on: Mar 4, 2017
 *      Author: aes
 */

#include<Qt>

/*
 Insert/Delete Return  cursor left/right F7  F1  F3  F5  cursor up/down
 3 W A 4 Z S E left Shift
 5 R D 6 C F T X
 7 Y G 8 B H U V
 9 I J 0 M K O N
 + (plus)  P L – (minus) . (period)  : (colon) @ (at)  , (comma)
 £ (pound) * (asterisk)  ; (semicolon) Clear/Home  right Shift (Shift Lock)  = (equal) ↑ (up arrow)  / (slash)
 1 ← (left arrow)  Control 2 Space Commodore Q Run/Stop
 */


int
key_matrix(int keycode)
{
  switch (keycode)
  {
  //row 0
  case Qt::Key_Backspace: //Insert/Delete
    return 000;
  case Qt::Key_Return: //Return
    return 001;
  case Qt::Key_Right: //cursor left/right
    return 002;
  case Qt::Key_F7: //F7
    return 003;
  case Qt::Key_F1: //F1
    return 004;
  case Qt::Key_F3: //F3
    return 005;
  case Qt::Key_F5: //F5
    return 006;
  case Qt::Key_Up: //cursor up/down
    return 007;
  //row1
  case '3':
    return 010;
  case 'W':
    return 011;
  case 'A':
    return 012;
  case '4':
    return 013;
  case 'Z':
    return 014;
  case 'S':
    return 015;
  case 'E':
    return 016;
  case Qt::Key_Shift: // left shift
    return 017;

    //row2
  case '5':
    return 020;
  case 'R':
    return 021;
  case 'D':
    return 022;
  case '6':
    return 023;
  case 'C':
    return 024;
  case 'F':
    return 025;
  case 'T':
    return 026;
  case 'X':
    return 027;

    //row3
  case '7':
    return 030;
  case 'Y':
    return 031;
  case 'G':
    return 032;
  case '8':
    return 033;
  case 'B':
    return 034;
  case 'H':
    return 035;
  case 'U':
    return 036;
  case 'V':
    return 037;

    //row4
  case '9':
    return 040;
  case 'I':
    return 041;
  case 'J':
    return 042;
  case '0':
    return 043;
  case 'M':
    return 044;
  case 'K':
    return 045;
  case 'O':
    return 046;
  case 'N':
    return 047;

    //row5
  case '+': //(plus)
    return 050;
  case 'P':
    return 051;
  case 'L':
    return 052;
  case '-': //(minus)
    return 053;
  case '.': //(period)
    return 054;
  case ':': //(colon)
    return 055;
  case '@': //(at)
    return 056;
  case ',': // comma
    return 057;

    //row6
  case '$': //(pound)
    return 060;
  case '\'': //asterisk
    return 061;
  case ';': // (semicolon)
    return 062;
  case Qt::Key_Escape: //( Clear/Home )
    return 063;
  case Qt::Key_CapsLock: // //right Shift (Shift Lock)
    return 064;
  case '=': //(equal)
    return 065;
  case Qt::Key_Bar: //(up arrow)
    return 066;
  case '/': // (slash)
    return 067;

    //row7
  case '1': //
    return 070;
  case Qt::Key_Left: // (left arrow)
    return 071;
  case Qt::Key_Control: // (Control)
    return 072;
  case '2': //
    return 073;
  case ' ': // Space
    return 074;
  case Qt::Key_ApplicationLeft: //(Commodore)
    return 075;
  case 'Q':
    return 076;
  case Qt::Key_ApplicationRight: // (Run/Stop)
    return 077;
  default:
    return 0xFF;
  }
}
