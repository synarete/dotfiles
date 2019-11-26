/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                             *
 *                            Funex File-System                                *
 *                                                                             *
 *  Copyright (C) 2014,2015 Synarete                                           *
 *                                                                             *
 *  Funex is a free software; you can redistribute it and/or modify it under   *
 *  the terms of the GNU General Public License as published by the Free       *
 *  Software Foundation, either version 3 of the License, or (at your option)  *
 *  any later version.                                                         *
 *                                                                             *
 *  Funex is distributed in the hope that it will be useful, but WITHOUT ANY   *
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS  *
 *  FOR A PARTICULAR PURPOSE. See the GNU General Public License for more      *
 *  details.                                                                   *
 *                                                                             *
 *  You should have received a copy of the GNU General Public License along    *
 *  with this program. If not, see <http://www.gnu.org/licenses/>              *
 *                                                                             *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
#include <fnxconfig.h>
#include <stdlib.h>
#include <string.h>

#include "infra-string.h"
#include "infra-substr.h"
#include "infra-ascii.h"

/*
  ASCII Table
  ===========

  Decimal   Octal   Hex    Binary       Value
  -------   -----   ----   --------     -----
   000      000     0x00   00000000      NUL    (Null char.)
   001      001     0x01   00000001      SOH    (Start of Header)
   002      002     0x02   00000010      STX    (Start of Text)
   003      003     0x03   00000011      ETX    (End of Text)
   004      004     0x04   00000100      EOT    (End of Transmission)
   005      005     0x05   00000101      ENQ    (Enquiry)
   006      006     0x06   00000110      ACK    (Acknowledgment)
   007      007     0x07   00000111      BEL    (Bell)
   008      010     0x08   00001000       BS    (Backspace)
   009      011     0x09   00001001       HT    (Horizontal Tab)
   010      012     0x0A   00001010       LF    (Line Feed)
   011      013     0x0B   00001011       VT    (Vertical Tab)
   012      014     0x0C   00001100       FF    (Form Feed)
   013      015     0x0D   00001101       CR    (Carriage Return)
   014      016     0x0E   00001110       SO    (Shift Out)
   015      017     0x0F   00001111       SI    (Shift In)
   016      020     0x10   00010000      DLE    (Data Link Escape)
   017      021     0x11   00010001      DC1    (XON) (Device Control 1)
   018      022     0x12   00010010      DC2          (Device Control 2)
   019      023     0x13   00010011      DC3    (XOFF)(Device Control 3)
   020      024     0x14   00010100      DC4          (Device Control 4)
   021      025     0x15   00010101      NAK    (Negative Acknowledgement)
   022      026     0x16   00010110      SYN    (Synchronous Idle)
   023      027     0x17   00010111      ETB    (End of Trans. Block)
   024      030     0x18   00011000      CAN    (Cancel)
   025      031     0x19   00011001       EM    (End of Medium)
   026      032     0x1A   00011010      SUB    (Substitute)
   027      033     0x1B   00011011      ESC    (Escape)
   028      034     0x1C   00011100       FS    (File Separator)
   029      035     0x1D   00011101       GS    (Group Separator)
   030      036     0x1E   00011110       RS    (Request to Send)
   031      037     0x1F   00011111       US    (Unit Separator)
   032      040     0x20   00100000       SP    (Space)
   033      041     0x21   00100001        !    (exclamation mark)
   034      042     0x22   00100010        "    (double quote)
   035      043     0x23   00100011        #    (number sign)
   036      044     0x24   00100100        $    (dollar sign)
   037      045     0x25   00100101        %    (percent)
   038      046     0x26   00100110        &    (ampersand)
   039      047     0x27   00100111        '    (single quote)
   040      050     0x28   00101000        (    (left/opening parenthesis)
   041      051     0x29   00101001        )    (right/closing parenthesis)
   042      052     0x2A   00101010        *    (asterisk)
   043      053     0x2B   00101011        +    (plus)
   044      054     0x2C   00101100        ,    (comma)
   045      055     0x2D   00101101        -    (minus or dash)
   046      056     0x2E   00101110        .    (dot)
   047      057     0x2F   00101111        /    (forward slash)
   048      060     0x30   00110000        0
   049      061     0x31   00110001        1
   050      062     0x32   00110010        2
   051      063     0x33   00110011        3
   052      064     0x34   00110100        4
   053      065     0x35   00110101        5
   054      066     0x36   00110110        6
   055      067     0x37   00110111        7
   056      070     0x38   00111000        8
   057      071     0x39   00111001        9
   058      072     0x3A   00111010        :    (colon)
   059      073     0x3B   00111011        ;    (semi-colon)
   060      074     0x3C   00111100        <    (less than)
   061      075     0x3D   00111101        =    (equal sign)
   062      076     0x3E   00111110        >    (greater than)
   063      077     0x3F   00111111        ?    (question mark)
   064      100     0x40   01000000        @    (AT symbol)
   065      101     0x41   01000001        A
   066      102     0x42   01000010        B
   067      103     0x43   01000011        C
   068      104     0x44   01000100        D
   069      105     0x45   01000101        E
   070      106     0x46   01000110        F
   071      107     0x47   01000111        G
   072      110     0x48   01001000        H
   073      111     0x49   01001001        I
   074      112     0x4A   01001010        J
   075      113     0x4B   01001011        K
   076      114     0x4C   01001100        L
   077      115     0x4D   01001101        M
   078      116     0x4E   01001110        N
   079      117     0x4F   01001111        O
   080      120     0x50   01010000        P
   081      121     0x51   01010001        Q
   082      122     0x52   01010010        R
   083      123     0x53   01010011        S
   084      124     0x54   01010100        T
   085      125     0x55   01010101        U
   086      126     0x56   01010110        V
   087      127     0x57   01010111        W
   088      130     0x58   01011000        X
   089      131     0x59   01011001        Y
   090      132     0x5A   01011010        Z
   091      133     0x5B   01011011        [    (left/opening bracket)
   092      134     0x5C   01011100        \    (back slash)
   093      135     0x5D   01011101        ]    (right/closing bracket)
   094      136     0x5E   01011110        ^    (caret/cirumflex)
   095      137     0x5F   01011111        _    (underscore)
   096      140     0x60   01100000        `
   097      141     0x61   01100001        a
   098      142     0x62   01100010        b
   099      143     0x63   01100011        c
   100      144     0x64   01100100        d
   101      145     0x65   01100101        e
   102      146     0x66   01100110        f
   103      147     0x67   01100111        g
   104      150     0x68   01101000        h
   105      151     0x69   01101001        i
   106      152     0x6A   01101010        j
   107      153     0x6B   01101011        k
   108      154     0x6C   01101100        l
   109      155     0x6D   01101101        m
   110      156     0x6E   01101110        n
   111      157     0x6F   01101111        o
   112      160     0x70   01110000        p
   113      161     0x71   01110001        q
   114      162     0x72   01110010        r
   115      163     0x73   01110011        s
   116      164     0x74   01110100        t
   117      165     0x75   01110101        u
   118      166     0x76   01110110        v
   119      167     0x77   01110111        w
   120      170     0x78   01111000        x
   121      171     0x79   01111001        y
   122      172     0x7A   01111010        z
   123      173     0x7B   01111011        {    (left/opening brace)
   124      174     0x7C   01111100        |    (vertical bar)
   125      175     0x7D   01111101        }    (right/closing brace)
   126      176     0x7E   01111110        ~    (tilde)
   127      177     0x7F   01111111      DEL    (delete)

*/

/*
 * Constant string-literals:
 */

const char *fnx_ascii_lowercase_letters(void)
{
	static const char s_abc[] = {
		/* a     b     c     d     e     f     g     h  */
		0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
		/* i     j     k     l     m     n     o     p  */
		0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70,
		/* q     r     s     t     u     v     w     x  */
		0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
		/* y     z   NUL */
		0x79, 0x7a, 0x00
	};

	return s_abc;
}

/* Returns a (null terminated) string of uppercase letters
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ". */
const char *fnx_ascii_uppercase_letters(void)
{
	static const char s_abc[] = {
		/* A     B     C     D     E     F     G     H  */
		0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
		/* I     J     K     L     M     N     O     P  */
		0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50,
		/* Q     R     S     T     U     V     W     X  */
		0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
		/* Y     Z   NUL  */
		0x59, 0x5A, 0x00
	};

	return s_abc;
}

/* Returns a (null terminated) string of "abc...z" or
    "ABC...Z" letters. */
const char *fnx_ascii_abc(int icase)
{
	return ((icase == FNX_UPPERCASE) ?
	        fnx_ascii_uppercase_letters() :
	        fnx_ascii_lowercase_letters());
}

/* Returns concatenation of lowercase_letters and
    uppercase_letters. */
const char *fnx_ascii_letters(void)
{
	static const char s_letters[] = {
		/* a     b     c     d     e     f     g     h  */
		0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
		/* i     j     k     l     m     n     o     p  */
		0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70,
		/* q     r     s     t     u     v     w     x  */
		0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
		/* y     z  */
		0x79, 0x7a,
		/* A     B     C     D     E     F     G     H  */
		0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
		/* I     J     K     L     M     N     O     P  */
		0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50,
		/* Q     R     S     T     U     V     W     X  */
		0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
		/* Y     Z   NUL    */
		0x59, 0x5A, 0x00
	};

	return s_letters;
}

/* Returns a (null terminated) string of decimal digits
    ("0123456789"). */
const char *fnx_ascii_digits(void)
{
	static const char s_digs[] = {
		/* 0     1     2     3     4     5     6     7     8     9   NUL */
		0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x00
	};

	return s_digs;
}

/* Returns a (null terminated) string of octal digits ("01234567"). */
const char *fnx_ascii_odigits(void)
{
	static const char s_odigs[] = {
		/* 0     1     2     3     4     5     6     7   NUL  */
		0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x00
	};

	return s_odigs;
}

/* Returns the string "0123456789ABCDEF". */
const char *fnx_ascii_xdigits_uppercase(void)
{
	static const char s_xdigs[] = {
		/* 0     1     2     3     4     5     6     7     8     9  */
		0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
		/* A     B     C     D     E     F   NUL */
		0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x00
	};

	return s_xdigs;
}

/* Returns the (null terminated) string "0123456789abcdef". */
const char *fnx_ascii_xdigits_lowercase(void)
{
	static const char s_xdigs[] = {
		/* 0     1     2     3     4     5     6     7     8     9 */
		0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
		/* a     b     c     d     e     f   NUL */
		0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x00
	};

	return s_xdigs;
}

/* Returns a (null terminated) string of hexadecimal digits. */
const char *fnx_ascii_xdigits(int icase)
{
	return ((icase == FNX_UPPERCASE) ?
	        fnx_ascii_xdigits_uppercase() : fnx_ascii_xdigits_lowercase());
}


/*
 * Returns a (null terminated) string of punctuation characters:
 * !"#$%&'()*+,-./:;<=>?@[\]^_`{|}~.
 */
const char *fnx_ascii_punctuation(void)
{
	static const char s_punct[] = {
		/* !     "     #     $     %     &     '     (     )     *      */
		0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a,
		/* +     ,     -     .     /     :     ;     <     =     >      */
		0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e,
		/* ?     @     [     \     ]     ^     _     `     {     |      */
		0x3f, 0x40, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60, 0x7b, 0x7c,
		/* }     ~   NUL    */
		0x7d, 0x7e, 0x00
	};

	return s_punct;
}

/* Returns a (null terminated) string of space, tab, return, linefeed,
    formfeed, and vertical-tab. */
const char *fnx_ascii_whitespaces(void)
{
	static const char s_whites[] = {
		/*SP    HT    CR    LF    FF    VT   NUL    */
		0x20, 0x09, 0x0d, 0x0a, 0x0c, 0x0b, 0x00
	};

	return s_whites;
}

/* Returns a (null terminated) string with single space character. */
const char *fnx_ascii_space(void)
{
	static const char s_space[] = { 0x20, 0x00 };
	return s_space;
}



