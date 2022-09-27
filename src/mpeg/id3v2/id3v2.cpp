#include <iostream>
#include <bitset>

#include "id3v2.hpp"


// TODO: "global" value 10 as header size ..
ID3v2::ID3v2(const std::vector<uint8_t> &data, uint8_t offset) {

  // Collect header data
  const std::vector<uint8_t> headerdata(data.begin()+offset, data.begin()+offset+10);

  // std::cout << "Header data: " << std::endl;
  // for (auto val : headerdata) printf("\\x%.2x", val);
  // std::cout << std::endl;

  Header = ParseHeader(headerdata);

  if(Header->isValid) {

    // TODO: Should only be valid if a header and one! frame is valid?
    isValid = true;

    std::vector<uint8_t> framesdata(data.begin()+offset+10, data.begin()+offset+10+Header->tagSize);

    // Not implemented yet
    // Parse all frames ..
    unsigned offset = 0;
    // unsigned validframes = 0;
    while(offset < framesdata.size() - Header->frameHeaderSize) {
      // std::cout << "offset: " << offset << " < " << framesdata.size() - Header->frameHeaderSize << std::endl;

      // If the next data is position is 0, assume that we've hit the padding
      // portion of the frame data.
      if(framesdata.at(offset) == 0) {
        if(Header->footerPresent) {
          debug(LOG_WARNING) << "Padding *and* a footer found.  This is not allowed by the spec." << std::endl;
        }
        break;
      }

      // Becuse we dont know the size of this frame, we need to parse the header first
      ID3v2FrameHeader* frameheader = ParseFrame(framesdata, offset, Header->majorVersion);

      if(frameheader) {
        // validframes++;
        m_frames.push_back(frameheader);

        // Advance
        offset += frameheader->m_frameSize + frameheader->m_frameHeaderSize;

      } else {
        debug(LOG_ERROR) << "ID3v2 Frame parsing failed!" << std::endl;
        break;
      }
    }

    // std::cout << "Number of valid frames: " << validframes << std::endl;

  }
}

long ID3v2::GetTagSize() {
  if(Header)
    return Header->tagSize;
  else
    return 0;
}

ID3v2Header* ID3v2::ParseHeader(const std::vector<uint8_t> &data) {
    if(data.size() < 10)
      return nullptr;


    ID3v2Header* header = new ID3v2Header;

    // do some sanity checking -- even in ID3v2.3.0 and less the tag size is a
    // synch-safe integer, so all bytes must be less than 128.  If this is not
    // true then this is an invalid tag.

    // note that we're doing things a little out of order here -- the size is
    // later in the bytestream than the version

    std::vector<uint8_t> sizeData = {data.begin() + 6, data.end()};

    // std::cout << "Header data: " << std::endl;
    // for (auto val : sizeData) printf("\\x%.2x", val);
    // std::cout << std::endl;

    // std::cout << "Header data: " << std::endl;
    // for (auto val : data) printf("\\x%.2x", val);
    // std::cout << std::endl;

    // std::vector<uint8_t> sizeData = data.mid(6, 4);

    if(sizeData.size() != 4) {
      header->tagSize = 0;
      debug(LOG_ERROR) << "ID3v2::ID3v2Header::parse() - The tag size as read was 0 bytes!" << std::endl;
      return nullptr;
    }

    for(auto val : sizeData) {
      if(val >= 128) {
        header->tagSize = 0;
        debug(LOG_ERROR) << "ID3v2::ID3v2Header::parse() - One of the size bytes in the id3v2 header was greater than the allowed 128." << std::endl;
        return nullptr;
      }
    }

    // Get the size from the remaining four bytes (read above)
    // header->tagSize = MPEG::Utils::toUInt(sizeData); // (structure 3.1 "size")
    header->tagSize = GetSynchsafeInteger(ReadBEValue(sizeData)); // (structure 3.1 "size")

    if(header->tagSize <= 0) {
      debug(LOG_ERROR) << "ID3v2 Tagsize cant be less or equal to zero." << std::endl;
      return nullptr;
    }

    // The first three bytes, data[0..2], are the File Identifier, "ID3". (structure 3.1 "file identifier")

    // Read the version number from the fourth and fifth bytes.
    header->majorVersion = data[3];   // (structure 3.1 "major version")
    header->revisionNumber = data[4]; // (structure 3.1 "revision number")

    // Read the flags, the first four bits of the sixth byte.
    std::bitset<8> flags(data[5]);

    header->unsynchronisation     = flags[7]; // (structure 3.1.a)
    header->extendedHeader        = flags[6]; // (structure 3.1.b)
    header->experimentalIndicator = flags[5]; // (structure 3.1.c)
    header->footerPresent         = flags[4]; // (structure 3.1.d)


    // Set FRAME header size depending on major version
    switch(header->majorVersion) {
      case 0:
      case 1:
      case 2: header->frameHeaderSize = 6; break;
      case 3:
      case 4:
      default: header->frameHeaderSize = 10;
    }

    header->isValid = true;

    return header;
}

ID3v2FrameHeader* ID3v2::ParseFrame(std::vector<uint8_t>& data, long offset, unsigned int majorVersion) {
  // Taglib verkar sätta headerversion lite beroende på frame..
  // varför vet jag inte. Men beroende på id3v2 version så ändras
  // strukturen på frames.. vissa kan bara bara id3v2.4 till ex?

  // får kika på det senare?

  // OK att mixa Majorversioner ?? borde ju inte "gå" för att den
  // specifieras i idv2 headern.. ska vara om det finns två id3v2 tags.
  // men det är inte fallet när jag körde i taglib
  ID3v2FrameHeader* frame = new ID3v2FrameHeader();

  frame->m_version = majorVersion;

  switch(frame->m_version) {
    case 0:
    case 1:
    case 2:
    {
      /*
      ID3v2.2
      Frame have 3 byte type.
      Frame size is 3 bytes big endian integer (not synch safe).
      Unsynchronization flag in tag header is for whole id3 tag.
      */
      // debug(LOG_INFO) << "FrameVersion: ID3v2.2 "<< std::endl;
      frame->m_frameHeaderSize = 6;

      if(data.size() - offset < 3) {
        debug(LOG_ERROR) << "You must at least specify a frame ID (ID3v2.2)." << std::endl;
        break;
      }

      // Set the frame ID -- the first three bytes
      frame->m_frameID = {data.begin()+offset, data.begin()+offset+3};

      // Debugging test
      std::string id(frame->m_frameID.begin(), frame->m_frameID.end());
      debug(LOG_INFO) << "FrameID: " << id << std::endl;

      // If the full header information was not passed in, do not continue to the
      // steps to parse the frame size and flags.

      if(data.size() - offset < 6) {
        debug(LOG_ERROR) << "Full frame header information was not passed in ( 6 bytes )" << std::endl;
        break;
      }

      // std::cout << "frameSize data: " << std::hex;
      // std::cout << (int)data[3] << ",";
      // std::cout << (int)data[4] << ",";
      // std::cout << (int)data[5] << std::dec << std::endl;

      // Read Bigendian 3 byte integer
      frame->m_frameSize = (data[offset + 5]<<0) | (data[offset + 4]<<8) | (data[offset + 3]<<16);
      debug(LOG_INFO) << "FrameSize: " << frame->m_frameSize << std::endl;

      frame->m_isValid = true;
      break;
    }
    case 3:
    {
      /*
      ID3v2.3
        Frame have 4 byte type.
        Frame size is 4 bytes big endian integer (not synch safe).
        Unsynchronization flag in tag header is for whole id3 tag.
        Optional extended header not compat with v2.4.
        Extended header size is 4 byte big endian (not sync safe).
        Extended header size is header excluding size field bytes.
      */
      // debug(LOG_INFO) << "FrameVersion: ID3v2.3 "<< std::endl;
      frame->m_frameHeaderSize = 10;

      if(data.size() - offset < 4) {
        debug(LOG_ERROR) << "You must at least specify a frame ID (ID3v2.3)." << std::endl;
        break;
      }

      // Set the frame ID -- the first four bytes
      frame->m_frameID = {data.begin()+offset, data.begin()+offset+4};

      std::string id(frame->m_frameID.begin(), frame->m_frameID.end());
      // debug(LOG_INFO) << "FrameID: " << id << std::endl;

      // If the full header information was not passed in, do not continue to the
      // steps to parse the frame size and flags.

      if(data.size() - offset < 10) {
        debug(LOG_ERROR) << "Full frame header information was not passed in ( 10 bytes )" << std::endl;
        break;
      }

      // Set the size -- the frame size is the four bytes syncsafe
      // frame->m_frameSize = MPEG::Utils::decodeSynsafe({data[offset+4], data[offset+5], data[offset+6], data[offset+7]});
      frame->m_frameSize = ReadBEValue({data[offset+4], data[offset+5], data[offset+6], data[offset+7]});


      { // read the first byte of flags
        std::bitset<8> flags(data[offset+8]);
        frame->m_tagAlterPreservation  = flags[7]; // (structure 3.3.1.a)
        frame->m_fileAlterPreservation = flags[6]; // (structure 3.3.1.b)
        frame->m_readOnly              = flags[5]; // (structure 3.3.1.c)
      }

      { // read the second byte of flags
        std::bitset<8> flags(data[offset+9]);
        frame->m_compression         = flags[7]; // (structure 3.3.1.i)
        frame->m_encryption          = flags[6]; // (structure 3.3.1.j)
        frame->m_groupingIdentity    = flags[5]; // (structure 3.3.1.k)
      }

      frame->m_isValid = true;
      break;
    }
    case 4:
    default:
    {
      /*
      ID3v2.4
        Frame have 4 byte type.
        Frame size is 4 bytes synch safe big endian integer.
        Unsynchronization flag in tag header is for whole id3 tag but if false individual frames can set a frame header unsynchronization flag.
        Optional extended header not compat with v2.3.
        Extended header size is 4 bytes synch safe big endian integer.
        Extended header size is whole header including size field bytes.
      */
      // debug(LOG_INFO) << "FrameVersion: ID3v2.4 "<< std::endl;
      frame->m_frameHeaderSize = 10;

      if(data.size() - offset < 4) {
        debug(LOG_ERROR) << "You must at least specify a frame ID (ID3v2.4)." << std::endl;
        break;
      }

      // Set the frame ID -- the first four bytes

      frame-> m_frameID = {data.begin(), data.begin()+4};

      std::string id(frame->m_frameID.begin(), frame->m_frameID.end());
      debug(LOG_INFO) << "FrameID: " << id << std::endl;

      // If the full header information was not passed in, do not continue to the
      // steps to parse the frame size and flags.

      if(data.size() - offset < 10) {
        debug(LOG_ERROR) << "Full frame header information was not passed in ( 10 bytes )" << std::endl;
        break;
      }

      // Set the size -- the frame size is the four bytes starting at byte four in
      // the frame header (structure 4)

      // frame->m_frameSize = MPEG::Utils::toUInt({data.begin()+4, data.begin()+8});
      frame->m_frameSize = GetSynchsafeInteger(ReadBEValue({data.begin()+4, data.begin()+8})); // EJ dubbel kollat
      debug(LOG_INFO) << "FrameSize: " << frame->m_frameSize << std::endl;


  // TODO:
  // #ifndef NO_ITUNES_HACKS
  //     // iTunes writes v2.4 tags with v2.3-like frame sizes
  //     if(d->frameSize > 127) {
  //       if(!isValidFrameID(data.mid(d->frameSize + 10, 4))) {
  //         unsigned int uintSize = data.toUInt(4U);
  //         if(isValidFrameID(data.mid(uintSize + 10, 4))) {
  //           d->frameSize = uintSize;
  //         }
  //       }
  //     }
  // #endif

      { // read the first byte of flags
        std::bitset<8> flags(data[8]);
        frame->m_tagAlterPreservation  = flags[6]; // (structure 4.1.1.a)
        frame->m_fileAlterPreservation = flags[5]; // (structure 4.1.1.b)
        frame->m_readOnly              = flags[4]; // (structure 4.1.1.c)
      }

      { // read the second byte of flags
        std::bitset<8> flags(data[9]);
        frame->m_groupingIdentity    = flags[6]; // (structure 4.1.2.h)
        frame->m_compression         = flags[3]; // (structure 4.1.2.k)
        frame->m_encryption          = flags[2]; // (structure 4.1.2.m)
        frame->m_unsynchronisation   = flags[1]; // (structure 4.1.2.n)
        frame->m_dataLengthIndicator = flags[0]; // (structure 4.1.2.p)
      }

      frame->m_isValid = true;
      break;
    }
  }

  if(frame->m_isValid)
    return frame;
  else
    return nullptr;

}

void ID3v2::Render(int loglevel) {
  // Ser ut att funka som det ska, får samma värden ifrån TagLib (på Johnny cash - hurt.mp3)
  debug(loglevel) << "majorVersion: \t\t" << Header->majorVersion << std::endl;
  debug(loglevel) << "revisionNumber: \t" << Header->revisionNumber << std::endl;
  debug(loglevel) << "unsynchronisation: \t" << (Header->unsynchronisation ? "Yes" : "No") << std::endl;
  debug(loglevel) << "extendedHeader: \t" << (Header->extendedHeader ? "Yes" : "No") << std::endl;
  debug(loglevel) << "experimentalIndicator:\t" << (Header->experimentalIndicator ? "Yes" : "No") << std::endl;
  debug(loglevel) << "footerPresent: \t\t" << (Header->footerPresent ? "Yes" : "No") << std::endl;

  debug(loglevel) << "tagSize: \t\t" << Header->tagSize << std::endl;

  // TODO: Frame info ...
  debug(loglevel) << "Frames:" << std::endl;
  for(auto frame : m_frames) {
    std::string id(frame->m_frameID.begin(), frame->m_frameID.end());
    // debug(loglevel) << "FrameID: " << id << " - " << frameTranslation[id] << std::endl;
    debug(loglevel) << id << " - " << frameTranslation[id] << std::endl;
    // debug(loglevel) << "FrameSize: " << frame->m_frameSize << std::endl;

  }

  std::cout << std::endl;
  debug(loglevel) << "Number of frames: " << m_frames.size() << std::endl;
  // Note:
  // The ID3v2 tag size is the size of the complete tag after unsychronisation,
  // including padding, excluding the header but not excluding the extended header
}

std::vector<uint8_t> ID3v2::unsynchronisationSchemeDecode(const std::vector<uint8_t> &data)
{
  debug(LOG_VERBOSE) << "Decoding unsynchronisation tag.." << std::endl;

  if (data.size() == 0) {
    return std::vector<uint8_t>();
  }

  // We have this optimized method instead of using ByteVector::replace(),
  // since it makes a great difference when decoding huge unsynchronized frames.

  std::vector<uint8_t> result(data.size());

  std::vector<uint8_t>::const_iterator src = data.begin();
  std::vector<uint8_t>::iterator dst = result.begin();

  while(src < data.end() - 1) {
    *dst++ = *src++;

    if(*(src - 1) == '\xff' && *src == '\x00')
      src++;
  }

  if(src < data.end())
    *dst++ = *src++;

  result.resize(static_cast<unsigned int>(dst - result.begin()));

  return result;
}

// (Konrad Windszus - https://www.codeproject.com/Articles/8295/MPEG-Audio-Frame-Header )
// return for each byte only lowest 7 bits (highest bit always zero)
unsigned int ID3v2::GetSynchsafeInteger(unsigned int dwValue)
{
	// masks first 7 bits
	const unsigned int dwMask = 0x7F000000;
	unsigned int dwReturn = 0;
	for (int n=0; n < sizeof(unsigned int); n++)
	{
		dwReturn = (dwReturn << 7) | ((dwValue & dwMask) >> 24);
		dwValue <<= 8;
	}
	return dwReturn;
}
