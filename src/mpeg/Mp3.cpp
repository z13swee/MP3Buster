#include "Mp3.hpp"

#define PI    3.141592653589793
#define SQRT2 1.414213562373095

MP3::MP3(std::filesystem::path path, myConfig& config)
{

  // GlobalLogLevel = LOG_VERBOS_ERROR;

  cfg = config;
  m_File = new FileWrapper(path);

  if(m_File->isValid() && m_File->getSize() != 0) {
    m_FileSize = m_File->getSize();

    // Batchmode !
    if(cfg.batchmode) {

      GlobalLogLevel = LOG_SILENT;
      Analyze();

      GlobalLogLevel = LOG_INFO;
      BatchmodeOutput(path, cfg);


    } else {  // Not batchmode..
      debug(LOG_INFO) << "File: " << path.filename().string() << std::endl;
      Analyze();
    }

  } else {
    debug(LOG_INFO) << std::left << CONSOLE_COLOR_RED << std::setw(cfg.longestWidthEntry + 3) << path.filename().string() << ": "  << "NOT OK! (zero bytes or invalid file)" << CONSOLE_COLOR_NORMAL << std::endl;
  }
}

MP3::~MP3()
{
  if(m_File)
    delete m_File;
}

bool MP3::hasBadFrames()
{
  return m_badframes; // Funkar detta?? :>
}

void MP3::BatchmodeOutput(std::filesystem::path path, myConfig& cfg)
{
  auto const column_width = cfg.longestWidthEntry + 3;

  if(m_badframes) {
    // If stoponerror is used, number of bad frames will alwats be 1.. and can be missleading
    if(cfg.stoponerror) {
      debug(LOG_INFO) << std::left << CONSOLE_COLOR_RED << std::setw(column_width) << path.filename().string() << ": "  << "NOT OK!" << CONSOLE_COLOR_NORMAL << std::endl;
    } else {
      debug(LOG_INFO) << std::left << CONSOLE_COLOR_RED << std::setw(column_width) << path.filename().string() << ": "  << "NOT OK! ("<< m_badframes << " bad frames)" << CONSOLE_COLOR_NORMAL << std::endl;
    }

  } else {
    // debug(LOG_INFO) << outputstring << std::endl;
    debug(LOG_INFO) << std::left << std::setw(column_width) << path.filename().string() << ": " <<  CONSOLE_COLOR_GOLD << "OK!" << CONSOLE_COLOR_NORMAL << std::endl;
  }

}

int syncint_decode(int value)
{
    unsigned int a, b, c, d, result = 0x0;
    a = value & 0xFF;
    b = (value >> 8) & 0xFF;
    c = (value >> 16) & 0xFF;
    d = (value >> 24) & 0xFF;

    result = result | a;
    result = result | (b << 7);
    result = result | (c << 14);
    result = result | (d << 21);

    return result;
}

void MP3::FindBeginningTags(unsigned amount) {

  // Search for ID3v2
  auto res = std::search(std::begin(m_filebuffer), std::begin(m_filebuffer) + amount, std::begin(ID3v2_Identifier), std::end(ID3v2_Identifier));

  if(res != std::begin(m_filebuffer) + amount)
  {
    foundID3v2 = true;
    id3v2_offset = std::distance( m_filebuffer.begin(), res );

    // Create and parse id3v2 tag
    id3v2 = new ID3v2(m_filebuffer , id3v2_offset);

    // std::cout << "GetTagSize: " << id3v2->GetTagSize() << std::endl;
    endOfStartTagsOffset += id3v2->GetTagSize();
  }

  // auto res = std::search(std::begin(m_filebuffer), std::begin(m_filebuffer)+1024, std::begin(APE_Identifier), std::end(APE_Identifier));
  res = std::search(std::begin(m_filebuffer), std::begin(m_filebuffer) + amount, std::begin(APE_Identifier), std::end(APE_Identifier));

  if(res != std::begin(m_filebuffer) + amount) {
    foundAPEv2 = true;
    apev2_offset = std::distance( m_filebuffer.begin(), res );

    // TODO: Add to endOfStartTagsOffset
  }
}



void MP3::FindEndTags(unsigned offset) {
  debug(LOG_DEBUG) << "Looing for end tags from offset: " << offset << std::endl;

  // Search for ID3v1
  auto res = std::search(std::begin(m_filebuffer) + offset, std::end(m_filebuffer), std::begin(ID3v1_Identifier), std::end(ID3v1_Identifier));

  if(res != std::end(m_filebuffer))
  {
    id3v1_offset = std::distance( m_filebuffer.begin(), res );


    // Create and parse id3v2 tag
    // id3v1 = new ID3v1(m_filebuffer , offset);
  }


  // Search for APEv1
  res = std::search(std::begin(m_filebuffer) + offset, std::end(m_filebuffer), std::begin(APE_Identifier), std::end(APE_Identifier));

  if(res != std::end(m_filebuffer))
  {
    apev1_offset = std::distance( m_filebuffer.begin(), res );

    // Create and parse APE tag
    // APEv1 = new APEv1(m_filebuffer , offset);
  }

}




inline bool MP3::isFrameSync(const std::vector<uint8_t> &bytes, unsigned int offset)
{
  const unsigned char b1 = bytes[offset + 0];
  const unsigned char b2 = bytes[offset + 1];
  return (b1 == 0xFF && b2 != 0xFF && (b2 & 0xE0) == 0xE0);
}


void MP3::Analyze()
{

  m_File->seek(0);
  m_filebuffer = m_File->readBlock(m_FileSize);

  debug(LOG_VERBOSE) << "Looking for tags in the first 1024 bytes" << std::endl;
  FindBeginningTags(1024);  // Search in first 1024 bytes

  // Silens false positive header errors
  // logger::GlobalLogLevel = LOG_SILENT;

  snd_pcm_t *handle = NULL;
  snd_pcm_hw_params_t *hw = NULL;
  snd_pcm_uframes_t frames = 128;

  for(unsigned int i = endOfStartTagsOffset; i < m_FileSize - 1; ++i) {

    if(isFrameSync(m_filebuffer, i)) {
      CurrentHeaderOffset = i;

      while (InitializeHeader(m_File)) {

        // For first valid header, setup audio output
        if(m_FrameIndex == 1) {
          // logger::GlobalLogLevel = LOG_INFO;

          firstvalidframe = i;
          debug(LOG_DEBUG) << "FIST VALID FRAME FOUND AT: " << firstvalidframe << std::endl;
          // Print some info about first frame data
          printHeaderInformation(LOG_INFO);

          // Setup for audio output ( done only for first valid frame)
          if(cfg.playback) {

            if (snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0)
              exit(1);

            snd_pcm_hw_params_alloca(&hw);
            snd_pcm_hw_params_any(handle, hw);

            if (snd_pcm_hw_params_set_access(handle, hw, SND_PCM_ACCESS_RW_INTERLEAVED) < 0)
              exit(1);
            if (snd_pcm_hw_params_set_format(handle, hw, SND_PCM_FORMAT_FLOAT_LE) < 0)
              exit(1);
            if (snd_pcm_hw_params_set_channels(handle, hw, channels) < 0)
              exit(1);
            if (snd_pcm_hw_params_set_rate_near(handle, hw, &samplingRate, NULL) < 0)
              exit(1);
            if (snd_pcm_hw_params_set_period_size_near(handle, hw, &frames, NULL) < 0)
              exit(1);
            if (snd_pcm_hw_params(handle, hw) < 0)
              exit(1);
            if (snd_pcm_hw_params_get_period_size(hw, &frames, NULL) < 0)
              exit(1);
            if (snd_pcm_hw_params_get_period_time(hw, &samplingRate, NULL) < 0)
              exit(1);
          }

        }

        // Decode frame data
        if(cfg.playback) {
          debug(LOG_DEBUG) <<  "init_frame_params @ " << CurrentHeaderOffset << std::endl;
          InitializeFrame(&m_filebuffer[CurrentHeaderOffset]);

          debug(LOG_DEBUG) << "\rPlaying frame: " << m_FrameIndex << " @ " << CurrentHeaderOffset;
          int e = snd_pcm_writei(handle, pcm, 1152);
          if (e == -EPIPE)
            snd_pcm_recover(handle, e, 0);
        }

        // Was this the last frame? next valid frame cant be bigger
        // or at end of the file
        if(offsetToNextValidFrame >= m_FileSize) {
          debug(LOG_VERBOSE) << "End of file at " << m_FileSize << std::endl;
          break;
        }

        CurrentHeaderOffset = offsetToNextValidFrame;
        m_FrameIndex++;
      }

      // Jump forward to where 'while' loop ended
      if(offsetToNextValidFrame > CurrentHeaderOffset)
        i = offsetToNextValidFrame - 1;
      else
        i = CurrentHeaderOffset;

      // Stop on Error
      if(cfg.stoponerror)
        break;

    }
  }

  if(handle) {
    snd_pcm_drain(handle);
    snd_pcm_close(handle);
  }

  // If offsetToEndOfAudioFrames is not 0, check for end tags att the remaining bytes of the file
  if(offsetToEndOfAudioFrames > 0)
    FindEndTags(offsetToEndOfAudioFrames);


  // Found tags..
  if(id3v1_offset)
    debug(LOG_INFO) << "ID3v1 Tag Found at " << id3v1_offset << std::endl;
  if(foundID3v2)
    debug(LOG_INFO) << "ID3v2 Tag Found at " << id3v2_offset << std::endl;
  if(apev1_offset)
    debug(LOG_INFO) << "APEv1 Tag Found at " << apev1_offset << std::endl;
  if(foundAPEv2)
    debug(LOG_INFO) << "APEv2 Tag Found at " << apev2_offset << std::endl;


  // Clearify that numver of frames is unitil stopped if that options is used.
  if(cfg.stoponerror)
    debug(LOG_INFO) << "Number of frames until first bad frame: " << m_FrameIndex << std::endl;
  else
    debug(LOG_INFO) << "Number of frames: " << m_FrameIndex << " Number of bad frames:" << m_badframes << std::endl;



}

int MP3::InitializeHeader(FileWrapper* file) {
  // std::cout << "InitializeHeader @ " << CurrentHeaderOffset << " (hex: " << std::hex << CurrentHeaderOffset << std::dec << ")" << " Frame: " << m_FrameIndex << std::endl;
  debug(LOG_DEBUG) << CONSOLE_COLOR_RED << "InitializeHeader @ " << CurrentHeaderOffset << " (hex: " << std::hex << CurrentHeaderOffset << std::dec << ")" << "\033[0;0m" << std::endl;

  isHeaderValid = false;

  file->seek(CurrentHeaderOffset); // Oklart om denna behövs då vi redan borde vara där?
  std::vector<uint8_t> data = file->readBlock(4);

  if(data.size() < 4) {
    debug(LOG_ERROR) << "Error reading header -- data is too short for an MPEG frame header." << std::endl;
    return 0;
  }

  // Set the MPEG version ID
  // -----------------------
  const int versionBits = (static_cast<unsigned char>(data[1]) >> 3) & 0x03;

  switch (versionBits) {
    case 0: version = 25; break;
    case 1: version = -1; debug(LOG_WARNING) << "Reserved version, likly bad frame?" << std::endl; break;
    case 2: version = 2; break;
    case 3: version = 1; break;
    default: debug(LOG_ERROR) << "No matching version!, " << versionBits << std::endl;
    // mpegerrortype = FRAMEHEADER;
  }


  // Set the MPEG layer
  // -----------------------
  const int layerBits = (static_cast<unsigned char>(data[1]) >> 1) & 0x03;

  switch (layerBits) {
    case 0: debug(LOG_WARNING) << "Reserved layer, likly bad frame? " << layerBits <<  " (Frame: " << m_FrameIndex << ")" << std::endl;
    case 1: layer = 3; break;
    case 2: layer = 2; break;
    case 3: layer = 1; break;
    default: debug(LOG_ERROR) << "No matching layer!, " << layerBits << std::endl;
    // mpegerrortype = FRAMEHEADER;
  }


  // CRC check
  bool checkCRC = false;
  std::vector<uint8_t> crc_checksum;
  protectionEnabled = !(static_cast<unsigned char>(data[1] & 0x01) == 0);


  if(protectionEnabled) {
    crc_checksum = file->readBlock(2);
    // crc_checksum = {filebuffer.begin() + offset + 4, filebuffer.begin() + offset + 4 + 2};
    debug(LOG_DEBUG) << "CRC for this frame: " << std::hex << std::uppercase << (int)crc_checksum[0] << (int)crc_checksum[1] << std::dec << std::endl;
    checkCRC = true;
  }

  // Set the bitrate
  // -----------------------
  static const int bitrates[2][3][16] = {
    { // Version 1
      { 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0 }, // layer 1
      { 0, 32, 48, 56, 64,  80,  96,  112, 128, 160, 192, 224, 256, 320, 384, 0 }, // layer 2
      { 0, 32, 40, 48, 56,  64,  80,  96,  112, 128, 160, 192, 224, 256, 320, 0 }  // layer 3
    },
    { // Version 2 or 2.5
      { 0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0 }, // layer 1
      { 0, 8,  16, 24, 32, 40, 48, 56,  64,  80,  96,  112, 128, 144, 160, 0 }, // layer 2
      { 0, 8,  16, 24, 32, 40, 48, 56,  64,  80,  96,  112, 128, 144, 160, 0 }  // layer 3
    }
  };

  const int versionIndex = (version == 1) ? 0 : 1;
  const int layerIndex   = (layer > 0) ? layer - 1 : 0;

  // The bitrate index is encoded as the first 4 bits of the 3rd byte,
  // i.e. 1111xxxx

  const int bitrateIndex = (static_cast<unsigned char>(data[2]) >> 4) & 0x0F;
  bitrate = bitrates[versionIndex][layerIndex][bitrateIndex];

  // Guessing bitrate can never be 0
  if(bitrate == 0) {
    debug(LOG_ERROR) << "Error parsing bitrate, bitrateIndex: " << bitrateIndex << " (Frame: " << m_FrameIndex << ")" << std::endl;
    // mpegerrortype = FRAMEHEADER;
    return 0;
  }



  // Check if this MPEG file have VBR (variable bit rate). If not,

  // Set the sample/frequency rate

  // static const int sampleRates[3][4] = {
  //   { 44100, 48000, 32000, 0 }, // Version 1
  //   { 22050, 24000, 16000, 0 }, // Version 2
  //   { 11025, 12000, 8000,  0 }  // Version 2.5
  // };
  //
  // // The sample rate index is encoded as two bits in the 3nd byte, i.e. xxxx11xx
  //
  // const int samplerateIndex = (static_cast<unsigned char>(data[2]) >> 2) & 0x03;
  // samplingRate = sampleRates[version][samplerateIndex];

  int rates[3][3] {44100, 48000, 32000, 22050, 24000, 16000, 11025, 12000, 8000};

	for (int rateversion = 1; rateversion <= 3; rateversion++)
		if (version == rateversion) {
			if ((data[2] & 0x08) != 0x08 && (data[2] & 0x04) != 0x04) {
				samplingRate = rates[rateversion - 1][0];
				break;
			} else if ((data[2] & 0x08) != 0x08 && (data[2] & 0x04) == 0x04) {
				samplingRate = rates[rateversion - 1][1];
				break;
			} else if ((data[2] & 0x08) == 0x08 && (data[2] & 0x04) != 0x04) {
				samplingRate = rates[rateversion - 1][2];
				break;
			}
		}

  // Guessing sampleRate can never be 0
  if(samplingRate == 0) {
    debug(LOG_ERROR) << "Error parsing sampleRate, sampleRate is zero"  << std::endl;
    // mpegerrortype = FRAMEHEADER;
    return 0;
  }



  // Set the tables
  // -----------------------
  // During the decoding process different tables are used depending on the sampling rate.
  switch (samplingRate) {
		case 32000:
			band_index.short_win = band_index_table.short_32;
			band_width.short_win = band_width_table.short_32;
			band_index.long_win = band_index_table.long_32;
			band_width.long_win = band_width_table.long_32;
			break;
		case 44100:
			band_index.short_win = band_index_table.short_44;
			band_width.short_win = band_width_table.short_44;
			band_index.long_win = band_index_table.long_44;
			band_width.long_win = band_width_table.long_44;
			break;
		case 48000:
			band_index.short_win = band_index_table.short_48;
			band_width.short_win = band_width_table.short_48;
			band_index.long_win = band_index_table.long_48;
			band_width.long_win = band_width_table.long_48;
			break;
	}



  // The channel mode is encoded as a 2 bit value at the end of the 3nd byte,
  // i.e. xxxxxx11

  channel_mode = static_cast<ChannelMode>((static_cast<unsigned char>(data[3]) >> 6) & 0x03);

  // channel_mode = static_cast<ChannelMode>(value);
	channels = channel_mode == Mono ? 1 : 2;



  // Mode extension
  static const int modeExtensionTable[2][4][2] = {
    { {4,31}, {8,31}, {12,31}, {16,31} }, // Version 1 & 2
    { {0,0}, {1,0}, {0,1}, {1,1} }, // Version 3
  };

  const int modeExtensionIndex = static_cast<ChannelMode>((static_cast<unsigned char>(data[3]) >> 4) & 0x03);
  const int modelayerIndex   = (layer > 2) ? 1 : 0;

  modeExtension[0] = modeExtensionTable[modelayerIndex][modeExtensionIndex][0];
  modeExtension[1] = modeExtensionTable[modelayerIndex][modeExtensionIndex][1];



  // Flags
  isCopyrighted = ((static_cast<unsigned char>(data[3]) & 0x08) != 0);
  isOriginal    = ((static_cast<unsigned char>(data[3]) & 0x04) != 0);
  isPadded      = ((static_cast<unsigned char>(data[2]) & 0x02) != 0);



  // emphasis = static_cast<int>((static_cast<unsigned char>(data[3])) & 0x03);
  unsigned value = (data[3] << 6) >> 6;
	emphasis = static_cast<Emphasis>(value);



  // Samples per frame
  static const int samplesPerFrameList[3][2] = {
    // MPEG1, 2/2.5
    {  384,   384 }, // Layer I
    { 1152,  1152 }, // Layer II
    { 1152,   576 }  // Layer III
  };

  samplesPerFrame = samplesPerFrameList[layerIndex][versionIndex];


  // Calculate the frame length
  static const int paddingSize[3] = { 4, 1, 1 };

  /* Minimum frame size = 1152 / 8 * 32000 / 48000 = 96
	 * Minimum main_data size = 96 - 36 - 2 = 58
	 * Maximum main_data_begin = 2^9 = 512
	 * Therefore remember ceil(512 / 58) = 9 previous frames. */
	for (int i = num_prev_frames-1; i > 0; --i)
		prev_frame_size[i] = prev_frame_size[i-1];
	prev_frame_size[0] = frame_size;

  frame_size = samplesPerFrame * bitrate * 125 / samplingRate;

  if(isPadded)
    frame_size += paddingSize[layerIndex];

  // debug(LOG_DEBUG) << "->version (" << version << ")" << std::endl;
  // debug(LOG_DEBUG) << "->layer (" << layer << ")" << std::endl;
  // debug(LOG_DEBUG) << "->protectionEnabled (crc) (" << protectionEnabled << ")" << std::endl;
  // debug(LOG_DEBUG) << "- Info missing! -" << std::endl;
  // debug(LOG_DEBUG) << "->emphasis (" << emphasis << ")" << std::endl;
  // debug(LOG_DEBUG) << "->samplingRate (" << samplingRate << ")" << std::endl;
  // debug(LOG_DEBUG) << "->tables (" << band_index.short_win << "," << band_width.short_win <<
  //                                     band_index.long_win << "," << band_width.long_win <<
  //                                     " )" << std::endl;
  //
  // debug(LOG_DEBUG) << "->channel_mode (" << channel_mode << ")" << std::endl;
  // debug(LOG_DEBUG) << "->channels (" << channels << ")" << std::endl;
  // debug(LOG_DEBUG) << "->modeExtension (" << modeExtension[0] << "," << modeExtension[1] << ")" << std::endl;
  // debug(LOG_DEBUG) << "->isPadded (" << isPadded << ")" << std::endl;
  // debug(LOG_DEBUG) << "->bitrate (" << bitrate << "k)" << std::endl;
  // debug(LOG_DEBUG) << "->samplesPerFrame (" << samplesPerFrame << ")" << std::endl;
  // debug(LOG_DEBUG) << "->frame_size (" << frame_size << ")" << std::endl;
  //
  // // Not used by Floris
  // debug(LOG_DEBUG) << "->isCopyrighted (" << isCopyrighted << ")" << std::endl;
  // debug(LOG_DEBUG) << "->isOriginal (" << isOriginal << ")" << std::endl;


  // Check if the frame length has been calculated correctly, or the next frame
  // header is right next to the end of this frame.

  // The MPEG versions, layers and sample rates of the two frames should be
  // consistent. Otherwise, we assume that either or both of the frames are
  // broken.

  // Checking CRC now when we know the supposed frame length
  if(checkCRC) {
    // Denna rutin för att kolla CRC checksumman har jag utläst ifrån mp3guessenc
    uint16_t calculated_checksum = 0xFFFF;

    // Kombinera 2 bytes till 1 uint16_t
    uint16_t checksum_target = crc_checksum[0]*256+crc_checksum[1];
    // debug(LOG_VERBOSE) << "CRC target: 0x" << std::hex << std::uppercase << (int)checksum_target << std::endl;


    uint8_t dummy = data[2];

    // debug(LOG_VERBOSE) << "dummy: " << std::hex << (int)data[2] << std::endl;

    // calculated_checksum = crc16(&dummy, sizeof(dummy), calculated_checksum);
    calculated_checksum = crc_update(calculated_checksum, &dummy, NULL, 8);
    // debug(LOG_VERBOSE) << "calculated_checksum: " << calculated_checksum << " ( 0x" << std::hex << calculated_checksum << ")" << std::endl;

    dummy = data[3];
    calculated_checksum = crc_update(calculated_checksum, &dummy, NULL, 8);

    // Lite oklart vart 256 kommer ifrån.. som jag ser det så är den konstant när jag läser koden ifrån
    // mp3guessenc och läses in efter CRC checksumman efter headern.

    // Då är vi inne i "side information" delen av mp3 strukturen, har inte dekodat
    // det än. Får se om det behövs

    std::vector<uint8_t> chk_buffer = file->readBlock(256);
    // 6 = 4 byte header and 2 byte crc
    // std::vector<uint8_t> chk_buffer = {filebuffer.begin() + offset + 6, filebuffer.begin() + offset + 6 + 256};

    // calculated_checksum = crc_update(calculated_checksum, &chk_buffer[0], NULL, 304-8*(sizeof(unsigned int)+sizeof(unsigned short)));
    calculated_checksum = crc_update(calculated_checksum, &chk_buffer[0], NULL, 256);
    // debug(LOG_VERBOSE) << "Calculated CRC: 0x" << std::hex << std::uppercase << calculated_checksum << std::endl;

    if(calculated_checksum != checksum_target) {
      CRCpassed = false;
      // mpegerrortype = CRC;
      // debug(LOG_ERROR) << "CRC check failed!" << std::endl;
    } else {
      CRCpassed = true;
      // debug(LOG_VERBOSE) << "CRC check passed!" << std::endl;
    }

  }

  // If this is the first MPEG header, check for Xing/VBRI information
  if(m_FrameIndex == 1) {

    static const int xingOffsetTable[2][2] = {
      // MPEG1, 2/2.5
      {  32+4,  17+4 }, // Stero, JointStereo, DualChannel
      {  17+4,  9+4 }, // Mono
    };

    // Look for Xing header
    xingHeaderOffset = xingOffsetTable[channels == Mono ? 1 : 0][version == 1 ? 0 : 1];

    file->seek(CurrentHeaderOffset + xingHeaderOffset);
    const std::vector<uint8_t> XingIdentification = file->readBlock(4);

    if(XingIdentification == xingPattern1 || XingIdentification == xingPattern2) {

      xingHeader = true;
      xingString = XingIdentification[0] == L'I' ? "Info" : "Xing";

      // Read flags
      const std::vector<uint8_t> XingFlags = file->readBlock(4);

      // Wierd haveing 4 bytes when only low nibble of one byte
      // makes up the flags.. or i'm missing something?

      // Check if we have 'number of frames' field
      if((static_cast<unsigned char>(XingFlags[3]) & 0x01) != 0){
        const std::vector<uint8_t> xing_framesdata = file->readBlock(4);
        xing_frames = ReadBEValue(xing_framesdata);
      }

      // Check if we have 'number of bytes' field
      if((static_cast<unsigned char>(XingFlags[3]) & 0x02) != 0){
        const std::vector<uint8_t> xing_bytesdata = file->readBlock(4);
        xing_bytes = ReadBEValue(xing_bytesdata);

      }

      // Check if we have 'TOC' field
      if((static_cast<unsigned char>(XingFlags[3]) & 0x04) != 0){
        // TOC = Table of contents, vet inte riktigt vad den är tillför..
        xing_TOCData = file->readBlock(100);
      }

      // Check if we have 'Quality indicator' field
      if((static_cast<unsigned char>(XingFlags[3]) & 0x08) != 0){
        // from 0 - best quality to 100 - worst
        const std::vector<uint8_t> xing_qualitydata = file->readBlock(4);
        xing_quality = ReadBEValue(xing_qualitydata);
      }


      // TODO: Decode lame extension
    }

    // Check for VBRI header
    file->seek(CurrentHeaderOffset + 4 + 32);
    const std::vector<uint8_t> VBRIIdentification = file->readBlock(4);

    if(VBRIIdentification == vbriPattern1) {
      VBRIHeader = true;

      // Read VBRI version
      const std::vector<uint8_t> vbri_versiondata = file->readBlock(2);
      VBRI_version = ReadBEValue(vbri_versiondata);

      // Read VBRI Delay
      const std::vector<uint8_t> vbri_delaydata = file->readBlock(2);
      VBRI_delay = ReadBEValue(vbri_delaydata);

      // Read VBRI Quality
      const std::vector<uint8_t> vbri_qualitydata = file->readBlock(2);
      VBRI_quality = ReadBEValue(vbri_qualitydata);

      // Read VBRI number of bytes
      const std::vector<uint8_t> vbri_bytesdata = file->readBlock(4);
      VBRI_bytes = ReadBEValue(vbri_bytesdata);

      // Read VBRI number of frames
      const std::vector<uint8_t> vbri_framesdata = file->readBlock(4);
      VBRI_frames = ReadBEValue(vbri_framesdata);

      // Read VBRI number of TOC entries
      const std::vector<uint8_t> vbri_tocentriesdata = file->readBlock(2);
      VBRI_TOC_entries = ReadBEValue(vbri_tocentriesdata);

      // Read VBRI
      const std::vector<uint8_t> vbri_scaledata = file->readBlock(2);
      VBRI_TOC_scale = ReadBEValue(vbri_scaledata);

      // Read VBRI size per table entry
      const std::vector<uint8_t> vbri_sizedata = file->readBlock(2);
      VBRI_TOC_sizetable = ReadBEValue(vbri_sizedata);

      // Read VBRI frames per table entry
      const std::vector<uint8_t> vbri_frametabledata = file->readBlock(2);
      VBRI_TOC_framestable = ReadBEValue(vbri_frametabledata);

      // Read VBRI TOC
      VBRI_TOC_size = VBRI_TOC_sizetable*VBRI_TOC_entries;
      // const std::vector<uint8_t> vbri_tocdata = file->readBlock(TOC_size);

    }

  }

  // If the frame is bigger then the auctual file..
  if(CurrentHeaderOffset + frame_size > m_FileSize) {
    debug(LOG_ERROR) << "Last frame is truncated. Excpected " << frame_size <<" bytes but is only " << m_FileSize - CurrentHeaderOffset << " bytes"<< std::endl;
    lastframe = true;
    // mpegerrortype = TRUNCATED;
    return 0;

  } else {

    // Check if there is room for a new header, if there is, check it
    if(CurrentHeaderOffset + frame_size + 4 < m_FileSize) {

      file->seek(CurrentHeaderOffset + frame_size);
      const std::vector<uint8_t> nextData = file->readBlock(4);

      if(nextData.size() < 4) {
        debug(LOG_ERROR) << "Could not read next header" << std::endl;
        return 0;
      }

      // The mask to check if another header is valid or not
      const unsigned int HeaderMask = 0xfffe0c00; // sync + MPEG Audio version ID + Layer + Sampling rate
      // Note: MPAHeader has change from Mono to Stero and Emphasis checket also..

      // Mask to our current valid header for checking others with
      const unsigned int header = ReadBEValue(data) & HeaderMask;

      // Make out 32-bit number of the 4 bytes ( with BigEndian )
      unsigned int nextHeader = ReadBEValue(nextData) & HeaderMask;

      if(header != nextHeader) {

        // This does not necessary mean that there is an error, it could also be end of
        // audio section or file

        bool foundNextFrame = false;
        std::vector<uint8_t> remainingbuffer = file->readBlock(m_FileSize - file->tell());

        // Save bitset for showing in debug error log (down)
        std::bitset<4*8> x(header);
        std::bitset<4*8> y(nextHeader);


        // Search for the next valid header
        for(unsigned int i = 0; i < remainingbuffer.size() - 1; ++i) {
          if(isFrameSync(remainingbuffer, i)) {

            std::vector<uint8_t> nextHeaderData = { remainingbuffer.begin()+i, remainingbuffer.begin()+i+4 };

            nextHeader = ReadBEValue(nextHeaderData) & HeaderMask;



            if(header == nextHeader) {
              // If we found another valid header, then this is not the end of file or into TAGs section,
              // "previous" header was missing/wrong place

              offsetToNextValidFrame = CurrentHeaderOffset + frame_size + i + 4;

              foundNextFrame = true;

              // We found another valid header wich mean this header was not the last, and there for is broken
              int lengthToNextValidHeader = offsetToNextValidFrame - CurrentHeaderOffset;
              int excpectedHeaderOffset = CurrentHeaderOffset + frame_size;

              debug(LOG_ERROR) << "Header for frame " << m_FrameIndex+1 << " was excpected @ " << excpectedHeaderOffset << " (0x" << std::hex << excpectedHeaderOffset << std::dec << ") but was found at: " << offsetToNextValidFrame << std::endl;


              debug(LOG_VERBOS_ERROR) << "Frame " << m_FrameIndex << " (offset:" << CurrentHeaderOffset << ") excpected length was " << frame_size << " bytes, but is " << lengthToNextValidHeader << std::endl;

              debug(LOG_VERBOS_ERROR) << "Current Header (" << m_FrameIndex << "): \t" << x << " (masked)" << std::endl;
              debug(LOG_VERBOS_ERROR) << "Wrong Header (" << m_FrameIndex+1 << "): \t" << y << " (masked)" << std::endl;
              // mpegerrortype = SYNC;

              debug(LOG_VERBOS_ERROR) << "Next Valid frame offset: " << offsetToNextValidFrame << " (0x" << std::hex << offsetToNextValidFrame << std::dec << ")" << std::endl;

              m_badframes++;
              m_FrameIndex++;
              break;
            }
          }
        }

        // We did not find another valid frame, maybe end of Audio section or end of file?
        if(!foundNextFrame) {
          offsetToEndOfAudioFrames = CurrentHeaderOffset + frame_size; // Of valid frames
          debug(LOG_DEBUG) << "End of audio section: " << offsetToEndOfAudioFrames << std::endl;
          lastframe = true;
        }


        return 0;
      } // if header != nextHeader

    }


  }

  isHeaderValid = true;
  offsetToNextValidFrame = CurrentHeaderOffset + frame_size;

  return 1;
}

void MP3::printHeaderInformation(int loglevel) {

  switch (version) {
    case 1: debug(loglevel) << "Version: MPEG Version 1" << std::endl; break;
    case 2: debug(loglevel) << "Version: MPEG Version 2" << std::endl; break;
    case 25: debug(loglevel) << "Version: MPEG Version 2.5" << std::endl; break;
  }

  switch (layer) {
    case 1: debug(loglevel) << "Layer: Layer I" << std::endl; break;
    case 2: debug(loglevel) << "Layer: Layer II" << std::endl; break;
    case 3: debug(loglevel) << "Layer: Layer III" << std::endl; break;
  }

  switch (channel_mode) {
    case Stereo: {debug(loglevel) << "ChannelMode: Stereo" << std::endl; break;}
    case JointStereo: {
      debug(loglevel) << "ChannelMode: Joint Stereo" << std::endl;

      // Mode extension is only used in Joint stereo
      switch (layer) {
        case 1: debug(loglevel) << "Mode Extension: bands " << modeExtension[0] << " to " << modeExtension[1] << std::endl; break;
        case 2: debug(loglevel) << "Mode Extension: bands " << modeExtension[0] << " to " << modeExtension[1] <<  std::endl; break;
        case 3: debug(loglevel) << "Mode Extension: Intensity stereo: " << ((modeExtension[0]) ? "On" : "Off") << " MS stereo: " << ((modeExtension[1]) ? "On" : "Off") <<  std::endl; break;
      }
      break;
    }
    case DualChannel: {debug(loglevel) << "ChannelMode: Dual Channel" << std::endl; break;}
    case Mono: {debug(loglevel) << "ChannelMode: Single Channel (Mono)" << std::endl; break;}
  }

  debug(loglevel) << "Bitrate: " << bitrate << " kbps" << std::endl;
  debug(loglevel) << "Sample rate: " << samplingRate << " Hz" << std::endl;
  debug(loglevel) << "Frame size: " << frame_size << " bytes" << std::endl;
  debug(loglevel) << "SamplesPerFrame: " << samplesPerFrame << std::endl;

  switch (emphasis) {
    case 0: { debug(loglevel) << "Emphasis: none" << std::endl; break; }
    case 1: { debug(loglevel) << "Emphasis: 50/15 ms" << std::endl; break; }
    case 2: { debug(loglevel) << "Emphasis: reserved" << std::endl; break; }
    case 3: { debug(loglevel) << "Emphasis: CCIT J.17" << std::endl; break; }
  }



  debug(loglevel) << "Protected by CRC: " << ((protectionEnabled) ? "Yes" : "No") << ((CRCpassed) ? "(passed)" : "") << std::endl;
  debug(loglevel) << "isPadded: " << ((isPadded) ? "Yes" : "No") << std::endl;
  debug(loglevel) << "isCopyrighted: " << ((isCopyrighted) ? "Yes" : "No") << std::endl;
  debug(loglevel) << "isOriginal: " << ((isOriginal) ? "Yes" : "No") << std::endl;

  if(xingHeader) {
    debug(loglevel) << "Xing tag: " << xingString << std::endl;
    if(xing_frames > 0)
      debug(loglevel) << "Xing Number of Frame: " << xing_frames << std::endl;

    if(xing_bytes > 0)
      debug(loglevel) << "Xing Number of Bytes: " << xing_bytes << " (" << xing_bytes/1024.0f << " Kb)" << std::endl;

    if(xing_quality > -1)
      debug(loglevel) << "Xing Quality: " << xing_quality << std::endl;
  }

  if(VBRIHeader) {
    debug(loglevel) << "VBRI Version: " << VBRI_version << std::endl;
    debug(loglevel) << "VBRI Delay: " << VBRI_delay << std::endl;
    debug(loglevel) << "VBRI Quality: " << VBRI_quality << std::endl;
    debug(loglevel) << "VBRI Number of Bytes: " << VBRI_bytes << std::endl;
    debug(loglevel) << "VBRI Number of Frames: " << VBRI_frames << std::endl;
    debug(loglevel) << "VBRI TOC Entries: " << VBRI_TOC_entries << std::endl;
    debug(loglevel) << "VBRI TOC Scale factory of TOC table entries: " << VBRI_TOC_scale << std::endl;
    debug(loglevel) << "VBRI TOC Size per table in bytes: " << VBRI_TOC_sizetable << std::endl;
    debug(loglevel) << "VBRI TOC Frames per table entry: " << VBRI_TOC_framestable << std::endl;
    debug(loglevel) << "VBRI TOC Size: " << VBRI_TOC_size << std::endl;
  }


}


void MP3::InitializeFrame(unsigned char *buffer) {
  debug(LOG_DEBUG) << "->init_frame_params" << std::endl;

  set_side_info(&buffer[protectionEnabled == 0 ? 6 : 4]);
  set_main_data(buffer);
  for (int gr = 0; gr < 2; gr++) {
    for (int ch = 0; ch < channels; ch++)
      requantize(gr, ch);

    if (channel_mode == JointStereo && modeExtension[0])
      ms_stereo(gr);


    for (int ch = 0; ch < channels; ch++) {
      if (block_type[gr][ch] == 2 || mixed_block_flag[gr][ch])
        reorder(gr, ch);
      else
        alias_reduction(gr, ch);

      imdct(gr, ch);
      frequency_inversion(gr, ch);
      synth_filterbank(gr, ch);
    }
  }
  interleave();
}

/**
 * The side information contains information on how to decode the main_data.
 * @param buffer A pointer to the first byte of the side info.
 */
void MP3::set_side_info(unsigned char *buffer)
{
	debug(LOG_DEBUG) <<  "->set_side_info" << std::endl;
	int count = 0;

	/* Number of bytes the main data ends before the next frame header. */
	main_data_begin = (int)get_bits_inc(buffer, &count, 9);

	/* Skip private bits. Not necessary. */
	count += channel_mode == Mono ? 5 : 3;

	for (int ch = 0; ch < channels; ch++)
		for (int scfsi_band = 0; scfsi_band < 4; scfsi_band++)
			/* - Scale factor selection information.
			 * - If scfsi[scfsi_band] == 1, then scale factors for the first
			 *   granule are reused in the second granule.
			 * - If scfsi[scfsi_band] == 0, then each granule has its own scaling factors.
			 * - scfsi_band indicates what group of scaling factors are reused. */
			scfsi[ch][scfsi_band] = get_bits_inc(buffer, &count, 1) != 0;

	for (int gr = 0; gr < 2; gr++)
		for (int ch = 0; ch < channels; ch++) {
			/* Length of the scaling factors and main data in bits. */
			part2_3_length[gr][ch] = (int)get_bits_inc(buffer, &count, 12);
			/* Number of values in each big_region. */
			big_value[gr][ch] = (int)get_bits_inc(buffer, &count, 9);
			/* Quantizer step size. */
			global_gain[gr][ch] = (int)get_bits_inc(buffer, &count, 8);
			/* Used to determine the values of slen1 and slen2. */
			scalefac_compress[gr][ch] = (int)get_bits_inc(buffer, &count, 4);
			/* Number of bits given to a range of scale factors.
			 * - Normal blocks: slen1 0 - 10, slen2 11 - 20
			 * - Short blocks && mixed_block_flag == 1: slen1 0 - 5, slen2 6-11
			 * - Short blocks && mixed_block_flag == 0: */
			slen1[gr][ch] = slen[scalefac_compress[gr][ch]][0];
			slen2[gr][ch] = slen[scalefac_compress[gr][ch]][1];
			/* If set, a not normal window is used. */

			window_switching[gr][ch] = get_bits_inc(buffer, &count, 1) == 1;

			if (window_switching[gr][ch]) {
				/* The window type for the granule.
				 * 0: reserved
				 * 1: start block
				 * 2: 3 short windows
				 * 3: end block */
				block_type[gr][ch] = (int)get_bits_inc(buffer, &count, 2);

				/* Number of scale factor bands before window switching. */
				mixed_block_flag[gr][ch] = get_bits_inc(buffer, &count, 1) == 1;
				if (mixed_block_flag[gr][ch]) {
					switch_point_l[gr][ch] = 8;
					switch_point_s[gr][ch] = 3;
				} else {
					switch_point_l[gr][ch] = 0;
					switch_point_s[gr][ch] = 0;
				}

				/* These are set by default if window_switching. */
				region0_count[gr][ch] = block_type[gr][ch] == 2 ? 8 : 7;
				/* No third region. */
				region1_count[gr][ch] = 20 - region0_count[gr][ch];

				for (int region = 0; region < 2; region++)
					/* Huffman table number for a big region. */
					table_select[gr][ch][region] = (int)get_bits_inc(buffer, &count, 5);
				for (int window = 0; window < 3; window++)
					subblock_gain[gr][ch][window] = (int)get_bits_inc(buffer, &count, 3);
			} else {
				/* Set by default if !window_switching. */
				block_type[gr][ch] = 0;
				mixed_block_flag[gr][ch] = false;

				for (int region = 0; region < 3; region++)
					table_select[gr][ch][region] = (int)get_bits_inc(buffer, &count, 5);

				/* Number of scale factor bands in the first big value region. */
				region0_count[gr][ch] = (int)get_bits_inc(buffer, &count, 4);
				/* Number of scale factor bands in the third big value region. */
				region1_count[gr][ch] = (int)get_bits_inc(buffer, &count, 3);
				/* # scale factor bands is 12*3 = 36 */
			}

			/* If set, add values from a table to the scaling factors. */
			preflag[gr][ch] = (int)get_bits_inc(buffer, &count, 1);
			/* Determines the step size. */
			scalefac_scale[gr][ch] = (int)get_bits_inc(buffer, &count, 1);
			/* Table that determines which count1 table is used. */
			count1table_select[gr][ch] = (int)get_bits_inc(buffer, &count, 1);
		}
}

/**
 * Due to the Huffman bits' varying length the main_data isn't aligned with the
 * frames. Unpacks the scaling factors and quantized samples.
 * @param buffer A buffer that points to the the first byte of the frame header.
 */
void MP3::set_main_data(unsigned char *buffer)
{
	debug(LOG_DEBUG) << "->set_main_data" << std::endl;
	/* header + side_information */
	int constant = channel_mode == Mono ? 21 : 36;
	if (protectionEnabled == 0) // protectionEnabled var crc förut
		constant += 2;

	/* Let's put the main data in a separate buffer so that side info and header
	 * don't interfere. The main_data_begin may be larger than the previous frame
	 * and doesn't include the size of side info and headers. */

	if (main_data_begin == 0) {

		// std::cout << "Resezing buffer.." << std::endl;
		main_data.resize(frame_size - constant);
		memcpy(&main_data[0], buffer + constant, frame_size - constant);

	} else {
		int bound = 0;
		for (int frame = 0; frame < num_prev_frames; frame++) {

			bound += prev_frame_size[frame] - constant;

			if (main_data_begin < bound) {
				int ptr_offset = main_data_begin + frame * constant;
				int buffer_offset = 0;

				int part[num_prev_frames];
				part[frame] = main_data_begin;

				for (int i = 0; i <= frame-1; i++) {
					part[i] = prev_frame_size[i] - constant;
					part[frame] -= part[i];
				}

				main_data.resize(frame_size - constant + main_data_begin);

				memcpy(main_data.data(), buffer - ptr_offset, part[frame]);
				ptr_offset -= (part[frame] + constant);
				buffer_offset += part[frame];
				for (int i = frame-1; i >= 0; i--) {
					memcpy(&main_data[buffer_offset], buffer - ptr_offset, part[i]);
					ptr_offset -= (part[i] + constant);
					buffer_offset += part[i];
				}
				memcpy(&main_data[main_data_begin], buffer + constant, frame_size - constant);
				break;
			}
		}
	}

	int bit = 0;
	for (int gr = 0; gr < 2; gr++)
		for (int ch = 0; ch < channels; ch++) {
			int max_bit = bit + part2_3_length[gr][ch];
			unpack_scalefac(main_data.data(), gr, ch, bit);
			unpack_samples(main_data.data(), gr, ch, bit, max_bit);
			bit = max_bit;
		}
}

/**
 * This will get the scale factor indices from the main data. slen1 and slen2
 * represent the size in bits of each scaling factor. There are a total of 21 scaling
 * factors for long windows and 12 for each short window.
 * @param main_data Buffer solely containing the main_data - excluding the frame header and side info.
 * @param gr
 * @param ch
 */
void MP3::unpack_scalefac(unsigned char *main_data, int gr, int ch, int &bit)
{
	debug(LOG_DEBUG) << "->unpack_scalefac" << std::endl;
	int sfb = 0;
	int window = 0;
	int scalefactor_length[2] {
		slen[scalefac_compress[gr][ch]][0],
		slen[scalefac_compress[gr][ch]][1]
	};

	/* No scale factor transmission for short blocks. */
	if (block_type[gr][ch] == 2 && window_switching[gr][ch]) {
		if (mixed_block_flag[gr][ch] == 1) { /* Mixed blocks. */
			for (sfb = 0; sfb < 8; sfb++)
				scalefac_l[gr][ch][sfb] = (int)get_bits_inc(main_data, &bit, scalefactor_length[0]);

			for (sfb = 3; sfb < 6; sfb++)
				for (window = 0; window < 3; window++)
					scalefac_s[gr][ch][window][sfb] = (int)get_bits_inc(main_data, &bit, scalefactor_length[0]);
		} else /* Short blocks. */
			for (sfb = 0; sfb < 6; sfb++)
				for (window = 0; window < 3; window++)
					scalefac_s[gr][ch][window][sfb] = (int)get_bits_inc(main_data, &bit, scalefactor_length[0]);

		for (sfb = 6; sfb < 12; sfb++)
			for (window = 0; window < 3; window++)
				scalefac_s[gr][ch][window][sfb] = (int)get_bits_inc(main_data, &bit, scalefactor_length[1]);

		for (window = 0; window < 3; window++)
			scalefac_s[gr][ch][window][12] = 0;
	}

	/* Scale factors for long blocks. */
	else {
		if (gr == 0) {
			for (sfb = 0; sfb < 11; sfb++)
				scalefac_l[gr][ch][sfb] = (int)get_bits_inc(main_data, &bit, scalefactor_length[0]);
			for (; sfb < 21; sfb++)
				scalefac_l[gr][ch][sfb] = (int)get_bits_inc(main_data, &bit, scalefactor_length[1]);
		} else {
			/* Scale factors might be reused in the second granule. */
			const int sb[5] = {6, 11, 16, 21};
			for (int i = 0; i < 2; i++)
				for (; sfb < sb[i]; sfb++) {
					if (scfsi[ch][i])
						scalefac_l[gr][ch][sfb] = scalefac_l[0][ch][sfb];
					else
						scalefac_l[gr][ch][sfb] = (int)get_bits_inc(main_data, &bit, scalefactor_length[0]);

				}
			for (int i = 2; i < 4; i++)
				for (; sfb < sb[i]; sfb++) {
					if (scfsi[ch][i])
						scalefac_l[gr][ch][sfb] = scalefac_l[0][ch][sfb];
					else
						scalefac_l[gr][ch][sfb] = (int)get_bits_inc(main_data, &bit, scalefactor_length[1]);
				}
		}
		scalefac_l[gr][ch][21] = 0;
	}
}

/**
 * The Huffman bits (part3) will be unpacked. Four bytes are retrieved from the
 * bit stream, and are consecutively evaluated against values of the selected Huffman
 * tables.
 * | big_value | big_value | big_value | quadruple | zero |
 * Each hit gives two samples.
 * @param main_data Buffer solely containing the main_data excluding the frame header and side info.
 * @param gr
 * @param ch
 */
void MP3::unpack_samples(unsigned char *main_data, int gr, int ch, int bit, int max_bit)
{
	debug(LOG_DEBUG) << "->unpack_samples" << std::endl;
	int sample = 0;
	int table_num;
	const unsigned *table;

	for (int i = 0; i < 576; i++)
		samples[gr][ch][i] = 0;

	/* Get the big value region boundaries. */
	int region0;
	int region1;
	if (window_switching[gr][ch] && block_type[gr][ch] == 2) {
		region0 = 36;
		region1 = 576;
	} else {
		region0 = band_index.long_win[region0_count[gr][ch] + 1];
		region1 = band_index.long_win[region0_count[gr][ch] + 1 + region1_count[gr][ch] + 1];
	}

	/* Get the samples in the big value region. Each entry in the Huffman tables
	 * yields two samples. */
	for (; sample < big_value[gr][ch] * 2; sample += 2) {
		if (sample < region0) {
			table_num = table_select[gr][ch][0];
			table = big_value_table[table_num];
		} else if (sample < region1) {
			table_num = table_select[gr][ch][1];
			table = big_value_table[table_num];
		} else {
			table_num = table_select[gr][ch][2];
			table = big_value_table[table_num];
		}

		if (table_num == 0) {
			samples[gr][ch][sample] = 0;
			continue;
		}

		bool repeat = true;
		unsigned bit_sample = get_bits(main_data, bit, bit + 32);

		/* Cycle through the Huffman table and find a matching bit pattern. */
		for (int row = 0; row < big_value_max[table_num] && repeat; row++)
			for (int col = 0; col < big_value_max[table_num]; col++) {
				int i = 2 * big_value_max[table_num] * row + 2 * col;
				unsigned value = table[i];
				unsigned size = table[i + 1];
				if (value >> (32 - size) == bit_sample >> (32 - size)) {
					bit += size;

					int values[2] = {row, col};
					for (int i = 0; i < 2; i++) {

						/* linbits extends the sample's size if needed. */
						int linbit = 0;
						if (big_value_linbit[table_num] != 0 && values[i] == big_value_max[table_num] - 1)
							linbit = (int)get_bits_inc(main_data, &bit, big_value_linbit[table_num]);

						/* If the sample is negative or positive. */
						int sign = 1;
						if (values[i] > 0)
							sign = get_bits_inc(main_data, &bit, 1) ? -1 : 1;

						samples[gr][ch][sample + i] = (float)(sign * (values[i] + linbit));
					}

					repeat = false;
					break;
				}
			}
	}

	/* Quadruples region. */
	for (; bit < max_bit && sample + 4 < 576; sample += 4) {
		int values[4];

		/* Flip bits. */
		if (count1table_select[gr][ch] == 1) {
			unsigned bit_sample = get_bits_inc(main_data, &bit, 4);
			values[0] = (bit_sample & 0x08) > 0 ? 0 : 1;
			values[1] = (bit_sample & 0x04) > 0 ? 0 : 1;
			values[2] = (bit_sample & 0x02) > 0 ? 0 : 1;
			values[3] = (bit_sample & 0x01) > 0 ? 0 : 1;
		} else {
			unsigned bit_sample = get_bits(main_data, bit, bit + 32);
			for (int entry = 0; entry < 16; entry++) {
				unsigned value = quad_table_1.hcod[entry];
				unsigned size = quad_table_1.hlen[entry];

				if (value >> (32 - size) == bit_sample >> (32 - size)) {
					bit += size;
					for (int i = 0; i < 4; i++)
						values[i] = (int)quad_table_1.value[entry][i];
					break;
				}
			}
		}

		/* Get the sign bit. */
		for (int i = 0; i < 4; i++)
			if (values[i] > 0 && get_bits_inc(main_data, &bit, 1) == 1)
				values[i] = -values[i];

		for (int i = 0; i < 4; i++)
			samples[gr][ch][sample + i] = values[i];
	}

	/* Fill remaining samples with zero. */
	for (; sample < 576; sample++)
		samples[gr][ch][sample] = 0;
}

/**
 * The reduced samples are rescaled to their original scales and precisions.
 * @param gr
 * @param ch
 */
void MP3::requantize(int gr, int ch)
{
	debug(LOG_DEBUG) << "->requantize" << std::endl;
	float exp1, exp2;
	int window = 0;
	int sfb = 0;
	const float scalefac_mult = scalefac_scale[gr][ch] == 0 ? 0.5 : 1;

	for (int sample = 0, i = 0; sample < 576; sample++, i++) {
		if (block_type[gr][ch] == 2 || (mixed_block_flag[gr][ch] && sfb >= 8)) {
			if (i == band_width.short_win[sfb]) {
				i = 0;
				if (window == 2) {
					window = 0;
					sfb++;
				} else
					window++;
			}

			exp1 = global_gain[gr][ch] - 210.0 - 8.0 * subblock_gain[gr][ch][window];
			exp2 = scalefac_mult * scalefac_s[gr][ch][window][sfb];
		} else {
			if (sample == band_index.long_win[sfb + 1])
				/* Don't increment sfb at the zeroth sample. */
				sfb++;

			exp1 = global_gain[gr][ch] - 210.0;
			exp2 = scalefac_mult * (scalefac_l[gr][ch][sfb] + preflag[gr][ch] * pretab[sfb]);
		}

		float sign = samples[gr][ch][sample] < 0 ? -1.0f : 1.0f;
		float a = std::pow(std::abs(samples[gr][ch][sample]), 4.0 / 3.0);
		float b = std::pow(2.0, exp1 / 4.0);
		float c = std::pow(2.0, -exp2);

		samples[gr][ch][sample] = sign * a * b * c;
	}
}

/**
 * Reorder short blocks, mapping from scalefactor subbands (for short windows) to 18 sample blocks.
 * @param gr
 * @param ch
 */
void MP3::reorder(int gr, int ch)
{
	debug(LOG_DEBUG) << "->reorder" << std::endl;
	int total = 0;
	int start = 0;
	int block = 0;
	float samples[576] = {0};

	for (int sb = 0; sb < 12; sb++) {
		const int sb_width = band_width.short_win[sb];

		for (int ss = 0; ss < sb_width; ss++) {
			samples[start + block + 0] = this->samples[gr][ch][total + ss + sb_width * 0];
			samples[start + block + 6] = this->samples[gr][ch][total + ss + sb_width * 1];
			samples[start + block + 12] = this->samples[gr][ch][total + ss + sb_width * 2];

			if (block != 0 && block % 5 == 0) { /* 6 * 3 = 18 */
				start += 18;
				block = 0;
			} else
				block++;
		}

		total += sb_width * 3;
	}

	for (int i = 0; i < 576; i++)
		this->samples[gr][ch][i] = samples[i];
}

/**
 * The left and right channels are added together to form the middle channel. The
 * difference between each channel is stored in the side channel.
 * @param gr
 */
void MP3::ms_stereo(int gr)
{
	debug(LOG_DEBUG) << "->ms_stereo (arguments " << gr << ")" << std::endl;

	for (int sample = 0; sample < 576; sample++) {
		float middle = samples[gr][0][sample];
		float side = samples[gr][1][sample];
		samples[gr][0][sample] = (middle + side) / SQRT2;
		samples[gr][1][sample] = (middle - side) / SQRT2;
	}
}

/**
 * @param gr
 * @param ch
 */
void MP3::alias_reduction(int gr, int ch)
{
	debug(LOG_DEBUG) << "->alias_reduction (arguments: " << gr << "," << ch << ")" << std::endl;

	static const float cs[8] {
			.8574929257, .8817419973, .9496286491, .9833145925,
			.9955178161, .9991605582, .9998991952, .9999931551
	};
	static const float ca[8] {
			-.5144957554, -.4717319686, -.3133774542, -.1819131996,
			-.0945741925, -.0409655829, -.0141985686, -.0036999747
	};

	int sb_max = mixed_block_flag[gr][ch] ? 2 : 32;

	for (int sb = 1; sb < sb_max; sb++)
		for (int sample = 0; sample < 8; sample++) {
			int offset1 = 18 * sb - sample - 1;
			int offset2 = 18 * sb + sample;
			float s1 = samples[gr][ch][offset1];
			float s2 = samples[gr][ch][offset2];
			samples[gr][ch][offset1] = s1 * cs[sample] - s2 * ca[sample];
			samples[gr][ch][offset2] = s2 * cs[sample] + s1 * ca[sample];
		}
}

/**
 * Inverted modified discrete cosine transformations (IMDCT) are applied to each
 * sample and are afterwards windowed to fit their window shape. As an addition, the
 * samples are overlapped.
 * @param gr
 * @param ch
 */
void MP3::imdct(int gr, int ch)
{
	debug(LOG_DEBUG) << "->imdct (arguments " << gr << "," << ch << ")" << std::endl;

	static bool init = true;
	static float sine_block[4][36];
	float sample_block[36];

	if (init) {
		int i;
		for (i = 0; i < 36; i++)
			sine_block[0][i] = std::sin(PI / 36.0 * (i + 0.5));
		for (i = 0; i < 18; i++)
			sine_block[1][i] = std::sin(PI / 36.0 * (i + 0.5));
		for (; i < 24; i++)
			sine_block[1][i] = 1.0;
		for (; i < 30; i++)
			sine_block[1][i] = std::sin(PI / 12.0 * (i - 18.0 + 0.5));
		for (; i < 36; i++)
			sine_block[1][i] = 0.0;
		for (i = 0; i < 12; i++)
			sine_block[2][i] = std::sin(PI / 12.0 * (i + 0.5));
		for (i = 0; i < 6; i++)
			sine_block[3][i] = 0.0;
		for (; i < 12; i++)
			sine_block[3][i] = std::sin(PI / 12.0 * (i - 6.0 + 0.5));
		for (; i < 18; i++)
			sine_block[3][i] = 1.0;
		for (; i < 36; i++)
			sine_block[3][i] = std::sin(PI / 36.0 * (i + 0.5));
		init = false;
	}

	const int n = block_type[gr][ch] == 2 ? 12 : 36;
	const int half_n = n / 2;
	int sample = 0;

	for (int block = 0; block < 32; block++) {
		for (int win = 0; win < (block_type[gr][ch] == 2 ? 3 : 1); win++) {
			for (int i = 0; i < n; i++) {
				float xi = 0.0;
				for (int k = 0; k < half_n; k++) {
					float s = samples[gr][ch][18 * block + half_n * win + k];
					xi += s * std::cos(PI / (2 * n) * (2 * i + 1 + half_n) * (2 * k + 1));
				}

				/* Windowing samples. */
				sample_block[win * n + i] = xi * sine_block[block_type[gr][ch]][i];
			}
		}

		if (block_type[gr][ch] == 2) {
			float temp_block[36];
			memcpy(temp_block, sample_block, 36 * 4);

			int i = 0;
			for (; i < 6; i++)
				sample_block[i] = 0;
			for (; i < 12; i++)
				sample_block[i] = temp_block[0 + i - 6];
			for (; i < 18; i++)
				sample_block[i] = temp_block[0 + i - 6] + temp_block[12 + i - 12];
			for (; i < 24; i++)
				sample_block[i] = temp_block[12 + i - 12] + temp_block[24 + i - 18];
			for (; i < 30; i++)
				sample_block[i] = temp_block[24 + i - 18];
			for (; i < 36; i++)
				sample_block[i] = 0;
		}

		/* Overlap. */
		for (int i = 0; i < 18; i++) {
			samples[gr][ch][sample + i] = sample_block[i] + prev_samples[ch][block][i];
			prev_samples[ch][block][i] = sample_block[18 + i];
		}
		sample += 18;
	}
}

/**
 * @param gr
 * @param ch
 */
void MP3::frequency_inversion(int gr, int ch)
{
	debug(LOG_DEBUG) << "->frequency_inversion (arguments " << gr << "," << ch << ")" << std::endl;
	for (int sb = 1; sb < 18; sb += 2)
		for (int i = 1; i < 32; i += 2)
			samples[gr][ch][i * 18 + sb] *= -1;
}

/**
 * @param gr
 * @param ch
 */
void MP3::synth_filterbank(int gr, int ch)
{
	debug(LOG_DEBUG) << "->synth_filterbank (arguments " << gr << "," << ch << ")" << std::endl;
	static float n[64][32];
	static bool init = true;

	if (init) {
		init = false;
		for (int i = 0; i < 64; i++)
			for (int j = 0; j < 32; j++)
				n[i][j] = std::cos((16.0 + i) * (2.0 * j + 1.0) * (PI / 64.0));
	}

	float s[32], u[512], w[512];
	float pcm[576];

	for (int sb = 0; sb < 18; sb++) {
		for (int i = 0; i < 32; i++)
			s[i] = samples[gr][ch][i * 18 + sb];

		for (int i = 1023; i > 63; i--)
			fifo[ch][i] = fifo[ch][i - 64];

		for (int i = 0; i < 64; i++) {
			fifo[ch][i] = 0.0;
			for (int j = 0; j < 32; j++)
				fifo[ch][i] += s[j] * n[i][j];
		}

		for (int i = 0; i < 8; i++)
			for (int j = 0; j < 32; j++) {
				u[i * 64 + j] = fifo[ch][i * 128 + j];
				u[i * 64 + j + 32] = fifo[ch][i * 128 + j + 96];
			}

		for (int i = 0; i < 512; i++)
			w[i] = u[i] * synth_window[i];

		for (int i = 0; i < 32; i++) {
			float sum = 0;
			for (int j = 0; j < 16; j++)
				sum += w[j * 32 + i];
			pcm[32 * sb + i] = sum;
		}
	}

	memcpy(samples[gr][ch], pcm, 576 * 4);
}

void MP3::interleave()
{
	debug(LOG_DEBUG) << "->interleave" << std::endl;
	int i = 0;
	for (int gr = 0; gr < 2; gr++)
		for (int sample = 0; sample < 576; sample++)
			for (int ch = 0; ch < channels; ch++)
				pcm[i++] = samples[gr][ch][sample];

}
