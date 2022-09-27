#include "Utils.hpp"

// // convert from big endian to native format (Intel=little endian) and return as unsigned int (32bit)
// unsigned int ReadBigEndianValue(std::vector<uint8_t> data) {
//   // Combine 4 bytes into one unsigned int
//   unsigned int x;
//   ::memcpy(&x, &data[0], sizeof(unsigned int));
//
//   // If system is LittleEndian, we do not need to revers(?)
//   // TODO: check if system is LittleEndian
//
//   // Reverses the order of bytes in an 32-bit integer.
//   return ((x & 0xff000000u) >> 24)
//        | ((x & 0x00ff0000u) >>  8)
//        | ((x & 0x0000ff00u) <<  8)
//        | ((x & 0x000000ffu) << 24);
//
// }

// convert from big endian to native format (Intel=little endian) and return as unsigned int (32bit)
unsigned ReadBEValue(std::vector<uint8_t> buffer)
{
	if(buffer.empty())
    return -1;

	unsigned result = 0;
	unsigned NumByteShifts = buffer.size() - 1;

	for (unsigned n=0; n < buffer.size(); n++)
	 result |= buffer[n] << 8*NumByteShifts--;

	return result;
}

// Credit for the code below goes to : Floris Creyf (May 2015)
unsigned get_bits(unsigned char *buffer, int start_bit, int end_bit)
{
	int start_byte = 0;
	int end_byte = 0;

	start_byte = start_bit >> 3;
	end_byte = end_bit >> 3;
	start_bit = start_bit % 8;
	end_bit = end_bit % 8;

	/* Get the bits. */
	unsigned result = ((unsigned)buffer[start_byte] << (32 - (8 - start_bit))) >> (32 - (8 - start_bit));

	if (start_byte != end_byte) {
		while (++start_byte != end_byte) {
			result <<= 8;
			result += buffer[start_byte];
		}
		result <<= end_bit;
		result += buffer[end_byte] >> (8 - end_bit);
	} else if (end_bit != 8)
		result >>= (8 - end_bit);

	return result;
}

unsigned get_bits_inc(unsigned char *buffer, int *offset, int count)
{
	unsigned result = get_bits(buffer, *offset, *offset + count);
	*offset += count;
	return result;
}

int char_to_int(unsigned char *buffer)
{
	unsigned num = 0x00;
	for (int i = 0; i < 4; i++)
		num = (num << 7) + buffer[i];
	return num;
}

#define CRC16 0x8005


// Credits for the caculation of CRC goes to:
// Copyright (C) 2002-2010 Naoki Shibata
// Copyright (C) 2011-2021 Elio Blanca <eblanca76@users.sourceforge.net>
unsigned short crc_update(unsigned short old_crc, unsigned char *data, unsigned int *begin_ptr, unsigned int length)
{
#define CRC16_POLYNOMIAL 0x8005

    unsigned char idx, data_len;
    unsigned int new_data, crc1=(unsigned int)old_crc, byte_length,
                 begin_offs=0, next_8multiple, jdx;

    if (begin_ptr != NULL && length > 0)
    {
        next_8multiple = (*begin_ptr+7) & ~7;
        if ((next_8multiple - *begin_ptr) > 0)
        {
            new_data = (unsigned int)(data[*begin_ptr/8]);
            new_data = new_data<<((*begin_ptr%8)+8);
            if (length < (next_8multiple - *begin_ptr))
                data_len = (unsigned char)length;
            else
                data_len = (unsigned char)(next_8multiple - *begin_ptr);
            /* 'data_len' is into [1..7] at most */

            for (idx=0; idx<data_len; idx++)
            {
                new_data <<= 1;
                crc1 <<= 1;

                if (((crc1 ^ new_data) & 0x10000))
                    crc1 ^= CRC16_POLYNOMIAL;
            }

            *begin_ptr += data_len;
            length -= data_len;
        }

        begin_offs = next_8multiple/8;
        *begin_ptr += length;
    }

    byte_length = length / 8;

    for (jdx=0; jdx<byte_length; jdx++)
    {
        new_data = (unsigned int)(data[begin_offs+jdx]);
        new_data = new_data<<8;
        for (idx=0; idx<8; idx++)
        {
            new_data <<= 1;
            crc1 <<= 1;

            if (((crc1 ^ new_data) & 0x10000))
                crc1 ^= CRC16_POLYNOMIAL;
        }
    }

    if (length % 8)
    {
        new_data = (unsigned int)(data[begin_offs+byte_length]);
        new_data = new_data<<8;
        for (idx=0; idx<length%8; idx++)
        {
            new_data <<= 1;
            crc1 <<= 1;

            if (((crc1 ^ new_data) & 0x10000))
                crc1 ^= CRC16_POLYNOMIAL;
        }
    }

    return (unsigned short)(crc1&0xFFFF);
}
