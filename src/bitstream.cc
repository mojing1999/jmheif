/*
 * HEIF codec.
 * Copyright (c) 2017 struktur AG, Dirk Farin <farin@struktur.de>
 *
 * This file is part of libheif.
 *
 * libheif is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * libheif is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libheif.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "bitstream.h"

#include <assert.h>

#define MAX_UVLC_LEADING_ZEROS 20

using namespace heif;


uint8_t BitstreamRange::read8()
{
  if (!read(1)) {
    return 0;
  }

  uint8_t buf;

  std::istream* istr = get_istream();
  istr->read((char*)&buf,1);

  if (istr->fail()) {
    set_eof_reached();
    return 0;
  }

  return buf;
}


uint16_t BitstreamRange::read16()
{
  if (!read(2)) {
    return 0;
  }

  uint8_t buf[2];

  std::istream* istr = get_istream();
  istr->read((char*)buf,2);

  if (istr->fail()) {
    set_eof_reached();
    return 0;
  }

  return static_cast<uint16_t>((buf[0]<<8) | (buf[1]));
}


uint32_t BitstreamRange::read32()
{
  if (!read(4)) {
    return 0;
  }

  uint8_t buf[4];

  std::istream* istr = get_istream();
  istr->read((char*)buf,4);

  if (istr->fail()) {
    set_eof_reached();
    return 0;
  }

  return ((buf[0]<<24) |
          (buf[1]<<16) |
          (buf[2]<< 8) |
          (buf[3]));
}


std::string BitstreamRange::read_string()
{
  std::string str;

  if (eof()) {
    return "";
  }

  for (;;) {
    if (!read(1)) {
      return std::string();
    }

    std::istream* istr = get_istream();
    int c = istr->get();

    if (istr->fail()) {
      set_eof_reached();
      return std::string();
    }

    if (c==0) {
      break;
    }
    else {
      str += (char)c;
    }
  }

  return str;
}




BitReader::BitReader(const uint8_t* buffer, int len)
{
  data = buffer;
  data_length = len;
  bytes_remaining = len;

  nextbits=0;
  nextbits_cnt=0;

  refill();
}

int BitReader::get_bits(int n)
{
  if (nextbits_cnt < n) {
    refill();
  }

  uint64_t val = nextbits;
  val >>= 64-n;

  nextbits <<= n;
  nextbits_cnt -= n;

  return (int)val;
}

int  BitReader::get_bits_fast(int n)
{
  assert(nextbits_cnt >= n);

  uint64_t val = nextbits;
  val >>= 64-n;

  nextbits <<= n;
  nextbits_cnt -= n;

  return (int)val;
}

int  BitReader::peek_bits(int n)
{
  if (nextbits_cnt < n) {
    refill();
  }

  uint64_t val = nextbits;
  val >>= 64-n;

  return (int)val;
}

void BitReader::skip_bits(int n)
{
  if (nextbits_cnt < n) {
    refill();
  }

  nextbits <<= n;
  nextbits_cnt -= n;
}

void BitReader::skip_bits_fast(int n)
{
  nextbits <<= n;
  nextbits_cnt -= n;
}

void BitReader::skip_to_byte_boundary()
{
  int nskip = (nextbits_cnt & 7);

  nextbits <<= nskip;
  nextbits_cnt -= nskip;
}

bool BitReader::get_uvlc(int* value)
{
  int num_zeros=0;

  while (get_bits(1)==0) {
    num_zeros++;

    if (num_zeros > MAX_UVLC_LEADING_ZEROS) { return false; }
  }

  int offset = 0;
  if (num_zeros != 0) {
    offset = (int)get_bits(num_zeros);
    *value = offset + (1<<num_zeros)-1;
    assert(*value>0);
    return true;
  } else {
    *value = 0;
    return true;
  }
}

bool BitReader::get_svlc(int* value)
{
  int v;
  if (!get_uvlc(&v)) {
    return false;
  } else if (v == 0) {
    *value = v;
    return true;
  }

  bool negative = ((v&1)==0);
  *value = negative ? -v/2 : (v+1)/2;
  return true;
}

void BitReader::refill()
{
#if 0
  // TODO: activate me one I'm sure this works
  while (nextbits_cnt <= 64-8 && bytes_remaining) {
    uint64_t newval = *data++;
    bytes_remaining--;

    nextbits_cnt += 8;
    newval <<= 64-nextbits_cnt;
    nextbits |= newval;
  }
#else
  int shift = 64 - nextbits_cnt;

  while (shift >= 8 && bytes_remaining) {
    uint64_t newval = *data++;
    bytes_remaining--;

    shift -= 8;
    newval <<= shift;
    nextbits |= newval;
  }

  nextbits_cnt = 64-shift;
#endif
}
