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
#ifndef FNX_INFRA_ASCII_H_
#define FNX_INFRA_ASCII_H_


/* Flags for case-sensitivity. */
enum FNX_CASE {
	FNX_UPPERCASE        = 0,
	FNX_LOWERCASE        = 1
};


/*
 * Returns the null terminated string of lowercase ascii letters a-z.
 */
const char *fnx_ascii_lowercase_letters(void);

/*
    Returns a (null terminated) string of uppercase letters
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ".
*/
const char *fnx_ascii_uppercase_letters(void);

/*
    Returns a (null terminated) string of "abc...z" or
    "ABC...Z" letters.
*/
const char *fnx_ascii_abc(int icase);

/*
    Returns concatenation of lowercase_letters and
    uppercase_letters.
*/
const char *fnx_ascii_letters(void);

/*
    Returns a (null terminated) string of decimal digits
    ("0123456789").
*/
const char *fnx_ascii_digits(void);


/*
    Returns a (null terminated) string of octal digits ("01234567").
*/
const char *fnx_ascii_odigits(void);

/*
    Returns the string "0123456789ABCDEF".
*/
const char *fnx_ascii_xdigits_uppercase(void);

/*
    Returns the (null terminated) string "0123456789abcdef".
*/
const char *fnx_ascii_xdigits_lowercase(void);

/*
    Returns a (null terminated) string of hexadecimal digits.
*/
const char *fnx_ascii_xdigits(int icase);


/*
    Returns a (null terminated) string of punctuation characters:
    !"#$%&'()*+,-./:;<=>?@[\]^_`{|}~.
*/
const char *fnx_ascii_punctuation(void);

/*
    Returns a (null terminated) string of space, tab, return, linefeed,
    formfeed, and vertical-tab.
*/
const char *fnx_ascii_whitespaces(void);

/*
    Returns a (null terminated) string with single space character.
*/
const char *fnx_ascii_space(void);


#endif /* FNX_INFRA_ASCII_H_ */


