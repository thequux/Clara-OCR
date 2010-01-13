/*
 *  File:    obd.c
 *  Purpose: Decode a barcode recognized by OCR.
 *  Machine:                      OS:
 *  Author:  John Wehle           Date: February 14, 2003
 *
 *  Copyright (c)  2003  Feith Systems and Software, Inc.
 *                      All Rights Reserved
 */

/*

   obs. integrated into Clara OCR (with permission) by
   Ricardo Ueda Karpischek, feb2003.

*/


/*
 *  $Log$
 */

static char rcsid[] = "$Header$";


#include <ctype.h>
#include <limits.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

static char *MyName = "obd";


struct barcode
  {
    char *scanline;
    size_t scanline_length;
    char *encoded;
    size_t encoded_length;
    char *decoded;
    size_t decoded_length;
  };

enum bc_type { bc_unknown_type, bc_char_type, bc_numeric_type,
               bc_numeric2_type, bc_charset_type, bc_null_type,
               bc_unsupported_type };

struct barcode_code {
  char *encoding;
  unsigned int nbits;
  struct {
    char c;
    enum bc_type type;
    } sets[3];
  struct {
    int start;
    int stop;
    } flags;
  };

struct barcode_symbology {
  unsigned int nwidths;
  struct barcode_code *bcp;
  int checksum_required;
  int (*verify_checksum) (struct barcode *bp, struct barcode_code **codes);
  };


/*
 * Codabar table based on:
 *
 *   http://www.barcodeisland.com/codabar.phtml
 *
 * with local corrections for B, C, and +.
 */

static struct barcode_code codabar_codes[] = {
  { "101010011", 9, { { '0', bc_char_type } }, { 0, 0 } },
  { "101011001", 9, { { '1', bc_char_type } }, { 0, 0 } },
  { "101001011", 9, { { '2', bc_char_type } }, { 0, 0 } },
  { "110010101", 9, { { '3', bc_char_type } }, { 0, 0 } },
  { "101101001", 9, { { '4', bc_char_type } }, { 0, 0 } },
  { "110101001", 9, { { '5', bc_char_type } }, { 0, 0 } },
  { "100101011", 9, { { '6', bc_char_type } }, { 0, 0 } },
  { "100101101", 9, { { '7', bc_char_type } }, { 0, 0 } },
  { "100110101", 9, { { '8', bc_char_type } }, { 0, 0 } },
  { "110100101", 9, { { '9', bc_char_type } }, { 0, 0 } },
  { "101001101", 9, { { '-', bc_char_type } }, { 0, 0 } },
  { "101100101", 9, { { '$', bc_char_type } }, { 0, 0 } },
  { "1101011011", 10, { { ':', bc_char_type } }, { 0, 0 } },
  { "1101101011", 10, { { '/', bc_char_type } }, { 0, 0 } },
  { "1101101101", 10, { { '.', bc_char_type } }, { 0, 0 } },
  { "1011001001", 10, { { 'A', bc_char_type } }, { 1, 1 } },
  { "1001001011", 10, { { 'B', bc_char_type } }, { 2, 2 } },
  { "1010010011", 10, { { 'C', bc_char_type } }, { 3, 3 } },
  { "1010011001", 10, { { 'D', bc_char_type } }, { 4, 4 } },
  { "1011011011", 10, { { '+', bc_char_type } }, { 0, 0 } },
  { NULL, 0, { { 0, 0 } }, { 0, 0 } } };

static struct barcode_symbology codabar = { 2, codabar_codes, 0, NULL };


/*
 * Code 3 of 9 table based on:
 *
 *   http://www.barcodeisland.com/code39.phtml
 */

static struct barcode_code c39_codes[] = {
  { "101001101101", 12, { { '0', bc_char_type } }, { 0, 0 } },
  { "110100101011", 12, { { '1', bc_char_type } }, { 0, 0 } },
  { "101100101011", 12, { { '2', bc_char_type } }, { 0, 0 } },
  { "110110010101", 12, { { '3', bc_char_type } }, { 0, 0 } },
  { "101001101011", 12, { { '4', bc_char_type } }, { 0, 0 } },
  { "110100110101", 12, { { '5', bc_char_type } }, { 0, 0 } },
  { "101100110101", 12, { { '6', bc_char_type } }, { 0, 0 } },
  { "101001011011", 12, { { '7', bc_char_type } }, { 0, 0 } },
  { "110100101101", 12, { { '8', bc_char_type } }, { 0, 0 } },
  { "101100101101", 12, { { '9', bc_char_type } }, { 0, 0 } },
  { "110101001011", 12, { { 'A', bc_char_type } }, { 0, 0 } },
  { "101101001011", 12, { { 'B', bc_char_type } }, { 0, 0 } },
  { "110110100101", 12, { { 'C', bc_char_type } }, { 0, 0 } },
  { "101011001011", 12, { { 'D', bc_char_type } }, { 0, 0 } },
  { "110101100101", 12, { { 'E', bc_char_type } }, { 0, 0 } },
  { "101101100101", 12, { { 'F', bc_char_type } }, { 0, 0 } },
  { "101010011011", 12, { { 'G', bc_char_type } }, { 0, 0 } },
  { "110101001101", 12, { { 'H', bc_char_type } }, { 0, 0 } },
  { "101101001101", 12, { { 'I', bc_char_type } }, { 0, 0 } },
  { "101011001101", 12, { { 'J', bc_char_type } }, { 0, 0 } },
  { "110101010011", 12, { { 'K', bc_char_type } }, { 0, 0 } },
  { "101101010011", 12, { { 'L', bc_char_type } }, { 0, 0 } },
  { "110110101001", 12, { { 'M', bc_char_type } }, { 0, 0 } },
  { "101011010011", 12, { { 'N', bc_char_type } }, { 0, 0 } },
  { "110101101001", 12, { { 'O', bc_char_type } }, { 0, 0 } },
  { "101101101001", 12, { { 'P', bc_char_type } }, { 0, 0 } },
  { "101010110011", 12, { { 'Q', bc_char_type } }, { 0, 0 } },
  { "110101011001", 12, { { 'R', bc_char_type } }, { 0, 0 } },
  { "101101011001", 12, { { 'S', bc_char_type } }, { 0, 0 } },
  { "101011011001", 12, { { 'T', bc_char_type } }, { 0, 0 } },
  { "110010101011", 12, { { 'U', bc_char_type } }, { 0, 0 } },
  { "100110101011", 12, { { 'V', bc_char_type } }, { 0, 0 } },
  { "110011010101", 12, { { 'W', bc_char_type } }, { 0, 0 } },
  { "100101101011", 12, { { 'X', bc_char_type } }, { 0, 0 } },
  { "110010110101", 12, { { 'Y', bc_char_type } }, { 0, 0 } },
  { "100110110101", 12, { { 'Z', bc_char_type } }, { 0, 0 } },
  { "100101011011", 12, { { '-', bc_char_type } }, { 0, 0 } },
  { "110010101101", 12, { { '.', bc_char_type } }, { 0, 0 } },
  { "100110101101", 12, { { ' ', bc_char_type } }, { 0, 0 } },
  { "100100100101", 12, { { '$', bc_char_type } }, { 0, 0 } },
  { "100100101001", 12, { { '/', bc_char_type } }, { 0, 0 } },
  { "100101001001", 12, { { '+', bc_char_type } }, { 0, 0 } },
  { "101001001001", 12, { { '%', bc_char_type } }, { 0, 0 } },
  { "100101101101", 12, { { 0, bc_null_type } }, { 1, 1 } },
  { NULL, 0, { { 0, 0 } }, { 0, 0 } } };

static int
c39_verify_checksum (struct barcode *bp, struct barcode_code **codes)
  {
  unsigned int c;
  unsigned int checksum;
  size_t idx;
  struct barcode_code *bcp;

  checksum = 0;
  idx = 1;

  while ((bcp = codes[idx++]) != NULL)
    {
      if (bcp->flags.stop || ! codes[idx] )
        return -1;

      c = (bcp - c39_codes);

      /*
       * The checksum appears right before the stop.
       */

      if (codes[idx]->flags.stop)
        {
          checksum %= 43;
          return c == checksum ? 0 : -1;
        }

      checksum += c;
    }

  return -1;
  }

static struct barcode_symbology c39 = { 2, c39_codes, 0, c39_verify_checksum };


/*
 * Code 128 table based on:
 *
 *   http://www.barcodeisland.com/code128.phtml
 *
 * Currently FNC1-4 are not supported.
 */

static struct barcode_code c128_codes[] = {
  { "11011001100", 11, { { ' ', bc_char_type },
                         { ' ', bc_char_type },
                         { 0, bc_numeric_type } }, { 0, 0 } },
  { "11001101100", 11, { { '!', bc_char_type },
                         { '!', bc_char_type },
                         { 1, bc_numeric_type } }, { 0, 0 } },
  { "11001100110", 11, { { '"', bc_char_type },
                         { '"', bc_char_type },
                         { 2, bc_numeric_type } }, { 0, 0 } },
  { "10010011000", 11, { { '#', bc_char_type },
                         { '#', bc_char_type },
                         { 3, bc_numeric_type } }, { 0, 0 } },
  { "10010001100", 11, { { '$', bc_char_type },
                         { '$', bc_char_type },
                         { 4, bc_numeric_type } }, { 0, 0 } },
  { "10001001100", 11, { { '%', bc_char_type },
                         { '%', bc_char_type },
                         { 5, bc_numeric_type } }, { 0, 0 } },
  { "10011001000", 11, { { '&', bc_char_type },
                         { '&', bc_char_type },
                         { 6, bc_numeric_type } }, { 0, 0 } },
  { "10011000100", 11, { { '\'', bc_char_type },
                         { '\'', bc_char_type },
                         { 7, bc_numeric_type } }, { 0, 0 } },
  { "10001100100", 11, { { '(', bc_char_type },
                         { '(', bc_char_type },
                         { 8, bc_numeric_type } }, { 0, 0 } },
  { "11001001000", 11, { { ')', bc_char_type },
                         { ')', bc_char_type },
                         { 9, bc_numeric_type } }, { 0, 0 } },
  { "11001000100", 11, { { '*', bc_char_type },
                         { '*', bc_char_type },
                         { 10, bc_numeric_type } }, { 0, 0 } },
  { "11000100100", 11, { { '+', bc_char_type },
                         { '+', bc_char_type },
                         { 11, bc_numeric_type } }, { 0, 0 } },
  { "10110011100", 11, { { ',', bc_char_type },
                         { ',', bc_char_type },
                         { 12, bc_numeric_type } }, { 0, 0 } },
  { "10011011100", 11, { { '-', bc_char_type },
                         { '-', bc_char_type },
                         { 13, bc_numeric_type } }, { 0, 0 } },
  { "10011001110", 11, { { '.', bc_char_type },
                         { '.', bc_char_type },
                         { 14, bc_numeric_type } }, { 0, 0 } },
  { "10111001100", 11, { { '/', bc_char_type },
                         { '/', bc_char_type },
                         { 15, bc_numeric_type } }, { 0, 0 } },
  { "10011101100", 11, { { '0', bc_char_type },
                         { '0', bc_char_type },
                         { 16, bc_numeric_type } }, { 0, 0 } },
  { "10011100110", 11, { { '1', bc_char_type },
                         { '1', bc_char_type },
                         { 17, bc_numeric_type } }, { 0, 0 } },
  { "11001110010", 11, { { '2', bc_char_type },
                         { '2', bc_char_type },
                         { 18, bc_numeric_type } }, { 0, 0 } },
  { "11001011100", 11, { { '3', bc_char_type },
                         { '3', bc_char_type },
                         { 19, bc_numeric_type } }, { 0, 0 } },
  { "11001001110", 11, { { '4', bc_char_type },
                         { '4', bc_char_type },
                         { 20, bc_numeric_type } }, { 0, 0 } },
  { "11011100100", 11, { { '5', bc_char_type },
                         { '5', bc_char_type },
                         { 21, bc_numeric_type } }, { 0, 0 } },
  { "11001110100", 11, { { '6', bc_char_type },
                         { '6', bc_char_type },
                         { 22, bc_numeric_type } }, { 0, 0 } },
  { "11101101110", 11, { { '7', bc_char_type },
                         { '7', bc_char_type },
                         { 23, bc_numeric_type } }, { 0, 0 } },
  { "11101001100", 11, { { '8', bc_char_type },
                         { '8', bc_char_type },
                         { 24, bc_numeric_type } }, { 0, 0 } },
  { "11100101100", 11, { { '9', bc_char_type },
                         { '9', bc_char_type },
                         { 25, bc_numeric_type } }, { 0, 0 } },
  { "11100100110", 11, { { ':', bc_char_type },
                         { ':', bc_char_type },
                         { 26, bc_numeric_type } }, { 0, 0 } },
  { "11101100100", 11, { { ';', bc_char_type },
                         { ';', bc_char_type },
                         { 27, bc_numeric_type } }, { 0, 0 } },
  { "11100110100", 11, { { '<', bc_char_type },
                         { '<', bc_char_type },
                         { 28, bc_numeric_type } }, { 0, 0 } },
  { "11100110010", 11, { { '=', bc_char_type },
                         { '=', bc_char_type },
                         { 29, bc_numeric_type } }, { 0, 0 } },
  { "11011011000", 11, { { '>', bc_char_type },
                         { '>', bc_char_type },
                         { 30, bc_numeric_type } }, { 0, 0 } },
  { "11011000110", 11, { { '?', bc_char_type },
                         { '?', bc_char_type },
                         { 31, bc_numeric_type } }, { 0, 0 } },
  { "11000110110", 11, { { '@', bc_char_type },
                         { '@', bc_char_type },
                         { 32, bc_numeric_type } }, { 0, 0 } },
  { "10100011000", 11, { { 'A', bc_char_type },
                         { 'A', bc_char_type },
                         { 33, bc_numeric_type } }, { 0, 0 } },
  { "10001011000", 11, { { 'B', bc_char_type },
                         { 'B', bc_char_type },
                         { 34, bc_numeric_type } }, { 0, 0 } },
  { "10001000110", 11, { { 'C', bc_char_type },
                         { 'C', bc_char_type },
                         { 35, bc_numeric_type } }, { 0, 0 } },
  { "10110001000", 11, { { 'D', bc_char_type },
                         { 'D', bc_char_type },
                         { 36, bc_numeric_type } }, { 0, 0 } },
  { "10001101000", 11, { { 'E', bc_char_type },
                         { 'E', bc_char_type },
                         { 37, bc_numeric_type } }, { 0, 0 } },
  { "10001100010", 11, { { 'F', bc_char_type },
                         { 'F', bc_char_type },
                         { 38, bc_numeric_type } }, { 0, 0 } },
  { "11010001000", 11, { { 'G', bc_char_type },
                         { 'G', bc_char_type },
                         { 39, bc_numeric_type } }, { 0, 0 } },
  { "11000101000", 11, { { 'H', bc_char_type },
                         { 'H', bc_char_type },
                         { 40, bc_numeric_type } }, { 0, 0 } },
  { "11000100010", 11, { { 'I', bc_char_type },
                         { 'I', bc_char_type },
                         { 41, bc_numeric_type } }, { 0, 0 } },
  { "10110111000", 11, { { 'J', bc_char_type },
                         { 'J', bc_char_type },
                         { 42, bc_numeric_type } }, { 0, 0 } },
  { "10110001110", 11, { { 'K', bc_char_type },
                         { 'K', bc_char_type },
                         { 43, bc_numeric_type } }, { 0, 0 } },
  { "10001101110", 11, { { 'L', bc_char_type },
                         { 'L', bc_char_type },
                         { 44, bc_numeric_type } }, { 0, 0 } },
  { "10111011000", 11, { { 'M', bc_char_type },
                         { 'M', bc_char_type },
                         { 45, bc_numeric_type } }, { 0, 0 } },
  { "10111000110", 11, { { 'N', bc_char_type },
                         { 'N', bc_char_type },
                         { 46, bc_numeric_type } }, { 0, 0 } },
  { "10001110110", 11, { { 'O', bc_char_type },
                         { 'O', bc_char_type },
                         { 47, bc_numeric_type } }, { 0, 0 } },
  { "11101110110", 11, { { 'P', bc_char_type },
                         { 'P', bc_char_type },
                         { 48, bc_numeric_type } }, { 0, 0 } },
  { "11010001110", 11, { { 'Q', bc_char_type },
                         { 'Q', bc_char_type },
                         { 49, bc_numeric_type } }, { 0, 0 } },
  { "11000101110", 11, { { 'R', bc_char_type },
                         { 'R', bc_char_type },
                         { 50, bc_numeric_type } }, { 0, 0 } },
  { "11011101000", 11, { { 'S', bc_char_type },
                         { 'S', bc_char_type },
                         { 51, bc_numeric_type } }, { 0, 0 } },
  { "11011100010", 11, { { 'T', bc_char_type },
                         { 'T', bc_char_type },
                         { 52, bc_numeric_type } }, { 0, 0 } },
  { "11011101110", 11, { { 'U', bc_char_type },
                         { 'U', bc_char_type },
                         { 53, bc_numeric_type } }, { 0, 0 } },
  { "11101011000", 11, { { 'V', bc_char_type },
                         { 'V', bc_char_type },
                         { 54, bc_numeric_type } }, { 0, 0 } },
  { "11101000110", 11, { { 'W', bc_char_type },
                         { 'W', bc_char_type },
                         { 55, bc_numeric_type } }, { 0, 0 } },
  { "11100010110", 11, { { 'X', bc_char_type },
                         { 'X', bc_char_type },
                         { 56, bc_numeric_type } }, { 0, 0 } },
  { "11101101000", 11, { { 'Y', bc_char_type },
                         { 'Y', bc_char_type },
                         { 57, bc_numeric_type } }, { 0, 0 } },
  { "11101100010", 11, { { 'Z', bc_char_type },
                         { 'Z', bc_char_type },
                         { 58, bc_numeric_type } }, { 0, 0 } },
  { "11100011010", 11, { { '[', bc_char_type },
                         { '[', bc_char_type },
                         { 59, bc_numeric_type } }, { 0, 0 } },
  { "11101111010", 11, { { '\\', bc_char_type },
                         { '\\', bc_char_type },
                         { 60, bc_numeric_type } }, { 0, 0 } },
  { "11001000010", 11, { { ']', bc_char_type },
                         { ']', bc_char_type },
                         { 61, bc_numeric_type } }, { 0, 0 } },
  { "11110001010", 11, { { ' ', bc_char_type },
                         { ' ', bc_char_type },
                         { 62, bc_numeric_type } }, { 0, 0 } },
  { "10100110000", 11, { { '_', bc_char_type },
                         { '_', bc_char_type },
                         { 63, bc_numeric_type } }, { 0, 0 } },
  { "10100001100", 11, { { 0, bc_char_type },
                         { '`', bc_char_type },
                         { 64, bc_numeric_type } }, { 0, 0 } },
  { "10010110000", 11, { { 1, bc_char_type },
                         { 'a', bc_char_type },
                         { 65, bc_numeric_type } }, { 0, 0 } },
  { "10010000110", 11, { { 2, bc_char_type },
                         { 'b', bc_char_type },
                         { 66, bc_numeric_type } }, { 0, 0 } },
  { "10000101100", 11, { { 3, bc_char_type },
                         { 'c', bc_char_type },
                         { 67, bc_numeric_type } }, { 0, 0 } },
  { "10000100110", 11, { { 4, bc_char_type },
                         { 'd', bc_char_type },
                         { 68, bc_numeric_type } }, { 0, 0 } },
  { "10110010000", 11, { { 5, bc_char_type },
                         { 'e', bc_char_type },
                         { 69, bc_numeric_type } }, { 0, 0 } },
  { "10110000100", 11, { { 6, bc_char_type },
                         { 'f', bc_char_type },
                         { 70, bc_numeric_type } }, { 0, 0 } },
  { "10011010000", 11, { { 7, bc_char_type },
                         { 'g', bc_char_type },
                         { 71, bc_numeric_type } }, { 0, 0 } },
  { "10011000010", 11, { { 8, bc_char_type },
                         { 'h', bc_char_type },
                         { 72, bc_numeric_type } }, { 0, 0 } },
  { "10000110100", 11, { { 9, bc_char_type },
                         { 'i', bc_char_type },
                         { 73, bc_numeric_type } }, { 0, 0 } },
  { "10000110010", 11, { { 10, bc_char_type },
                         { 'j', bc_char_type },
                         { 74, bc_numeric_type } }, { 0, 0 } },
  { "11000010010", 11, { { 11, bc_char_type },
                         { 'k', bc_char_type },
                         { 75, bc_numeric_type } }, { 0, 0 } },
  { "11001010000", 11, { { 12, bc_char_type },
                         { 'l', bc_char_type },
                         { 76, bc_numeric_type } }, { 0, 0 } },
  { "11110111010", 11, { { 13, bc_char_type },
                         { 'm', bc_char_type },
                         { 77, bc_numeric_type } }, { 0, 0 } },
  { "11000010100", 11, { { 14, bc_char_type },
                         { 'n', bc_char_type },
                         { 78, bc_numeric_type } }, { 0, 0 } },
  { "10001111010", 11, { { 15, bc_char_type },
                         { 'o', bc_char_type },
                         { 79, bc_numeric_type } }, { 0, 0 } },
  { "10100111100", 11, { { 16, bc_char_type },
                         { 'p', bc_char_type },
                         { 80, bc_numeric_type } }, { 0, 0 } },
  { "10010111100", 11, { { 17, bc_char_type },
                         { 'q', bc_char_type },
                         { 81, bc_numeric_type } }, { 0, 0 } },
  { "10010011110", 11, { { 18, bc_char_type },
                         { 'r', bc_char_type },
                         { 82, bc_numeric_type } }, { 0, 0 } },
  { "10111100100", 11, { { 19, bc_char_type },
                         { 's', bc_char_type },
                         { 83, bc_numeric_type } }, { 0, 0 } },
  { "10011110100", 11, { { 20, bc_char_type },
                         { 't', bc_char_type },
                         { 84, bc_numeric_type } }, { 0, 0 } },
  { "10011110010", 11, { { 21, bc_char_type },
                         { 'u', bc_char_type },
                         { 85, bc_numeric_type } }, { 0, 0 } },
  { "11110100100", 11, { { 22, bc_char_type },
                         { 'v', bc_char_type },
                         { 86, bc_numeric_type } }, { 0, 0 } },
  { "11110010100", 11, { { 23, bc_char_type },
                         { 'w', bc_char_type },
                         { 87, bc_numeric_type } }, { 0, 0 } },
  { "11110010010", 11, { { 24, bc_char_type },
                         { 'x', bc_char_type },
                         { 88, bc_numeric_type } }, { 0, 0 } },
  { "11011011110", 11, { { 25, bc_char_type },
                         { 'y', bc_char_type },
                         { 89, bc_numeric_type } }, { 0, 0 } },
  { "11011110110", 11, { { 26, bc_char_type },
                         { 'z', bc_char_type },
                         { 90, bc_numeric_type } }, { 0, 0 } },
  { "11110110110", 11, { { 27, bc_char_type },
                         { '{', bc_char_type },
                         { 91, bc_numeric_type } }, { 0, 0 } },
  { "10101111000", 11, { { 28, bc_char_type },
                         { '|', bc_char_type },
                         { 92, bc_numeric_type } }, { 0, 0 } },
  { "10100011110", 11, { { 29, bc_char_type },
                         { '}', bc_char_type },
                         { 93, bc_numeric_type } }, { 0, 0 } },
  { "10001011110", 11, { { 30, bc_char_type },
                         { '~', bc_char_type },
                         { 94, bc_numeric_type } }, { 0, 0 } },
  { "10111101000", 11, { { 31, bc_char_type },
                         { 127, bc_char_type },
                         { 95, bc_numeric_type } }, { 0, 0 } },
  { "10111100010", 11, { { 0, bc_unsupported_type },
                         { 0, bc_unsupported_type },
                         { 96, bc_numeric_type } }, { 0, 0 } },
  { "11110101000", 11, { { 0, bc_unsupported_type },
                         { 0, bc_unsupported_type },
                         { 97, bc_numeric_type } }, { 0, 0 } },
  { "11110100010", 11, { { 0, bc_unsupported_type },
                         { 0, bc_unsupported_type },
                         { 98, bc_numeric_type } }, { 0, 0 } },
  { "10111011110", 11, { { 2, bc_charset_type },
                         { 2, bc_charset_type },
                         { 99, bc_numeric_type } }, { 0, 0 } },
  { "10111101110", 11, { { 1, bc_charset_type },
                         { 0, bc_unsupported_type },
                         { 1, bc_charset_type } }, { 0, 0 } },
  { "11101011110", 11, { { 0, bc_unsupported_type },
                         { 0, bc_charset_type },
                         { 0, bc_charset_type } }, { 0, 0 } },
  { "11110101110", 11, { { 0, bc_unsupported_type },
                         { 0, bc_unsupported_type },
                         { 0, bc_unsupported_type } }, { 0, 0 } },
  { "11010000100", 11, { { 0, bc_charset_type },
                         { 0, bc_charset_type },
                         { 0, bc_charset_type } }, { 1, 0 } },
  { "11010010000", 11, { { 1, bc_charset_type },
                         { 1, bc_charset_type },
                         { 1, bc_charset_type } }, { 1, 0 } },
  { "11010011100", 11, { { 2, bc_charset_type },
                         { 2, bc_charset_type },
                         { 2, bc_charset_type } }, { 1, 0 } },
  { "11000111010", 11, { { 0, bc_null_type },
                         { 0, bc_null_type },
                         { 0, bc_null_type } }, { 0, 1 } },
  { NULL, 0, { { 0, 0 } }, { 0, 0 } } };

static int
c128_verify_checksum (struct barcode *bp, struct barcode_code **codes)
  {
  unsigned int c;
  unsigned int checksum;
  size_t idx;
  struct barcode_code *bcp;

  checksum = *codes - c128_codes;
  idx = 1;

  while ((bcp = codes[idx++]) != NULL)
    {
      if (bcp->flags.stop || ! codes[idx] )
        return -1;

      c = (bcp - c128_codes);

      /*
       * The checksum appears right before the stop.
       */

      if (codes[idx]->flags.stop)
        {
          checksum %= 103;
          return c == checksum ? 0 : -1;
        }

      checksum += c * (idx - 1);
    }

  return -1;
  }

static struct barcode_symbology c128 = { 4, c128_codes, 1,
                                         c128_verify_checksum };


/*
 * Interleaved 2 of 5 table based on:
 *
 *   http://www.barcodeisland.com/int2of5.phtml
 */

static struct barcode_code i2o5_codes[] = {
  { "1010", 4, { { 0, bc_null_type } }, { 1, 0 } },
  { "1101", 4, { { 0, bc_null_type } }, { 0, 1 } },
  { "10101100110010", 14, { { 0, bc_numeric2_type } }, { 0, 0 } },
  { "10010110110100", 14, { { 1, bc_numeric2_type } }, { 0, 0 } },
  { "10100110110100", 14, { { 2, bc_numeric2_type } }, { 0, 0 } },
  { "10010011011010", 14, { { 3, bc_numeric2_type } }, { 0, 0 } },
  { "10101100110100", 14, { { 4, bc_numeric2_type } }, { 0, 0 } },
  { "10010110011010", 14, { { 5, bc_numeric2_type } }, { 0, 0 } },
  { "10100110011010", 14, { { 6, bc_numeric2_type } }, { 0, 0 } },
  { "10101101100100", 14, { { 7, bc_numeric2_type } }, { 0, 0 } },
  { "10010110110010", 14, { { 8, bc_numeric2_type } }, { 0, 0 } },
  { "10100110110010", 14, { { 9, bc_numeric2_type } }, { 0, 0 } },
  { "11010100100110", 14, { { 10, bc_numeric2_type } }, { 0, 0 } },
  { "11001010101100", 14, { { 11, bc_numeric2_type } }, { 0, 0 } },
  { "11010010101100", 14, { { 12, bc_numeric2_type } }, { 0, 0 } },
  { "11001001010110", 14, { { 13, bc_numeric2_type } }, { 0, 0 } },
  { "11010100101100", 14, { { 14, bc_numeric2_type } }, { 0, 0 } },
  { "11001010010110", 14, { { 15, bc_numeric2_type } }, { 0, 0 } },
  { "11010010010110", 14, { { 16, bc_numeric2_type } }, { 0, 0 } },
  { "11010101001100", 14, { { 17, bc_numeric2_type } }, { 0, 0 } },
  { "11001010100110", 14, { { 18, bc_numeric2_type } }, { 0, 0 } },
  { "11010010100110", 14, { { 19, bc_numeric2_type } }, { 0, 0 } },
  { "10110100100110", 14, { { 20, bc_numeric2_type } }, { 0, 0 } },
  { "10011010101100", 14, { { 21, bc_numeric2_type } }, { 0, 0 } },
  { "10110010101100", 14, { { 22, bc_numeric2_type } }, { 0, 0 } },
  { "10011001010110", 14, { { 23, bc_numeric2_type } }, { 0, 0 } },
  { "10110100101100", 14, { { 24, bc_numeric2_type } }, { 0, 0 } },
  { "10011010010110", 14, { { 25, bc_numeric2_type } }, { 0, 0 } },
  { "10110010010110", 14, { { 26, bc_numeric2_type } }, { 0, 0 } },
  { "10110101001100", 14, { { 27, bc_numeric2_type } }, { 0, 0 } },
  { "10011010100110", 14, { { 28, bc_numeric2_type } }, { 0, 0 } },
  { "10110010100110", 14, { { 29, bc_numeric2_type } }, { 0, 0 } },
  { "11011010010010", 14, { { 30, bc_numeric2_type } }, { 0, 0 } },
  { "11001101010100", 14, { { 31, bc_numeric2_type } }, { 0, 0 } },
  { "11011001010100", 14, { { 32, bc_numeric2_type } }, { 0, 0 } },
  { "11001100101010", 14, { { 33, bc_numeric2_type } }, { 0, 0 } },
  { "11011010010100", 14, { { 34, bc_numeric2_type } }, { 0, 0 } },
  { "11001101001010", 14, { { 35, bc_numeric2_type } }, { 0, 0 } },
  { "11011001001010", 14, { { 36, bc_numeric2_type } }, { 0, 0 } },
  { "11011010100100", 14, { { 37, bc_numeric2_type } }, { 0, 0 } },
  { "11001101010010", 14, { { 38, bc_numeric2_type } }, { 0, 0 } },
  { "11011001010010", 14, { { 39, bc_numeric2_type } }, { 0, 0 } },
  { "10101100100110", 14, { { 40, bc_numeric2_type } }, { 0, 0 } },
  { "10010110101100", 14, { { 41, bc_numeric2_type } }, { 0, 0 } },
  { "10100110101100", 14, { { 42, bc_numeric2_type } }, { 0, 0 } },
  { "10010011010110", 14, { { 43, bc_numeric2_type } }, { 0, 0 } },
  { "10101100101100", 14, { { 44, bc_numeric2_type } }, { 0, 0 } },
  { "10010110010110", 14, { { 45, bc_numeric2_type } }, { 0, 0 } },
  { "10100110010110", 14, { { 46, bc_numeric2_type } }, { 0, 0 } },
  { "10101101001100", 14, { { 47, bc_numeric2_type } }, { 0, 0 } },
  { "10010110100110", 14, { { 48, bc_numeric2_type } }, { 0, 0 } },
  { "10100110100110", 14, { { 49, bc_numeric2_type } }, { 0, 0 } },
  { "11010110010010", 14, { { 50, bc_numeric2_type } }, { 0, 0 } },
  { "11001011010100", 14, { { 51, bc_numeric2_type } }, { 0, 0 } },
  { "11010011010100", 14, { { 52, bc_numeric2_type } }, { 0, 0 } },
  { "11001001101010", 14, { { 53, bc_numeric2_type } }, { 0, 0 } },
  { "11010110010100", 14, { { 54, bc_numeric2_type } }, { 0, 0 } },
  { "11001011001010", 14, { { 55, bc_numeric2_type } }, { 0, 0 } },
  { "11010011001010", 14, { { 56, bc_numeric2_type } }, { 0, 0 } },
  { "11010110100100", 14, { { 57, bc_numeric2_type } }, { 0, 0 } },
  { "11001011010010", 14, { { 58, bc_numeric2_type } }, { 0, 0 } },
  { "11010011010010", 14, { { 59, bc_numeric2_type } }, { 0, 0 } },
  { "10110110010010", 14, { { 60, bc_numeric2_type } }, { 0, 0 } },
  { "10011011010100", 14, { { 61, bc_numeric2_type } }, { 0, 0 } },
  { "10110011010100", 14, { { 62, bc_numeric2_type } }, { 0, 0 } },
  { "10011001101010", 14, { { 63, bc_numeric2_type } }, { 0, 0 } },
  { "10110110010100", 14, { { 64, bc_numeric2_type } }, { 0, 0 } },
  { "10011011001010", 14, { { 65, bc_numeric2_type } }, { 0, 0 } },
  { "10110011001010", 14, { { 66, bc_numeric2_type } }, { 0, 0 } },
  { "10110110100100", 14, { { 67, bc_numeric2_type } }, { 0, 0 } },
  { "10011011010010", 14, { { 68, bc_numeric2_type } }, { 0, 0 } },
  { "10110011010010", 14, { { 69, bc_numeric2_type } }, { 0, 0 } },
  { "10101001100110", 14, { { 70, bc_numeric2_type } }, { 0, 0 } },
  { "10010101101100", 14, { { 71, bc_numeric2_type } }, { 0, 0 } },
  { "10100101101100", 14, { { 72, bc_numeric2_type } }, { 0, 0 } },
  { "10010010110110", 14, { { 73, bc_numeric2_type } }, { 0, 0 } },
  { "10101001101100", 14, { { 74, bc_numeric2_type } }, { 0, 0 } },
  { "10010100110110", 14, { { 75, bc_numeric2_type } }, { 0, 0 } },
  { "10100100110110", 14, { { 76, bc_numeric2_type } }, { 0, 0 } },
  { "10101011001100", 14, { { 77, bc_numeric2_type } }, { 0, 0 } },
  { "10010101100110", 14, { { 78, bc_numeric2_type } }, { 0, 0 } },
  { "10100101100110", 14, { { 79, bc_numeric2_type } }, { 0, 0 } },
  { "11010100110010", 14, { { 80, bc_numeric2_type } }, { 0, 0 } },
  { "11001010110100", 14, { { 81, bc_numeric2_type } }, { 0, 0 } },
  { "11010010110100", 14, { { 82, bc_numeric2_type } }, { 0, 0 } },
  { "11001001011010", 14, { { 83, bc_numeric2_type } }, { 0, 0 } },
  { "11010100110100", 14, { { 84, bc_numeric2_type } }, { 0, 0 } },
  { "11001010011010", 14, { { 85, bc_numeric2_type } }, { 0, 0 } },
  { "11010010011010", 14, { { 86, bc_numeric2_type } }, { 0, 0 } },
  { "11010101100100", 14, { { 87, bc_numeric2_type } }, { 0, 0 } },
  { "11001010110010", 14, { { 88, bc_numeric2_type } }, { 0, 0 } },
  { "11010010110010", 14, { { 89, bc_numeric2_type } }, { 0, 0 } },
  { "10110100110010", 14, { { 90, bc_numeric2_type } }, { 0, 0 } },
  { "10011010110100", 14, { { 91, bc_numeric2_type } }, { 0, 0 } },
  { "10110010110100", 14, { { 92, bc_numeric2_type } }, { 0, 0 } },
  { "10011001011010", 14, { { 93, bc_numeric2_type } }, { 0, 0 } },
  { "10110100110100", 14, { { 94, bc_numeric2_type } }, { 0, 0 } },
  { "10011010011010", 14, { { 95, bc_numeric2_type } }, { 0, 0 } },
  { "10110010011010", 14, { { 96, bc_numeric2_type } }, { 0, 0 } },
  { "10110101100100", 14, { { 97, bc_numeric2_type } }, { 0, 0 } },
  { "10011010110010", 14, { { 98, bc_numeric2_type } }, { 0, 0 } },
  { "10110010110010", 14, { { 99, bc_numeric2_type } }, { 0, 0 } },
  { NULL, 0, { { 0, 0 } }, { 0, 0 } } };

static int
i2o5_verify_checksum (struct barcode *bp, struct barcode_code **codes)
  {
  unsigned int c;
  unsigned int checksum;
  size_t idx;
  struct barcode_code *bcp;

  checksum = 0;
  idx = 1;

  while ((bcp = codes[idx++]) != NULL)
    {
      if (bcp->flags.stop || ! codes[idx] )
        return -1;

      c = bcp->sets[0].c;

      /*
       * The checksum appears right before the stop.
       */

      if (codes[idx]->flags.stop)
        {
          bp->decoded[bp->decoded_length++] = '0' + c / 10;

          checksum += (c % 10) + (c / 10) * 3;
          checksum %= 10;
          return checksum == 0 ? 0 : -1;
        }

      checksum += (c % 10) + (c / 10) * 3;
    }

  return -1;
  }

static struct barcode_symbology i2o5 = { 2, i2o5_codes, 0,
                                         i2o5_verify_checksum };


static struct barcode_symbology *symbologies[] = {
  &codabar, &c39, &c128, &i2o5, NULL };


/*
 * Read the next barcode from the file.
 *
 * File format:
 *
 *   The width of a bar is indicated by a number from 1 to 4.
 *   A narrow space is assumed after each bar.  Wider spaces
 *   are created by the addition of a space character.
 *   For example the code39 start character is represented as:
 *
 *     1 1221
 *
 *   Multiple barcodes may be present (one per line).
 */

static int
read_ocr (FILE *fp, struct barcode *bp)
  {
  char *sp;
  int c;
  long offset;
  size_t length;

  bp->scanline = NULL;
  bp->scanline_length = 0;

  /*
   * Skip leading whitespace.
   */

  while ((c = fgetc (fp)) != EOF)
    {
      if (! isspace (c))
        {
          ungetc (c, fp);
          break;
        }
    }

  /*
   * Determine the length of the next line.
   */

  offset = ftell (fp);

  while ((c = fgetc (fp)) != EOF)
    if (c == '\n')
      break;

  if (c == EOF)
    return 0;

  length = ftell (fp) - offset;
  fseek (fp, offset, SEEK_SET);

  /*
   * Each character in the file can result in up to
   * four bars plus a space.  The width of a bar or
   * space has arbitrarily been set to seven pixels.
   */

  if (! (bp->scanline = malloc (length * 5 * 7 + 1)) )
    return -1;

  sp = bp->scanline;

  /*
   * Read the barcode.
   */

  while ((c = fgetc (fp)) != EOF)
    {
      if (c == '\r' || c == '\n')
        break;

      switch (c)
        {
        case '4':
          memcpy (sp, "XXXXXXX", 7);
          sp += 7;
        /* FALLTHROUGH */

        case '3':
          memcpy (sp, "XXXXXXX", 7);
          sp += 7;
        /* FALLTHROUGH */

        case '2':
          memcpy (sp, "XXXXXXX", 7);
          sp += 7;
        /* FALLTHROUGH */

        case '1':
          memcpy (sp, "XXXXXXX", 7);
          sp += 7;
          memcpy (sp, "       ", 7);
          sp += 7;
          break;

        case ' ':
          memcpy (sp, "       ", 7);
          sp += 7;
          break;

        default:
          free (bp->scanline);
          bp->scanline = NULL;
          return -1;
        /* NOTREACHED */
          break;
        }
    }

  if (c == EOF)
    {
      free (bp->scanline);
      bp->scanline = NULL;
      return -1;
    }

  /*
   * Trim trailing spaces.
   */

  while (sp != bp->scanline && *(sp - 1) == ' ')
    sp--;

  *sp = '\0';

  if (! *bp->scanline )
    {
      free (bp->scanline);
      bp->scanline = NULL;
      return -1;
    }

  bp->scanline_length = sp - bp->scanline;

  return 1;
  }


/*
 * Read the next barcode from the file.
 *
 * File format:
 *
 *   A black pixel is indicated by X.  A bar consists of a
 *   series of black pixels.  A white pixel is indicated by
 *   a space character.  A space is indicated by a series of
 *   white pixels.  For example the code39 start character is
 *   represented as:
 *
 *     XXXXX          XXXXX     XXXXXXXXXX     XXXXXXXXXX     XXXXX
 *
 *   Multiple barcodes may be present (one per line).
 */

static int
read_scanline (FILE *fp, struct barcode *bp)
  {
  char *sp;
  int c;
  long offset;
  size_t length;

  bp->scanline = NULL;
  bp->scanline_length = 0;

  /*
   * Skip leading whitespace.
   */

  while ((c = fgetc (fp)) != EOF)
    {
      if (! isspace (c))
        {
          ungetc (c, fp);
          break;
        }
    }

  /*
   * Determine the length of the next line.
   */

  offset = ftell (fp);

  while ((c = fgetc (fp)) != EOF)
    if (c == '\n')
      break;

  if (c == EOF)
    return 0;

  length = ftell (fp) - offset;
  fseek (fp, offset, SEEK_SET);

  /*
   * Read the scanline.
   */

  if (! (bp->scanline = malloc (length + 1)) )
    return -1;

  sp = bp->scanline;

  while ((c = fgetc (fp)) != EOF)
    {
      if (c == '\r' || c == '\n')
        break;

      switch (c)
        {
        case 'X':
        case ' ':
          *sp++ = c;
          break;

        default:
          free (bp->scanline);
          bp->scanline = NULL;
          return -1;
        /* NOTREACHED */
          break;
        }
    }

  if (c == EOF)
    {
      free (bp->scanline);
      bp->scanline = NULL;
      return -1;
    }

  /*
   * Trim trailing spaces.
   */

  while (sp != bp->scanline && *(sp - 1) == ' ')
    sp--;

  *sp = '\0';

  if (! *bp->scanline )
    {
      free (bp->scanline);
      bp->scanline = NULL;
      return -1;
    }

  bp->scanline_length = sp - bp->scanline;

  return 1;
  }


/*
 * Convert the scanline to an encoded barcode.
 *
 * Scanline format:
 *
 *   A black pixel is indicated by X.  A bar consists of a
 *   series of black pixels.  A white pixel is indicated by
 *   a space character.  A space is indicated by a series of
 *   white pixels.  For example the code39 start character is
 *   represented as:
 *
 *     XXXXX          XXXXX     XXXXXXXXXX     XXXXXXXXXX     XXXXX
 *
 *   The series of pixels is terminated by the null character.
 */

static int
convert_scanline (struct barcode *bp, struct barcode_symbology *bsp)
  {
  char *ep;
  char *sp;
  char *start;
  int c;
  unsigned int nwidths[2];
  unsigned int distance;
  unsigned int d0;
  unsigned int d1;
  unsigned int multiplier;
  unsigned int o0;
  unsigned int o1;
  unsigned int width;
  size_t idx;

  struct width
    {
      unsigned int min;
      unsigned int max;
      struct width *next;
    };

  struct width *head[2];
  struct width *cp;
  struct width *np;
  struct width *wp;
  struct width m0;
  struct width m1;

  bp->encoded = NULL;
  bp->encoded_length = 0;

  /*
   * Each pixel can result in up to one bar or a space.
   */

  if (! (bp->encoded = malloc (bp->scanline_length + 1)) )
    return -1;

  /*
   * Measure the different widths.
   */

  head[0] = head[1] = NULL;
  nwidths[0] = nwidths[1] = 0;
  sp = bp->scanline;

  while (*sp == ' ')
    sp++;

  while ((c = *sp) != '\0')
    {
      start = sp;

      while (c == *++sp)
        ;

      if (! (wp = (struct width *)calloc (1, sizeof (struct width))) )
        {
          for (idx = 0; idx < 2; idx++)
            for (np = NULL, cp = head[idx]; cp; cp = np)
              {
                np = cp->next;
                free (cp);
              }
          free (bp->encoded);
          bp->encoded = NULL;
          return -1;
        }

      idx = c == ' ' ? 0 : 1;
      width = sp - start;

      wp->min = wp->max = width;

      if (! head[idx] || head[idx]->min > width)
        {
          wp->next = head[idx];
          head[idx] = wp;
          nwidths[idx]++;
          continue;
        }

      for (cp = head[idx]; cp->min != width; cp = cp->next)
        if (cp->min < width && (! cp->next || cp->next->min > width))
          break;

      if (cp->min == width)
        {
          free (wp);
          continue;
        }

      wp->next = cp->next;
      cp->next = wp;
      nwidths[idx]++;
    }

  /*
   * Consolidate widths by merging "adjacent" widths togeither.
   */

  for (idx = 0; idx < 2; idx++)
    while (nwidths[idx] > bsp->nwidths)
      {

        /*
         * Find the smallest distance.
         */

        distance = UINT_MAX;

        for (cp = head[idx]; cp && cp->next; cp = cp->next)
          if ((cp->next->min - cp->max) < distance)
            distance = cp->next->min - cp->max;

        /*
         * Merge.
         */

        for (cp = head[idx]; cp && cp->next; cp = cp->next)
          while (cp->next && (cp->next->min - cp->max) <= distance)
            {
              np = cp->next;
              cp->max = cp->next->max;
              cp->next = np->next;
              free (np);
              nwidths[idx]--;
            }
      }

  /*
   * Generate placeholders for missing widths.
   * It's assumed that a narrow bar and narrow
   * space are always present (which is currently
   * true due to the start / stop codes for the
   * supported symbologies).
   */

  for (idx = 0; idx < 2; idx++)
    if (nwidths[idx] < bsp->nwidths)
      {
        multiplier = 2;

        for (cp = head[idx]; cp && cp->next; cp = cp->next)
          {

            /*
             * The widths are ordered from smallest to largest.
             * Each width is an multiple of the smallest and
             * there should be no missing widths.
             *
             * Check for a missing width by seeing if the next
             * width divided by (multiplier + 1) matches the
             * smallest width better than the next width divided
             * by multipler.  The critera used is the distance
             * separating the widths and the amount of overlap.
             */

            d0 = 0;
            o0 = 0;

            m0.min = cp->next->min / multiplier;
            m0.max = cp->next->max / multiplier;

            if (m0.max < head[idx]->min)
              d0 = head[idx]->min - m0.max;
            else if (m0.min > head[idx]->max)
              d0 = m0.min - head[idx]->max;
            else if (m0.min < head[idx]->min && m0.max >= head[idx]->min)
              o0 = m0.max - head[idx]->min;
            else if (m0.min <= head[idx]->max && m0.max > head[idx]->max)
              o0 = head[idx]->max - m0.min;
            else if (m0.min >= head[idx]->min && m0.max <= head[idx]->max)
              o0 = m0.max - m0.min;
            else
              o0 = head[idx]->max - head[idx]->min;

            d1 = 0;
            o1 = 0;

            m1.min = cp->next->min / (multiplier + 1);
            m1.max = cp->next->max / (multiplier + 1);

            if (m1.max < head[idx]->min)
              d1 = head[idx]->min - m1.max;
            else if (m1.min > head[idx]->max)
              d1 = m1.min - head[idx]->max;
            else if (m1.min < head[idx]->min && m1.max >= head[idx]->min)
              o1 = m1.max - head[idx]->min;
            else if (m1.min <= head[idx]->max && m1.max > head[idx]->max)
              o1 = head[idx]->max - m1.min;
            else if (m1.min >= head[idx]->min && m1.max <= head[idx]->max)
              o1 = m1.max - m1.min;
            else
              o1 = head[idx]->max - head[idx]->min;

            if (d1 < d0 || o1 > o0)
              {
                if (! (wp = (struct width *)calloc (1,
                                                    sizeof (struct width))) )
                  {
                    for (idx = 0; idx < 2; idx++)
                      for (np = NULL, cp = head[idx]; cp; cp = np)
                        {
                          np = cp->next;
                          free (cp);
                        }
                    free (bp->encoded);
                    bp->encoded = NULL;
                    return -1;
                  }

                wp->min = wp->max = 0;
                wp->next = cp->next;
                cp->next = wp;

                nwidths[idx]++;
              }

            multiplier++;
          }
      }

  /*
   * Convert the scanline into a barcode.
   */

  ep = bp->encoded;
  sp = bp->scanline;

  while (*sp == ' ')
    sp++;

  while ((c = *sp) != '\0')
    {
      start = sp;

      while (c == *++sp)
        ;

      idx = c == ' ' ? 0 : 1;
      width = sp - start;

      for (cp = head[idx]; cp && cp->min <= width; cp = cp->next)
        *ep++ = '0' + idx;
    }

  /*
   * Trim trailing spaces.
   */

  while (ep != bp->encoded && *(ep - 1) == '0')
    ep--;

  *ep = '\0';

  /*
   * Free memory which is no longer needed.
   */

  for (idx = 0; idx < 2; idx++)
    for (np = NULL, cp = head[idx]; cp; cp = np)
      {
        np = cp->next;
        free (cp);
      }
  head[0] = head[1] = NULL;

  if (! *bp->encoded )
    {
      free (bp->encoded);
      bp->encoded = NULL;
      return -1;
    }

  bp->encoded_length = ep - bp->encoded;

  return 1;
  }


/*
 * Decoded the barcode using the desired symbology.
 */

static int
decode_symbology (struct barcode *bp, struct barcode_symbology *bsp,
                  int with_checksum)
  {
  char *symbol;
  int charset;
  int start;
  size_t idx;
  size_t previous_decoded_length;
  size_t symbol_length;
  struct barcode_code **codes;
  struct barcode_code *bcp;
  struct barcode_code *tbcp;

  bp->decoded = NULL;
  bp->decoded_length = 0;

  if (! (codes = (struct barcode_code **)
                 calloc (bp->encoded_length, sizeof (struct barcode *))) )
    return -1;

  /*
   * The decoded message can't possibly be longer
   * than the encoded version.
   */

  if (! (bp->decoded = malloc (bp->encoded_length + 1)) )
    {
      free (codes);
      return -1;
    }

  symbol = bp->encoded;
  symbol_length = 0;

  bcp = bsp->bcp;
  charset = 0;
  start = 0;

  idx = 0;
  previous_decoded_length = 0;

  while (symbol[symbol_length])
    {

      symbol_length++;

      /*
       * See if we have a known symbol.
       */

      while (bcp->encoding && symbol_length >= bcp->nbits)
        {
          if (strncmp (bcp->encoding, symbol, bcp->nbits) == 0)
            break;
          bcp++;
        }

      if (! bcp->encoding)
        break;

      if (symbol_length < bcp->nbits)
        continue;

      /*
       * Only match start if it's unique or at the
       * beginning of the barcode.  Only match stop
       * if it's unique or at the end of the barcode.
       */

      if ((bcp->flags.start && symbol != bp->encoded)
          || (bcp->flags.stop && symbol != &bp->encoded[bp->encoded_length
                                                        - symbol_length]))
        {
          for (tbcp = bcp + 1; tbcp->encoding; tbcp++)
            if (strncmp (tbcp->encoding, symbol, symbol_length) == 0)
              break;

          if (tbcp->encoding)
            {
              bcp = tbcp;
              continue;
            }
        }

      /*
       * Require a start symbol and don't allow
       * them to appear in the middle of a barcode.
       */

      if ((! start && ! bcp->flags.start)
          || (start && bcp->flags.start && ! bcp->flags.stop))
        break;

      /*
       * Handle the symbol.
       */

      switch (bcp->sets[charset].type)
        {
          case bc_char_type:
            previous_decoded_length = bp->decoded_length;

            bp->decoded[bp->decoded_length++] = bcp->sets[charset].c;
            break;

          case bc_numeric_type:
            previous_decoded_length = bp->decoded_length;

            if ((unsigned int)bcp->sets[charset].c > 10)
              bp->decoded[bp->decoded_length++]
                = '0' + ((unsigned int)bcp->sets[charset].c / 10);
            bp->decoded[bp->decoded_length++]
              = '0' + ((unsigned int)bcp->sets[charset].c % 10);
            break;

          case bc_numeric2_type:
            previous_decoded_length = bp->decoded_length;

            bp->decoded[bp->decoded_length++]
              = '0' + ((unsigned int)bcp->sets[charset].c / 10);
            bp->decoded[bp->decoded_length++]
              = '0' + ((unsigned int)bcp->sets[charset].c % 10);
            break;

          case bc_charset_type:
            charset = bcp->sets[charset].c;
            break;

          case bc_null_type:
            break;

          case bc_unsupported_type:
            previous_decoded_length = bp->decoded_length;

            bp->decoded[bp->decoded_length++] = '?';
            break;

          default:
            free (codes);

            free (bp->decoded);
            bp->decoded = NULL;
            bp->decoded_length = 0;

            return -1;
          /* NOTREACHED */
            break;
        }

      codes[idx++] = bcp;

      if (! start)
        start = 1;
      else if (bcp->flags.stop)
        {
          if (with_checksum || bsp->checksum_required)
            {
              bp->decoded_length = previous_decoded_length;

              if ((*bsp->verify_checksum)(bp, codes) < 0)
                {
                  free (codes);

                  free (bp->decoded);
                  bp->decoded = NULL;
                  bp->decoded_length = 0;

                  return -1;
                }
            }

          bp->decoded[bp->decoded_length] = '\0';

          free (codes);

          return 0;
        }

      /*
       * Start the next symbol.
       */

      symbol += bcp->nbits;
      symbol_length = 0;

      bcp = bsp->bcp;

      /*
       * Skip any whitespace which is separating the symbols.
       */

      while (*symbol == '0')
        symbol++;
    }

  free (codes);

  free (bp->decoded);
  bp->decoded = NULL;
  bp->decoded_length = 0;

  return -1;
  }


/*
 * Decoded the barcode.
 */

static int
decode (struct barcode *bp, struct barcode_symbology *bsp,
                  int with_checksum)
  {
  char c;
  size_t idx;
  struct barcode barcode;
  struct barcode_symbology **bspp;

  /*
   * Use a specific symbology if it was supplied.
   */

  if (bsp)
    {
      if (convert_scanline (bp, bsp) < 0)
        return -1;

      if (decode_symbology (bp, bsp, with_checksum) >= 0)
        return 0;

      /*
       * Try reversing the encoded barcode in case it was backwards
       * (i.e. the page was scanned upside down).
       */

      for (idx = 0; idx < bp->encoded_length / 2; idx++)
        {
          c = bp->encoded[idx];
          bp->encoded[idx] = bp->encoded[bp->encoded_length - 1 - idx];
          bp->encoded[bp->encoded_length - 1 - idx] = c;
        }

      if (decode_symbology (bp, bsp, with_checksum) >= 0)
        return 0;

      free (bp->encoded);
      bp->encoded = NULL;
      bp->encoded_length = 0;

      return -1;
    }

  /*
   * Try each symbology.
   */

  barcode.encoded = NULL;
  barcode.encoded_length = 0;

  barcode.decoded = NULL;
  barcode.decoded_length = 0;

  for (bspp = symbologies; *bspp; bspp++)
    {
      if (convert_scanline (bp, *bspp) < 0)
        {
          if (barcode.decoded)
            {
              free (barcode.encoded);
              free (barcode.decoded);
            }

          return -1;
        }

      if (decode_symbology (bp, *bspp, with_checksum) >= 0)
        {
          if (barcode.decoded)
            {
              free (barcode.encoded);
              free (barcode.decoded);

              free (bp->encoded);
              bp->encoded = NULL;
              bp->encoded_length = 0;

              free (bp->decoded);
              bp->decoded = NULL;
              bp->decoded_length = 0;

              return -1;
            }

          barcode.encoded = bp->encoded;
          barcode.encoded_length = bp->encoded_length;

          barcode.decoded = bp->decoded;
          barcode.decoded_length = bp->decoded_length;

          continue;
        }

      /*
       * Try reversing the encoded barcode in case it was backwards
       * (i.e. the page was scanned upside down).
       */

      for (idx = 0; idx < bp->encoded_length / 2; idx++)
        {
          c = bp->encoded[idx];
          bp->encoded[idx] = bp->encoded[bp->encoded_length - 1 - idx];
          bp->encoded[bp->encoded_length - 1 - idx] = c;
        }

      if (decode_symbology (bp, *bspp, with_checksum) >= 0)
        {
          if (barcode.decoded)
            {
              free (barcode.encoded);
              free (barcode.decoded);

              free (bp->encoded);
              bp->encoded = NULL;
              bp->encoded_length = 0;

              free (bp->decoded);
              bp->decoded = NULL;
              bp->decoded_length = 0;

              return -1;
            }

          barcode.encoded = bp->encoded;
          barcode.encoded_length = bp->encoded_length;

          barcode.decoded = bp->decoded;
          barcode.decoded_length = bp->decoded_length;

          continue;
        }

      free (bp->encoded);
    }

  bp->encoded = barcode.encoded;
  bp->encoded_length = barcode.encoded_length;

  bp->decoded = barcode.decoded;
  bp->decoded_length = barcode.decoded_length;

  return bp->decoded ? 0 : -1;
  }


static void
print_usage ()
  {

  fprintf (stderr, "Usage: %s [options] [input_files]\n"
                   "where options are:\n"
                   "-c            checksum present\n"
                   "-i format     input format (ocr, scanline)\n"
                   "-s symbology  barcode type (codabar, c39, c128, i2o5)\n",
                   MyName);
  }


int
obd_main (int argc, char **argv)
  {
  FILE *fp;
  char *filename;
  enum { unknown_if, ocr_if, scanline_if } input_format;
  int c;
  int result;
  int with_checksum;
  struct barcode barcode;
  struct barcode_symbology *bsp;
  struct stat statbuf;

  bsp = NULL;
  input_format = ocr_if;
  with_checksum = 0;

  /*
     as claraocr already processed the true options, need to
     reset the counter, hope this is portable.
  */
  optind = 1;

  while ((c = getopt (argc, argv, "ci:s:")) != EOF)
    switch (c)
      {
        case 'c':
          with_checksum = 1;
          break;

        case 'i':
          if (strcmp (optarg, "ocr") == 0)
            input_format = ocr_if;
          else if (strcmp (optarg, "scanline") == 0)
            input_format = scanline_if;
          else
            {
              print_usage ();
              exit (1);
            }
          break;

        case 's':
          if (strcmp (optarg, "codabar") == 0)
            bsp = &codabar;
          else if (strcmp (optarg, "c39") == 0)
            bsp = &c39;
          else if (strcmp (optarg, "c128") == 0)
            bsp = &c128;
          else if (strcmp (optarg, "i2o5") == 0)
            bsp = &i2o5;
          else
            {
              print_usage ();
              exit (1);
            }
          break;

        default:
          print_usage ();
          exit (1);
        /* NOTREACHED */
          break;
      }

  filename = "stdin";
  fp = stdin;

  do
    {
      if (optind < argc)
        {
          filename = argv[optind];
          if (! (fp = fopen (filename, "r")) )
            {
              fprintf (stderr, "%s: can't open <%s>\n", MyName, filename);
              perror (MyName);
              exit (1);
            }
        }

      if (fstat (fileno (fp), &statbuf) < 0
          || !S_ISREG (statbuf.st_mode))
        {
          fprintf (stderr, "%s: <%s> must be a regular file.\n",
                   MyName, filename);
          if (fp != stdin)
            fclose (fp);
          exit (1);
        }

      for ( ; ; )
        {
          switch (input_format)
            {
              case ocr_if:
                result = read_ocr (fp, &barcode);
                break;

              case scanline_if:
                result = read_scanline (fp, &barcode);
                break;

              default:
                fprintf (stderr, "Internal error.\n");
                abort ();
              /* NOTREACHED */
                break;
            }

          if (result == 0)
            break;
          else if (result < 0)
            {
              fprintf (stderr, "%s: <%s> contains an invalid barcode.\n",
                       MyName, filename);
              if (fp != stdin)
                fclose (fp);
              exit (1);
            }
            
          if (decode (&barcode, bsp, with_checksum) < 0)
            {
              fprintf (stderr, "%s: <%s> contains an invalid barcode.\n",
                       MyName, filename);
              free (barcode.scanline);
              if (fp != stdin)
                fclose (fp);
              exit (1);
            }

          fwrite (barcode.decoded, barcode.decoded_length, 1, stdout);

          fputc ('\n', stdout);

          free (barcode.scanline);
          free (barcode.encoded);
          free (barcode.decoded);
        }

      if (fp != stdin)
        fclose (fp);

    } while (++optind < argc);

  exit (0);
  }
