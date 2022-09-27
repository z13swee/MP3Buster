#ifndef UTILS_HPP
#define	UTILS_HPP

#include <vector>
#include <cstring>
#include <stdint.h>
#include <string>

#include <iostream>
#include <fstream>


// Default configuration
struct myConfig {
  unsigned longestWidthEntry = 0; // Used for outputing in batchmode
  int loglevel = 3;
  bool playback = false;
  bool stoponerror = true;
  bool batchmode = false;
  bool stopbatchmode = false;   // Used for forcing a stop on batchmode
  bool recursive = false;
  bool erroroutput = false;

  std::string configpath = ""; // Path to config file
  std::string erroroutputpath = "";

  std::ofstream* errorourputStream = nullptr;
};

// Utils
// unsigned ReadBigEndianValue(std::vector<uint8_t> data);
unsigned ReadBEValue(std::vector<uint8_t> buffer);

/*
 * Author: Floris Creyf
 * Date: May 2015
 */

/**
 * Assumes that end_bit is greater than start_bit and that the result is less than
 * 32 bits, length of an unsigned type.
 * @param buffer
 * @param start_bit
 * @param end_bit
 * @return {unsigned}
 */
unsigned get_bits(unsigned char *buffer, int start_bit, int end_bit);

/**
 * Uses get_bits() but mutates offset so that offset == offset + count.
 * @param buffer
 * @param offset
 * @param count
 */
unsigned get_bits_inc(unsigned char *buffer, int *offset, int count);

/** Puts four bytes into a single four byte integer type. */
int char_to_int(unsigned char *buffer);

unsigned short crc_update(unsigned short old_crc, unsigned char *data, unsigned int *begin_ptr, unsigned int length);

#endif	/* UTIL_H */
