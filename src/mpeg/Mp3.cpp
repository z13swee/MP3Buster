#include "Mp3.hpp"

#define PI    3.141592653589793
#define SQRT2 1.414213562373095

#define MAD_TIMER_RESOLUTION	352800000UL

MP3::MP3(std::filesystem::path path, myConfig& config)
{

  cfg = config;
  m_File = new FileWrapper(path);


  if(m_File->isValid() && m_File->getSize() != 0) {
    m_FileSize = m_File->getSize();

    // Batchmode !
    if(cfg.batchmode) {


      GlobalLogLevel = LOG_SILENT;
      Analyze();

      GlobalLogLevel = cfg.loglevel;
      BatchmodeOutput(path, cfg);


    } else {

      // Single file
      debug(LOG_INFO) << "File: " << path.filename().string() << std::endl;
      Analyze();
      std::cout << std::endl;
    }

  } else {
    debug(LOG_INFO) << std::left << CONSOLE_COLOR_RED << path.filename().string() << "\tInvalid file!" << CONSOLE_COLOR_NORMAL << std::endl;
    errors = true;
  }
}

MP3::~MP3()
{
  if(m_File)
    delete m_File;
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
  debug(LOG_VERBOSE) << "Looking for Tags at offset: 0" << std::endl;

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
  debug(LOG_VERBOSE) << "Looking for Tags at offset: " << offset << std::endl;

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


void MP3::EstimateDuration()
{
  // The calculation for VBR MP3 is a little complicated:
  // Duration (seconds) = Samples Per Frame * Total Frames (samples) / Sample Rate (samples/second)

  // The duration is not accurate when the total frames above is an estimated value.
  // If the total frames isn’t predefined, then it could be estimated by:

  // Estimated Total Frames=File Size / Average Frame Size


  if (mpegHeader.VBRIHeader && mpegHeader.VBRI_frames != -1)
  {
    if (mpegHeader.VBRI_bytes == -1) {
      debug(LOG_WARNING) << "VBRI header does not include byte size. Using filesize instead." << std::endl;
      mpegHeader.VBRI_bytes = m_File->getSize();
    }

    // if (mpegHeader.VBRI_frames == -1)
    //   mpegHeader.VBRI_frames = m_File->getSize() / Average frame size??;

    estimatedDuration.raw = mpegHeader.samplesPerFrame * mpegHeader.VBRI_frames / mpegHeader.samplingRate;
    estimatedDuration.minutes = estimatedDuration.raw  / 60;
    estimatedDuration.seconds = (int)estimatedDuration.raw  % 60;

    debug(LOG_INFO) << "Estimate duration (VBRI): " << std::setfill('0') << std::setw(2) << estimatedDuration.minutes << ":"
                                                    << std::setfill('0') << std::setw(2) << estimatedDuration.seconds << std::endl;
    return;
  }

  if (mpegHeader.xingHeader && mpegHeader.xing_frames != -1)
  {
    if (mpegHeader.xing_bytes == -1) {
      debug(LOG_WARNING) << "Xing header does not include byte size. Using filesize instead." << std::endl;
      mpegHeader.xing_bytes = m_File->getSize();
    }

    // if (mpegHeader.VBRI_frames == -1)
    //   mpegHeader.VBRI_frames = m_File->getSize() / Average frame size??;

    estimatedDuration.raw  = mpegHeader.samplesPerFrame * mpegHeader.xing_frames / mpegHeader.samplingRate;
    estimatedDuration.minutes = estimatedDuration.raw  / 60;
    estimatedDuration.seconds = (int)estimatedDuration.raw  % 60;

    debug(LOG_INFO) << "Estimate duration (XING): " << std::setfill('0') << std::setw(2) << estimatedDuration.minutes << ":"
                                                    << std::setfill('0') << std::setw(2) << estimatedDuration.seconds << std::endl;
    return;
  }

  // The calculation for CBR (ConstantBitRate) MP3 is straightforward:
  // Duration (seconds) = File Size (bits) / Bitrate (bits/second)
  // This estimate can be trhouwn off by ex. tags?

  unsigned FileSizeInBits = m_File->getSize()*8;
  estimatedDuration.raw  = FileSizeInBits  / (mpegHeader.bitrate*1000);
  estimatedDuration.minutes = estimatedDuration.raw  / 60;
  estimatedDuration.seconds = (int)estimatedDuration.raw  % 60;

  debug(LOG_INFO) << "Estimate duration (CBR): " << std::setfill('0') << std::setw(2) << estimatedDuration.minutes  << ":"
                                                 << std::setfill('0') << std::setw(2) << estimatedDuration.seconds << std::endl;

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
    debug(LOG_VERBOSE) << "Starting search for valid MPEG frame at: " << endOfStartTagsOffset << std::endl;

    if(lastframe)
      break;

    if(isFrameSync(m_filebuffer, i)) {
      CurrentHeaderOffset = i;

      while (InitializeHeader(m_File)) {


        // For first valid header, setup audio output
        if(m_FrameIndex == 1) {
          // logger::GlobalLogLevel = LOG_INFO;

          firstvalidframe = i;
          debug(LOG_VERBOSE) << "First valid frame found at: " << firstvalidframe << std::endl;

          EstimateDuration();

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
            if (snd_pcm_hw_params_set_channels(handle, hw, mpegHeader.channels) < 0)
              exit(1);

            unsigned smplRate = (unsigned)mpegHeader.samplingRate;
            if (snd_pcm_hw_params_set_rate_near(handle, hw, &smplRate, NULL) < 0)
              exit(1);


            if (snd_pcm_hw_params_set_period_size_near(handle, hw, &frames, NULL) < 0)
              exit(1);
            if (snd_pcm_hw_params(handle, hw) < 0)
              exit(1);
            if (snd_pcm_hw_params_get_period_size(hw, &frames, NULL) < 0)
              exit(1);


            if (snd_pcm_hw_params_get_period_time(hw, &smplRate, NULL) < 0)
              exit(1);
          }

        }

        // Only works if we read the whole file
        calculatedDuration.raw += ((float)mpegHeader.samplesPerFrame / (float)mpegHeader.samplingRate);

        // Decode frame data
        if(cfg.playback) {
          debug(LOG_VERBOSE) <<  "init_frame_params @ " << CurrentHeaderOffset << std::endl;
          InitializeFrame(&m_filebuffer[CurrentHeaderOffset]);

          debug(LOG_VERBOSE) << "\rPlaying frame: " << m_FrameIndex << " @ " << CurrentHeaderOffset;
          int e = snd_pcm_writei(handle, mpegFrame.pcm, 1152);
          if (e == -EPIPE)
            snd_pcm_recover(handle, e, 0);
        }

        CurrentHeaderOffset = offsetToNextValidFrame;
        m_FrameIndex++;
      }

      // IF we got here and it was NOT the last frame, the while looped exit becuse
      // of an error
      if(!lastframe)
        errors = true;

      // Stop on Error
      if(cfg.stoponerror && errors)
        break;

      // Jump forward to where 'while' loop ended
      if(offsetToNextValidFrame > CurrentHeaderOffset)
        i = offsetToNextValidFrame - 1;
      else
        i = CurrentHeaderOffset;

    }
  }

  // Show calculated duration if we have read the whole file with no errors, or
  // with errors when cfg.stoponerror is set to false
  if(cfg.stoponerror && !errors || !cfg.stoponerror) {
    calculatedDuration.hours = (calculatedDuration.raw / 60.f) / 60;
    calculatedDuration.seconds = (int)calculatedDuration.raw % 60;
    calculatedDuration.minutes = calculatedDuration.raw / 60;

    if(calculatedDuration.hours > 0) {
      debug(LOG_INFO) << "Calculated duration: " << std::setfill('0') << std::setw(2) << calculatedDuration.hours << ":"
                                                  << std::setfill('0') << std::setw(2) << calculatedDuration.minutes << ":"
                                                  << std::setfill('0') << std::setw(2) << calculatedDuration.seconds << std::endl;
    } else {
      debug(LOG_INFO) << "Calculated duration: "  << std::setfill('0') << std::setw(2) << calculatedDuration.minutes << ":"
                                                  << std::setfill('0') << std::setw(2) << calculatedDuration.seconds << std::endl;
    }
  }

  // If offsetToEndOfAudioFrames is not 0, check for end tags att the remaining bytes of the file
  if(offsetToEndOfAudioFrames > 0)
    FindEndTags(offsetToEndOfAudioFrames);


  // Found tags..
  if(id3v1_offset)
    debug(LOG_INFO) << "ID3v1 Tag Found at " << id3v1_offset << std::endl;
  if(foundID3v2){
    debug(LOG_INFO) << "ID3v2 Tag Found at " << id3v2_offset << std::endl;
    // id3v2->Render(cfg.loglevel);
  }

  if(apev1_offset)
    debug(LOG_INFO) << "APEv1 Tag Found at " << apev1_offset << std::endl;
  if(foundAPEv2)
    debug(LOG_INFO) << "APEv2 Tag Found at " << apev2_offset << std::endl;

  if(!id3v1_offset && !foundID3v2 && !apev1_offset && !foundAPEv2)
    debug(LOG_INFO) << "No tags found" << std::endl;

  // Clearify that numver of frames is unitil stopped if that options is used.
  if(cfg.stoponerror) {
    if(lastframe)
      debug(LOG_INFO) << "Number of frames: " << m_FrameIndex << std::endl;
    else
      debug(LOG_INFO) << "Number of frames processed (befor hitting a bad frame): " << m_FrameIndex << std::endl;

  } else {
    debug(LOG_INFO) << "Number of frames: " << m_FrameIndex << std::endl;
    debug(LOG_INFO) << "Number of bad frames:" << m_badframes << std::endl;
  }


  if(handle) {
    snd_pcm_drain(handle);
    snd_pcm_close(handle);
  }
}

int MP3::InitializeHeader(FileWrapper* file) {
  // std::cout << "InitializeHeader @ " << CurrentHeaderOffset << " (hex: " << std::hex << CurrentHeaderOffset << std::dec << ")" << " Frame: " << m_FrameIndex << std::endl;
  debug(LOG_VERBOSE) << CONSOLE_COLOR_GOLD << "InitializeHeader @ " << CurrentHeaderOffset << " (hex: " << std::hex << CurrentHeaderOffset << std::dec << ")" << "\033[0;0m" << " Frame: " << m_FrameIndex << std::endl;

  mpegHeader.isHeaderValid = false;

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
    case 0: mpegHeader.version = 25; break;
    case 1: mpegHeader.version = -1; debug(LOG_WARNING) << "Reserved version, likly bad frame?" << std::endl; break;
    case 2: mpegHeader.version = 2; break;
    case 3: mpegHeader.version = 1; break;
    default: debug(LOG_ERROR) << "No matching version!, " << versionBits << std::endl;
    // mpegerrortype = FRAMEHEADER;
  }


  // Set the MPEG layer
  // -----------------------
  const int layerBits = (static_cast<unsigned char>(data[1]) >> 1) & 0x03;

  switch (layerBits) {
    case 0: debug(LOG_WARNING) << "Reserved layer, likly bad frame? " << layerBits <<  " (Frame: " << m_FrameIndex << ")" << std::endl;
    case 1: mpegHeader.layer = 3; break;
    case 2: mpegHeader.layer = 2; break;
    case 3: mpegHeader.layer = 1; break;
    default: debug(LOG_ERROR) << "No matching layer!, " << layerBits << std::endl;
    // mpegerrortype = FRAMEHEADER;
  }


  // CRC check
  bool checkCRC = false;
  std::vector<uint8_t> crc_checksum;

  // Tricky, reading 0 actually means it has CRC
  mpegHeader.protectionEnabled = (static_cast<unsigned char>(data[1] & 0x01) == 0);

  if(mpegHeader.protectionEnabled) {
    crc_checksum = file->readBlock(2);
    // crc_checksum = {filebuffer.begin() + offset + 4, filebuffer.begin() + offset + 4 + 2};
    // debug(LOG_VERBOSE) << "CRC for this frame: " << std::hex << std::uppercase << (int)crc_checksum[0] << (int)crc_checksum[1] << std::dec << std::endl;
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

  const int versionIndex = (mpegHeader.version == 1) ? 0 : 1;
  const int layerIndex   = (mpegHeader.layer > 0) ? mpegHeader.layer - 1 : 0;

  // The bitrate index is encoded as the first 4 bits of the 3rd byte,
  // i.e. 1111xxxx

  const int bitrateIndex = (static_cast<unsigned char>(data[2]) >> 4) & 0x0F;
  mpegHeader.bitrate = bitrates[versionIndex][layerIndex][bitrateIndex];


  // if(prevBitrate == -1 )
  //   prevBitrate = mpegHeader.bitrate;

  // std::cout << "Bitrate: " << mpegHeader.bitrate << " prevBitrate;" << prevBitrate << std::endl;
  // if(mpegHeader.bitrate != prevBitrate && !isVBRI) {
  //   std::cout << "This is a VBRI mp3!" << std::endl;
  //   isVBRI = true;
  // }

  // prevBitrate = mpegHeader.bitrate;

  // Guessing bitrate can never be 0
  if(mpegHeader.bitrate == 0) {
    debug(LOG_ERROR) << "Error parsing bitrate, bitrateIndex: " << bitrateIndex << " (Frame: " << m_FrameIndex << ")" << std::endl;
    // mpegerrortype = FRAMEHEADER;
    return 0;
  }



  // Set the sample/frequency rate
  static const int sampleRates[3][4] = {
    { 44100, 48000, 32000, 0 }, // Version 1
    { 22050, 24000, 16000, 0 }, // Version 2
    { 11025, 12000, 8000,  0 }  // Version 2.5
  };

  // The sample rate index is encoded as two bits in the 3nd byte, i.e. xxxx11xx

  const int samplerateIndex = (static_cast<unsigned char>(data[2]) >> 2) & 0x03;

  // Ugly.. i know
  if(mpegHeader.version == 25)
    mpegHeader.samplingRate = sampleRates[2][samplerateIndex];
  else
    mpegHeader.samplingRate = sampleRates[mpegHeader.version-1][samplerateIndex];



  if(mpegHeader.samplingRate <= 0) {
    debug(LOG_ERROR) << "Error parsing sampleRate"  << std::endl;
    // mpegerrortype = FRAMEHEADER;
    return 0;
  }



  // Set the tables
  // -----------------------
  // During the decoding process different tables are used depending on the sampling rate.
  switch (mpegHeader.samplingRate) {
		case 32000:
			mpegHeader.band_index.short_win = band_index_table.short_32;
			mpegHeader.band_width.short_win = band_width_table.short_32;
			mpegHeader.band_index.long_win = band_index_table.long_32;
			mpegHeader.band_width.long_win = band_width_table.long_32;
			break;
		case 44100:
			mpegHeader.band_index.short_win = band_index_table.short_44;
			mpegHeader.band_width.short_win = band_width_table.short_44;
			mpegHeader.band_index.long_win = band_index_table.long_44;
			mpegHeader.band_width.long_win = band_width_table.long_44;
			break;
		case 48000:
			mpegHeader.band_index.short_win = band_index_table.short_48;
			mpegHeader.band_width.short_win = band_width_table.short_48;
			mpegHeader.band_index.long_win = band_index_table.long_48;
			mpegHeader.band_width.long_win = band_width_table.long_48;
			break;
	}



  // The channel mode is encoded as a 2 bit value at the end of the 3nd byte,
  // i.e. xxxxxx11

  // mpegHeader.channel_mode = static_cast<ChannelMode>((static_cast<unsigned char>(data[3]) >> 6) & 0x03);
  mpegHeader.channel_mode = (static_cast<unsigned char>(data[3]) >> 6) & 0x03;

  // channel_mode = static_cast<ChannelMode>(value);
	mpegHeader.channels = mpegHeader.channel_mode == 3 ? 1 : 2; // 3 = Mono



  // Mode extension
  static const int modeExtensionTable[2][4][2] = {
    { {4,31}, {8,31}, {12,31}, {16,31} }, // Version 1 & 2
    { {0,0}, {1,0}, {0,1}, {1,1} }, // Version 3
  };

  // const int modeExtensionIndex = static_cast<ChannelMode>((static_cast<unsigned char>(data[3]) >> 4) & 0x03);
  const int modeExtensionIndex = (static_cast<unsigned char>(data[3]) >> 4) & 0x03;
  const int modelayerIndex   = (mpegHeader.layer > 2) ? 1 : 0;

  mpegHeader.modeExtension[0] = modeExtensionTable[modelayerIndex][modeExtensionIndex][0];
  mpegHeader.modeExtension[1] = modeExtensionTable[modelayerIndex][modeExtensionIndex][1];



  // Flags
  mpegHeader.isCopyrighted = ((static_cast<unsigned char>(data[3]) & 0x08) != 0);
  mpegHeader.isOriginal    = ((static_cast<unsigned char>(data[3]) & 0x04) != 0);
  mpegHeader.isPadded      = ((static_cast<unsigned char>(data[2]) & 0x02) != 0);

  mpegHeader.emphasis = (data[3] << 6) >> 6;

  // Samples per frame
  static const int samplesPerFrameList[3][2] = {
    // MPEG1, 2/2.5
    {  384,   384 }, // Layer I
    { 1152,  1152 }, // Layer II
    { 1152,   576 }  // Layer III
  };

  mpegHeader.samplesPerFrame = samplesPerFrameList[layerIndex][versionIndex];


  // Calculate the frame length
  static const int paddingSize[3] = { 4, 1, 1 };

  /* Minimum frame size = 1152 / 8 * 32000 / 48000 = 96
	 * Minimum mpegFrame.main_data size = 96 - 36 - 2 = 58
	 * Maximum mpegFrame.main_data_begin = 2^9 = 512
	 * Therefore remember ceil(512 / 58) = 9 previous frames. */
	for (int i = mpegFrame.num_prev_frames-1; i > 0; --i)
		mpegFrame.prev_frame_size[i] = mpegFrame.prev_frame_size[i-1];
	mpegFrame.prev_frame_size[0] = mpegFrame.frame_size;


  mpegFrame.frame_size = mpegHeader.samplesPerFrame * mpegHeader.bitrate * 125 / mpegHeader.samplingRate;

  if(mpegHeader.isPadded)
    mpegFrame.frame_size += paddingSize[layerIndex];

  // debug(LOG_VERBOSE) << "->version (" << mpegHeader.version << ")" << std::endl;
  // debug(LOG_VERBOSE) << "->layer (" << mpegHeader.layer << ")" << std::endl;
  // debug(LOG_VERBOSE) << "->protectionEnabled (crc) (" << mpegHeader.protectionEnabled << ")" << std::endl;
  // debug(LOG_VERBOSE) << "- Info missing! -" << std::endl;
  // debug(LOG_VERBOSE) << "->emphasis (" << mpegHeader.emphasis << ")" << std::endl;
  // debug(LOG_VERBOSE) << "->samplingRate (" << mpegHeader.samplingRate << ")" << std::endl;
  // debug(LOG_VERBOSE) << "->tables (" << mpegHeader.band_index.short_win << "," << mpegHeader.band_width.short_win <<
  //                                     mpegHeader.band_index.long_win << "," << mpegHeader.band_width.long_win <<
  //                                     " )" << std::endl;
  //
  // debug(LOG_VERBOSE) << "->channel_mode (" << mpegHeader.channel_mode << ")" << std::endl;
  // debug(LOG_VERBOSE) << "->channels (" << mpegHeader.channels << ")" << std::endl;
  // debug(LOG_VERBOSE) << "->modeExtension (" << mpegHeader.modeExtension[0] << "," << mpegHeader.modeExtension[1] << ")" << std::endl;
  // debug(LOG_VERBOSE) << "->isPadded (" << mpegHeader.isPadded << ")" << std::endl;
  // debug(LOG_VERBOSE) << "->bitrate (" << mpegHeader.bitrate << "k)" << std::endl;
  // debug(LOG_VERBOSE) << "->samplesPerFrame (" << mpegHeader.samplesPerFrame << ")" << std::endl;
  // debug(LOG_VERBOSE) << "->mpegFrame.frame_size (" << mpegFrame.frame_size << ")" << std::endl;
  //
  // // Not used by Floris
  // debug(LOG_VERBOSE) << "->isCopyrighted (" << isCopyrighted << ")" << std::endl;
  // debug(LOG_VERBOSE) << "->isOriginal (" << isOriginal << ")" << std::endl;


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
      mpegHeader.CRCpassed = false;
      // mpegerrortype = CRC;
      // debug(LOG_ERROR) << "CRC check failed!" << std::endl;
    } else {
      mpegHeader.CRCpassed = true;
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
    xingHeaderOffset = xingOffsetTable[mpegHeader.channels == 3 ? 1 : 0][mpegHeader.version == 1 ? 0 : 1]; // 3 = Mono

    file->seek(CurrentHeaderOffset + xingHeaderOffset);
    const std::vector<uint8_t> XingIdentification = file->readBlock(4);

    if(XingIdentification == xingPattern1 || XingIdentification == xingPattern2) {

      mpegHeader.xingHeader = true;
      mpegHeader.xingString = XingIdentification[0] == L'I' ? "Info" : "Xing";

      // Read flags
      const std::vector<uint8_t> XingFlags = file->readBlock(4);

      // Wierd haveing 4 bytes when only low nibble of one byte
      // makes up the flags.. or i'm missing something?

      // Check if we have 'number of frames' field
      if((static_cast<unsigned char>(XingFlags[3]) & 0x01) != 0){
        const std::vector<uint8_t> xing_framesdata = file->readBlock(4);
        mpegHeader.xing_frames = ReadBEValue(xing_framesdata);
      }

      // Check if we have 'number of bytes' field
      if((static_cast<unsigned char>(XingFlags[3]) & 0x02) != 0){
        const std::vector<uint8_t> xing_bytesdata = file->readBlock(4);
        mpegHeader.xing_bytes = ReadBEValue(xing_bytesdata);

      }

      // Check if we have 'TOC' field
      if((static_cast<unsigned char>(XingFlags[3]) & 0x04) != 0){
        // TOC = Table of contents, vet inte riktigt vad den är tillför..
        mpegHeader.xing_TOCData = file->readBlock(100);
      }

      // Check if we have 'Quality indicator' field
      if((static_cast<unsigned char>(XingFlags[3]) & 0x08) != 0){
        // from 0 - best quality to 100 - worst
        const std::vector<uint8_t> xing_qualitydata = file->readBlock(4);
        mpegHeader.xing_quality = ReadBEValue(xing_qualitydata);
      }


      // TODO: Decode lame extension
    }

    // Check for VBRI header
    file->seek(CurrentHeaderOffset + 4 + 32);
    const std::vector<uint8_t> VBRIIdentification = file->readBlock(4);

    if(VBRIIdentification == vbriPattern1) {
      mpegHeader.VBRIHeader = true;

      // Read VBRI version
      const std::vector<uint8_t> vbri_versiondata = file->readBlock(2);
      mpegHeader.VBRI_version = ReadBEValue(vbri_versiondata);

      // Read VBRI Delay
      const std::vector<uint8_t> vbri_delaydata = file->readBlock(2);
      mpegHeader.VBRI_delay = ReadBEValue(vbri_delaydata);

      // Read VBRI Quality
      const std::vector<uint8_t> vbri_qualitydata = file->readBlock(2);
      mpegHeader.VBRI_quality = ReadBEValue(vbri_qualitydata);

      // Read VBRI number of bytes
      const std::vector<uint8_t> vbri_bytesdata = file->readBlock(4);
      mpegHeader.VBRI_bytes = ReadBEValue(vbri_bytesdata);

      // Read VBRI number of frames
      const std::vector<uint8_t> vbri_framesdata = file->readBlock(4);
      mpegHeader.VBRI_frames = ReadBEValue(vbri_framesdata);

      // Read VBRI number of TOC entries
      const std::vector<uint8_t> vbri_tocentriesdata = file->readBlock(2);
      mpegHeader.VBRI_TOC_entries = ReadBEValue(vbri_tocentriesdata);

      // Read VBRI
      const std::vector<uint8_t> vbri_scaledata = file->readBlock(2);
      mpegHeader.VBRI_TOC_scale = ReadBEValue(vbri_scaledata);

      // Read VBRI size per table entry
      const std::vector<uint8_t> vbri_sizedata = file->readBlock(2);
      mpegHeader.VBRI_TOC_sizetable = ReadBEValue(vbri_sizedata);

      // Read VBRI frames per table entry
      const std::vector<uint8_t> vbri_frametabledata = file->readBlock(2);
      mpegHeader.VBRI_TOC_framestable = ReadBEValue(vbri_frametabledata);

      // Read VBRI TOC
      mpegHeader.VBRI_TOC_size = mpegHeader.VBRI_TOC_sizetable*mpegHeader.VBRI_TOC_entries;
      // const std::vector<uint8_t> vbri_tocdata = file->readBlock(TOC_size);

    }

  }

  // If the frame is bigger then the auctual file..
  if(CurrentHeaderOffset + mpegFrame.frame_size > m_FileSize) {
    debug(LOG_ERROR) << "Last frame is truncated. Frame (@" << CurrentHeaderOffset << ") size is " << mpegFrame.frame_size <<" bytes but only " << m_FileSize - CurrentHeaderOffset << " bytes is left of file"<< std::endl;
    lastframe = true;
    // mpegerrortype = TRUNCATED;
    return 0;

  } else {

    // Check if there is room for a new header, if there is, check it
    if(CurrentHeaderOffset + mpegFrame.frame_size + 4 < m_FileSize) {
      debug(LOG_VERBOSE) << "Checking next header at: " << CurrentHeaderOffset + mpegFrame.frame_size << std::endl;

      file->seek(CurrentHeaderOffset + mpegFrame.frame_size);
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

              offsetToNextValidFrame = CurrentHeaderOffset + mpegFrame.frame_size + i + 4;

              foundNextFrame = true;

              // We found another valid header wich mean this header was not the last, and there for is broken
              int lengthToNextValidHeader = offsetToNextValidFrame - CurrentHeaderOffset;
              int excpectedHeaderOffset = CurrentHeaderOffset + mpegFrame.frame_size;

              debug(LOG_ERROR) << "Header for frame " << m_FrameIndex+1 << " was excpected @ " << excpectedHeaderOffset << " (0x" << std::hex << excpectedHeaderOffset << std::dec << ") but was found at: " << offsetToNextValidFrame << std::endl;
              debug(LOG_VERBOSE) << "Frame " << m_FrameIndex << " (offset:" << CurrentHeaderOffset << ") excpected length was " << mpegFrame.frame_size << " bytes, but is " << lengthToNextValidHeader << std::endl;
              debug(LOG_VERBOSE) << "Current Header (" << m_FrameIndex << "): \t" << x << " (masked)" << std::endl;
              debug(LOG_VERBOSE) << "Wrong Header (" << m_FrameIndex+1 << "): \t" << y << " (masked)" << std::endl;
              // mpegerrortype = SYNC;

              debug(LOG_VERBOSE) << "Next Valid frame offset: " << offsetToNextValidFrame << " (0x" << std::hex << offsetToNextValidFrame << std::dec << ")" << std::endl;

              m_badframes++;
              m_FrameIndex++;
              break;
            }
          }
        }

        // We did not find another valid frame, maybe end of Audio section or end of file?
        if(!foundNextFrame) {
          offsetToEndOfAudioFrames = CurrentHeaderOffset + mpegFrame.frame_size; // Of valid frames
          debug(LOG_VERBOSE) << "End of audio section: " << offsetToEndOfAudioFrames << std::endl;
          lastframe = true;
        }


        return 0;
      } // if header != nextHeader

    }


  }

  mpegHeader.isHeaderValid = true;
  offsetToNextValidFrame = CurrentHeaderOffset + mpegFrame.frame_size;


  return 1;
}

void MP3::printHeaderInformation(int loglevel) {

  switch (mpegHeader.version) {
    case 1: debug(loglevel) << "Version: MPEG Version 1" << std::endl; break;
    case 2: debug(loglevel) << "Version: MPEG Version 2" << std::endl; break;
    case 25: debug(loglevel) << "Version: MPEG Version 2.5" << std::endl; break;
  }

  switch (mpegHeader.layer) {
    case 1: debug(loglevel) << "Layer: Layer I" << std::endl; break;
    case 2: debug(loglevel) << "Layer: Layer II" << std::endl; break;
    case 3: debug(loglevel) << "Layer: Layer III" << std::endl; break;
  }

  switch (mpegHeader.channel_mode) {
    case 0: {debug(loglevel) << "ChannelMode: Stereo" << std::endl; break;}
    case 1: {
      debug(loglevel) << "ChannelMode: Joint Stereo" << std::endl;

      // Mode extension is only used in Joint stereo
      switch (mpegHeader.layer) {
        case 1: debug(loglevel) << "Mode Extension: bands " << mpegHeader.modeExtension[0] << " to " << mpegHeader.modeExtension[1] << std::endl; break;
        case 2: debug(loglevel) << "Mode Extension: bands " << mpegHeader.modeExtension[0] << " to " << mpegHeader.modeExtension[1] <<  std::endl; break;
        case 3: debug(loglevel) << "Mode Extension: Intensity stereo: " << ((mpegHeader.modeExtension[0]) ? "On" : "Off") << " MS stereo: " << ((mpegHeader.modeExtension[1]) ? "On" : "Off") <<  std::endl; break;
      }
      break;
    }
    case 2: {debug(loglevel) << "ChannelMode: Dual Channel" << std::endl; break;}
    case 3: {debug(loglevel) << "ChannelMode: Single Channel (Mono)" << std::endl; break;}
  }

  debug(loglevel) << "Bitrate: " << mpegHeader.bitrate << " kbps" << std::endl;
  debug(loglevel) << "Sample rate: " << mpegHeader.samplingRate << " Hz" << std::endl;
  // debug(loglevel) << "Frame size: " << mpegFrame.frame_size << " bytes" << std::endl;
  debug(loglevel) << "SamplesPerFrame: " << mpegHeader.samplesPerFrame << std::endl;

  switch (mpegHeader.emphasis) {
    case 0: { debug(loglevel) << "Emphasis: none" << std::endl; break; }
    case 1: { debug(loglevel) << "Emphasis: 50/15 ms" << std::endl; break; }
    case 2: { debug(loglevel) << "Emphasis: reserved" << std::endl; break; }
    case 3: { debug(loglevel) << "Emphasis: CCIT J.17" << std::endl; break; }
  }



  debug(loglevel) << "Protected by CRC: " << ((mpegHeader.protectionEnabled) ? "Yes" : "No") << ((mpegHeader.CRCpassed) ? "(passed)" : "") << std::endl;
  debug(loglevel) << "isPadded: " << ((mpegHeader.isPadded) ? "Yes" : "No") << std::endl;
  debug(loglevel) << "isCopyrighted: " << ((mpegHeader.isCopyrighted) ? "Yes" : "No") << std::endl;
  debug(loglevel) << "isOriginal: " << ((mpegHeader.isOriginal) ? "Yes" : "No") << std::endl;

  if(mpegHeader.xingHeader) {
    debug(loglevel) << "Xing tag: " << mpegHeader.xingString << std::endl;
    if(mpegHeader.xing_frames > 0)
      debug(loglevel) << "Xing Number of Frame: " << mpegHeader.xing_frames << std::endl;

    if(mpegHeader.xing_bytes > 0)
      debug(loglevel) << "Xing Number of Bytes: " << mpegHeader.xing_bytes << " (" << mpegHeader.xing_bytes/1024.0f << " Kb)" << std::endl;

    if(mpegHeader.xing_quality > -1)
      debug(loglevel) << "Xing Quality: " << mpegHeader.xing_quality << std::endl;
  } else {
    debug(loglevel) << "Xing tag: No" << std::endl;
  }

  if(mpegHeader.VBRIHeader) {
    debug(loglevel) << "VBRI Version: " << mpegHeader.VBRI_version << std::endl;
    debug(loglevel) << "VBRI Delay: " << mpegHeader.VBRI_delay << std::endl;
    debug(loglevel) << "VBRI Quality: " << mpegHeader.VBRI_quality << std::endl;
    debug(loglevel) << "VBRI Number of Bytes: " << mpegHeader.VBRI_bytes << std::endl;
    debug(loglevel) << "VBRI Number of Frames: " << mpegHeader.VBRI_frames << std::endl;
    debug(loglevel) << "VBRI TOC Entries: " << mpegHeader.VBRI_TOC_entries << std::endl;
    debug(loglevel) << "VBRI TOC Scale factory of TOC table entries: " << mpegHeader.VBRI_TOC_scale << std::endl;
    debug(loglevel) << "VBRI TOC Size per table in bytes: " << mpegHeader.VBRI_TOC_sizetable << std::endl;
    debug(loglevel) << "VBRI TOC Frames per table entry: " << mpegHeader.VBRI_TOC_framestable << std::endl;
    debug(loglevel) << "VBRI TOC Size: " << mpegHeader.VBRI_TOC_size << std::endl;
  } else {
    debug(loglevel) << "VBRI: No" << std::endl;
  }


}


void MP3::InitializeFrame(unsigned char *buffer) {
  // debug(LOG_VERBOSE) << "->init_frame_params" << std::endl;

  set_side_info(&buffer[mpegHeader.protectionEnabled == 0 ? 6 : 4]);
  set_main_data(buffer);
  for (int gr = 0; gr < 2; gr++) {
    for (int ch = 0; ch < mpegHeader.channels; ch++)
      requantize(gr, ch);


    if (mpegHeader.channel_mode == 1 && mpegHeader.modeExtension[0]) // 1 = JointStereo
      ms_stereo(gr);


    for (int ch = 0; ch < mpegHeader.channels; ch++) {
      if (mpegFrame.block_type[gr][ch] == 2 || mpegFrame.mixed_block_flag[gr][ch])
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
 * The side information contains information on how to decode the mpegFrame.main_data.
 * @param buffer A pointer to the first byte of the side info.
 */
void MP3::set_side_info(unsigned char *buffer)
{
	// debug(LOG_VERBOSE) <<  "->set_side_info" << std::endl;
	int count = 0;

	/* Number of bytes the main data ends before the next frame header. */
	mpegFrame.main_data_begin = (int)get_bits_inc(buffer, &count, 9);

	/* Skip private bits. Not necessary. */
	count += mpegHeader.channel_mode == 3 ? 5 : 3; // 3 = Mono

	for (int ch = 0; ch < mpegHeader.channels; ch++)
		for (int scfsi_band = 0; scfsi_band < 4; scfsi_band++)
			/* - Scale factor selection information.
			 * - If mpegFrame.scfsi[scfsi_band] == 1, then scale factors for the first
			 *   granule are reused in the second granule.
			 * - If mpegFrame.scfsi[scfsi_band] == 0, then each granule has its own scaling factors.
			 * - scfsi_band indicates what group of scaling factors are reused. */
			mpegFrame.scfsi[ch][scfsi_band] = get_bits_inc(buffer, &count, 1) != 0;

	for (int gr = 0; gr < 2; gr++)
		for (int ch = 0; ch < mpegHeader.channels; ch++) {
			/* Length of the scaling factors and main data in bits. */
			mpegFrame.part2_3_length[gr][ch] = (int)get_bits_inc(buffer, &count, 12);
			/* Number of values in each big_region. */
			mpegFrame.big_value[gr][ch] = (int)get_bits_inc(buffer, &count, 9);
			/* Quantizer step size. */
			mpegFrame.global_gain[gr][ch] = (int)get_bits_inc(buffer, &count, 8);
			/* Used to determine the values of mpegFrame.slen1 and mpegFrame.slen2. */
			mpegFrame.scalefac_compress[gr][ch] = (int)get_bits_inc(buffer, &count, 4);
			/* Number of bits given to a range of scale factors.
			 * - Normal blocks: mpegFrame.slen1 0 - 10, mpegFrame.slen2 11 - 20
			 * - Short blocks && mpegFrame.mixed_block_flag == 1: mpegFrame.slen1 0 - 5, mpegFrame.slen2 6-11
			 * - Short blocks && mpegFrame.mixed_block_flag == 0: */
			mpegFrame.slen1[gr][ch] = slen[mpegFrame.scalefac_compress[gr][ch]][0];
			mpegFrame.slen2[gr][ch] = slen[mpegFrame.scalefac_compress[gr][ch]][1];
			/* If set, a not normal window is used. */

			mpegFrame.window_switching[gr][ch] = get_bits_inc(buffer, &count, 1) == 1;

			if (mpegFrame.window_switching[gr][ch]) {
				/* The window type for the granule.
				 * 0: reserved
				 * 1: start block
				 * 2: 3 short windows
				 * 3: end block */
				mpegFrame.block_type[gr][ch] = (int)get_bits_inc(buffer, &count, 2);

				/* Number of scale factor bands before window switching. */
				mpegFrame.mixed_block_flag[gr][ch] = get_bits_inc(buffer, &count, 1) == 1;
				if (mpegFrame.mixed_block_flag[gr][ch]) {
					mpegFrame.switch_point_l[gr][ch] = 8;
					mpegFrame.switch_point_s[gr][ch] = 3;
				} else {
					mpegFrame.switch_point_l[gr][ch] = 0;
					mpegFrame.switch_point_s[gr][ch] = 0;
				}

				/* These are set by default if mpegFrame.window_switching. */
				mpegFrame.region0_count[gr][ch] = mpegFrame.block_type[gr][ch] == 2 ? 8 : 7;
				/* No third region. */
				mpegFrame.region1_count[gr][ch] = 20 - mpegFrame.region0_count[gr][ch];

				for (int region = 0; region < 2; region++)
					/* Huffman table number for a big region. */
					mpegFrame.table_select[gr][ch][region] = (int)get_bits_inc(buffer, &count, 5);
				for (int window = 0; window < 3; window++)
					mpegFrame.subblock_gain[gr][ch][window] = (int)get_bits_inc(buffer, &count, 3);
			} else {
				/* Set by default if !mpegFrame.window_switching. */
				mpegFrame.block_type[gr][ch] = 0;
				mpegFrame.mixed_block_flag[gr][ch] = false;

				for (int region = 0; region < 3; region++)
					mpegFrame.table_select[gr][ch][region] = (int)get_bits_inc(buffer, &count, 5);

				/* Number of scale factor bands in the first big value region. */
				mpegFrame.region0_count[gr][ch] = (int)get_bits_inc(buffer, &count, 4);
				/* Number of scale factor bands in the third big value region. */
				mpegFrame.region1_count[gr][ch] = (int)get_bits_inc(buffer, &count, 3);
				/* # scale factor bands is 12*3 = 36 */
			}

			/* If set, add values from a table to the scaling factors. */
			mpegFrame.preflag[gr][ch] = (int)get_bits_inc(buffer, &count, 1);
			/* Determines the step size. */
			mpegFrame.scalefac_scale[gr][ch] = (int)get_bits_inc(buffer, &count, 1);
			/* Table that determines which count1 table is used. */
			mpegFrame.count1table_select[gr][ch] = (int)get_bits_inc(buffer, &count, 1);
		}
}

/**
 * Due to the Huffman bits' varying length the mpegFrame.main_data isn't aligned with the
 * frames. Unpacks the scaling factors and quantized mpegFrame.samples.
 * @param buffer A buffer that points to the the first byte of the frame header.
 */
void MP3::set_main_data(unsigned char *buffer)
{
	// debug(LOG_VERBOSE) << "->set_main_data" << std::endl;
	/* header + side_information */
	int constant = mpegHeader.channel_mode == 3 ? 21 : 36; // 3 = Mono
	if (mpegHeader.protectionEnabled == 0) // protectionEnabled var crc förut
		constant += 2;

	/* Let's put the main data in a separate buffer so that side info and header
	 * don't interfere. The mpegFrame.main_data_begin may be larger than the previous frame
	 * and doesn't include the size of side info and headers. */

	if (mpegFrame.main_data_begin == 0) {

		// std::cout << "Resezing buffer.." << std::endl;
		mpegFrame.main_data.resize(mpegFrame.frame_size - constant);
		memcpy(&mpegFrame.main_data[0], buffer + constant, mpegFrame.frame_size - constant);

	} else {
		int bound = 0;
		for (int frame = 0; frame < mpegFrame.num_prev_frames; frame++) {

			bound += mpegFrame.prev_frame_size[frame] - constant;

			if (mpegFrame.main_data_begin < bound) {
				int ptr_offset = mpegFrame.main_data_begin + frame * constant;
				int buffer_offset = 0;

				int part[mpegFrame.num_prev_frames];
				part[frame] = mpegFrame.main_data_begin;

				for (int i = 0; i <= frame-1; i++) {
					part[i] = mpegFrame.prev_frame_size[i] - constant;
					part[frame] -= part[i];
				}

				mpegFrame.main_data.resize(mpegFrame.frame_size - constant + mpegFrame.main_data_begin);

				memcpy(mpegFrame.main_data.data(), buffer - ptr_offset, part[frame]);
				ptr_offset -= (part[frame] + constant);
				buffer_offset += part[frame];
				for (int i = frame-1; i >= 0; i--) {
					memcpy(&mpegFrame.main_data[buffer_offset], buffer - ptr_offset, part[i]);
					ptr_offset -= (part[i] + constant);
					buffer_offset += part[i];
				}
				memcpy(&mpegFrame.main_data[mpegFrame.main_data_begin], buffer + constant, mpegFrame.frame_size - constant);
				break;
			}
		}
	}

	int bit = 0;
	for (int gr = 0; gr < 2; gr++)
		for (int ch = 0; ch < mpegHeader.channels; ch++) {
			int max_bit = bit + mpegFrame.part2_3_length[gr][ch];
			unpack_scalefac(mpegFrame.main_data.data(), gr, ch, bit);
			unpack_samples(mpegFrame.main_data.data(), gr, ch, bit, max_bit);
			bit = max_bit;
		}
}

/**
 * This will get the scale factor indices from the main data. mpegFrame.slen1 and mpegFrame.slen2
 * represent the size in bits of each scaling factor. There are a total of 21 scaling
 * factors for long windows and 12 for each short window.
 * @param main_data Buffer solely containing the main_data - excluding the frame header and side info.
 * @param gr
 * @param ch
 */
void MP3::unpack_scalefac(unsigned char *main_data, int gr, int ch, int &bit)
{
	// debug(LOG_VERBOSE) << "->unpack_scalefac" << std::endl;
	int sfb = 0;
	int window = 0;
	int scalefactor_length[2] {
		slen[mpegFrame.scalefac_compress[gr][ch]][0],
		slen[mpegFrame.scalefac_compress[gr][ch]][1]
	};

	/* No scale factor transmission for short blocks. */
	if (mpegFrame.block_type[gr][ch] == 2 && mpegFrame.window_switching[gr][ch]) {
		if (mpegFrame.mixed_block_flag[gr][ch] == 1) { /* Mixed blocks. */
			for (sfb = 0; sfb < 8; sfb++)
				mpegFrame.scalefac_l[gr][ch][sfb] = (int)get_bits_inc(main_data, &bit, scalefactor_length[0]);

			for (sfb = 3; sfb < 6; sfb++)
				for (window = 0; window < 3; window++)
					mpegFrame.scalefac_s[gr][ch][window][sfb] = (int)get_bits_inc(main_data, &bit, scalefactor_length[0]);
		} else /* Short blocks. */
			for (sfb = 0; sfb < 6; sfb++)
				for (window = 0; window < 3; window++)
					mpegFrame.scalefac_s[gr][ch][window][sfb] = (int)get_bits_inc(main_data, &bit, scalefactor_length[0]);

		for (sfb = 6; sfb < 12; sfb++)
			for (window = 0; window < 3; window++)
				mpegFrame.scalefac_s[gr][ch][window][sfb] = (int)get_bits_inc(main_data, &bit, scalefactor_length[1]);

		for (window = 0; window < 3; window++)
			mpegFrame.scalefac_s[gr][ch][window][12] = 0;
	}

	/* Scale factors for long blocks. */
	else {
		if (gr == 0) {
			for (sfb = 0; sfb < 11; sfb++)
				mpegFrame.scalefac_l[gr][ch][sfb] = (int)get_bits_inc(main_data, &bit, scalefactor_length[0]);
			for (; sfb < 21; sfb++)
				mpegFrame.scalefac_l[gr][ch][sfb] = (int)get_bits_inc(main_data, &bit, scalefactor_length[1]);
		} else {
			/* Scale factors might be reused in the second granule. */
			const int sb[5] = {6, 11, 16, 21};
			for (int i = 0; i < 2; i++)
				for (; sfb < sb[i]; sfb++) {
					if (mpegFrame.scfsi[ch][i])
						mpegFrame.scalefac_l[gr][ch][sfb] = mpegFrame.scalefac_l[0][ch][sfb];
					else
						mpegFrame.scalefac_l[gr][ch][sfb] = (int)get_bits_inc(main_data, &bit, scalefactor_length[0]);

				}
			for (int i = 2; i < 4; i++)
				for (; sfb < sb[i]; sfb++) {
					if (mpegFrame.scfsi[ch][i])
						mpegFrame.scalefac_l[gr][ch][sfb] = mpegFrame.scalefac_l[0][ch][sfb];
					else
						mpegFrame.scalefac_l[gr][ch][sfb] = (int)get_bits_inc(main_data, &bit, scalefactor_length[1]);
				}
		}
		mpegFrame.scalefac_l[gr][ch][21] = 0;
	}
}

/**
 * The Huffman bits (part3) will be unpacked. Four bytes are retrieved from the
 * bit stream, and are consecutively evaluated against values of the selected Huffman
 * tables.
 * | mpegFrame.big_value | mpegFrame.big_value | mpegFrame.big_value | quadruple | zero |
 * Each hit gives two mpegFrame.samples.
 * @param main_data Buffer solely containing the main_data excluding the frame header and side info.
 * @param gr
 * @param ch
 */
void MP3::unpack_samples(unsigned char *main_data, int gr, int ch, int bit, int max_bit)
{
	// debug(LOG_VERBOSE) << "->unpack_samples" << std::endl;
	int sample = 0;
	int table_num;
	const unsigned *table;

	for (int i = 0; i < 576; i++)
		mpegFrame.samples[gr][ch][i] = 0;

	/* Get the big value region boundaries. */
	int region0;
	int region1;
	if (mpegFrame.window_switching[gr][ch] && mpegFrame.block_type[gr][ch] == 2) {
		region0 = 36;
		region1 = 576;
	} else {
		region0 = mpegHeader.band_index.long_win[mpegFrame.region0_count[gr][ch] + 1];
		region1 = mpegHeader.band_index.long_win[mpegFrame.region0_count[gr][ch] + 1 + mpegFrame.region1_count[gr][ch] + 1];
	}

	/* Get the mpegFrame.samples in the big value region. Each entry in the Huffman tables
	 * yields two mpegFrame.samples. */
	for (; sample < mpegFrame.big_value[gr][ch] * 2; sample += 2) {
		if (sample < region0) {
			table_num = mpegFrame.table_select[gr][ch][0];
			table = big_value_table[table_num];
		} else if (sample < region1) {
			table_num = mpegFrame.table_select[gr][ch][1];
			table = big_value_table[table_num];
		} else {
			table_num = mpegFrame.table_select[gr][ch][2];
			table = big_value_table[table_num];
		}

		if (table_num == 0) {
			mpegFrame.samples[gr][ch][sample] = 0;
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

						mpegFrame.samples[gr][ch][sample + i] = (float)(sign * (values[i] + linbit));
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
		if (mpegFrame.count1table_select[gr][ch] == 1) {
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
			mpegFrame.samples[gr][ch][sample + i] = values[i];
	}

	/* Fill remaining mpegFrame.samples with zero. */
	for (; sample < 576; sample++)
		mpegFrame.samples[gr][ch][sample] = 0;
}

/**
 * The reduced mpegFrame.samples are rescaled to their original scales and precisions.
 * @param gr
 * @param ch
 */
void MP3::requantize(int gr, int ch)
{
	// debug(LOG_VERBOSE) << "->requantize" << std::endl;
	float exp1, exp2;
	int window = 0;
	int sfb = 0;
	const float scalefac_mult = mpegFrame.scalefac_scale[gr][ch] == 0 ? 0.5 : 1;

	for (int sample = 0, i = 0; sample < 576; sample++, i++) {
		if (mpegFrame.block_type[gr][ch] == 2 || (mpegFrame.mixed_block_flag[gr][ch] && sfb >= 8)) {
			if (i == mpegHeader.band_width.short_win[sfb]) {
				i = 0;
				if (window == 2) {
					window = 0;
					sfb++;
				} else
					window++;
			}

			exp1 = mpegFrame.global_gain[gr][ch] - 210.0 - 8.0 * mpegFrame.subblock_gain[gr][ch][window];
			exp2 = scalefac_mult * mpegFrame.scalefac_s[gr][ch][window][sfb];
		} else {
			if (sample == mpegHeader.band_index.long_win[sfb + 1])
				/* Don't increment sfb at the zeroth sample. */
				sfb++;

			exp1 = mpegFrame.global_gain[gr][ch] - 210.0;
			exp2 = scalefac_mult * (mpegFrame.scalefac_l[gr][ch][sfb] + mpegFrame.preflag[gr][ch] * pretab[sfb]);
		}

		float sign = mpegFrame.samples[gr][ch][sample] < 0 ? -1.0f : 1.0f;
		float a = std::pow(std::abs(mpegFrame.samples[gr][ch][sample]), 4.0 / 3.0);
		float b = std::pow(2.0, exp1 / 4.0);
		float c = std::pow(2.0, -exp2);

		mpegFrame.samples[gr][ch][sample] = sign * a * b * c;
	}
}

/**
 * Reorder short blocks, mapping from scalefactor subbands (for short windows) to 18 sample blocks.
 * @param gr
 * @param ch
 */
void MP3::reorder(int gr, int ch)
{
	// debug(LOG_VERBOSE) << "->reorder" << std::endl;
	int total = 0;
	int start = 0;
	int block = 0;
	float samples[576] = {0};

	for (int sb = 0; sb < 12; sb++) {
		const int sb_width = mpegHeader.band_width.short_win[sb];

		for (int ss = 0; ss < sb_width; ss++) {
			samples[start + block + 0] = this->mpegFrame.samples[gr][ch][total + ss + sb_width * 0];
			samples[start + block + 6] = this->mpegFrame.samples[gr][ch][total + ss + sb_width * 1];
			samples[start + block + 12] = this->mpegFrame.samples[gr][ch][total + ss + sb_width * 2];

			if (block != 0 && block % 5 == 0) { /* 6 * 3 = 18 */
				start += 18;
				block = 0;
			} else
				block++;
		}

		total += sb_width * 3;
	}

	for (int i = 0; i < 576; i++)
		this->mpegFrame.samples[gr][ch][i] = samples[i];
}

/**
 * The left and right channels are added together to form the middle channel. The
 * difference between each channel is stored in the side channel.
 * @param gr
 */
void MP3::ms_stereo(int gr)
{
	// debug(LOG_VERBOSE) << "->ms_stereo (arguments " << gr << ")" << std::endl;

	for (int sample = 0; sample < 576; sample++) {
		float middle = mpegFrame.samples[gr][0][sample];
		float side = mpegFrame.samples[gr][1][sample];
		mpegFrame.samples[gr][0][sample] = (middle + side) / SQRT2;
		mpegFrame.samples[gr][1][sample] = (middle - side) / SQRT2;
	}
}

/**
 * @param gr
 * @param ch
 */
void MP3::alias_reduction(int gr, int ch)
{
	// debug(LOG_VERBOSE) << "->alias_reduction (arguments: " << gr << "," << ch << ")" << std::endl;

	static const float cs[8] {
			.8574929257, .8817419973, .9496286491, .9833145925,
			.9955178161, .9991605582, .9998991952, .9999931551
	};
	static const float ca[8] {
			-.5144957554, -.4717319686, -.3133774542, -.1819131996,
			-.0945741925, -.0409655829, -.0141985686, -.0036999747
	};

	int sb_max = mpegFrame.mixed_block_flag[gr][ch] ? 2 : 32;

	for (int sb = 1; sb < sb_max; sb++)
		for (int sample = 0; sample < 8; sample++) {
			int offset1 = 18 * sb - sample - 1;
			int offset2 = 18 * sb + sample;
			float s1 = mpegFrame.samples[gr][ch][offset1];
			float s2 = mpegFrame.samples[gr][ch][offset2];
			mpegFrame.samples[gr][ch][offset1] = s1 * cs[sample] - s2 * ca[sample];
			mpegFrame.samples[gr][ch][offset2] = s2 * cs[sample] + s1 * ca[sample];
		}
}

/**
 * Inverted modified discrete cosine transformations (IMDCT) are applied to each
 * sample and are afterwards windowed to fit their window shape. As an addition, the
 * mpegFrame.samples are overlapped.
 * @param gr
 * @param ch
 */
void MP3::imdct(int gr, int ch)
{
	// debug(LOG_VERBOSE) << "->imdct (arguments " << gr << "," << ch << ")" << std::endl;

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

	const int n = mpegFrame.block_type[gr][ch] == 2 ? 12 : 36;
	const int half_n = n / 2;
	int sample = 0;

	for (int block = 0; block < 32; block++) {
		for (int win = 0; win < (mpegFrame.block_type[gr][ch] == 2 ? 3 : 1); win++) {
			for (int i = 0; i < n; i++) {
				float xi = 0.0;
				for (int k = 0; k < half_n; k++) {
					float s = mpegFrame.samples[gr][ch][18 * block + half_n * win + k];
					xi += s * std::cos(PI / (2 * n) * (2 * i + 1 + half_n) * (2 * k + 1));
				}

				/* Windowing mpegFrame.samples. */
				sample_block[win * n + i] = xi * sine_block[mpegFrame.block_type[gr][ch]][i];
			}
		}

		if (mpegFrame.block_type[gr][ch] == 2) {
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
			mpegFrame.samples[gr][ch][sample + i] = sample_block[i] + mpegFrame.prev_samples[ch][block][i];
			mpegFrame.prev_samples[ch][block][i] = sample_block[18 + i];
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
	// debug(LOG_VERBOSE) << "->frequency_inversion (arguments " << gr << "," << ch << ")" << std::endl;
	for (int sb = 1; sb < 18; sb += 2)
		for (int i = 1; i < 32; i += 2)
			mpegFrame.samples[gr][ch][i * 18 + sb] *= -1;
}

/**
 * @param gr
 * @param ch
 */
void MP3::synth_filterbank(int gr, int ch)
{
	// debug(LOG_VERBOSE) << "->synth_filterbank (arguments " << gr << "," << ch << ")" << std::endl;
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
			s[i] = mpegFrame.samples[gr][ch][i * 18 + sb];

		for (int i = 1023; i > 63; i--)
			mpegFrame.fifo[ch][i] = mpegFrame.fifo[ch][i - 64];

		for (int i = 0; i < 64; i++) {
			mpegFrame.fifo[ch][i] = 0.0;
			for (int j = 0; j < 32; j++)
				mpegFrame.fifo[ch][i] += s[j] * n[i][j];
		}

		for (int i = 0; i < 8; i++)
			for (int j = 0; j < 32; j++) {
				u[i * 64 + j] = mpegFrame.fifo[ch][i * 128 + j];
				u[i * 64 + j + 32] = mpegFrame.fifo[ch][i * 128 + j + 96];
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

	memcpy(mpegFrame.samples[gr][ch], pcm, 576 * 4);
}

void MP3::interleave()
{
	// debug(LOG_VERBOSE) << "->interleave" << std::endl;
	int i = 0;
	for (int gr = 0; gr < 2; gr++)
		for (int sample = 0; sample < 576; sample++)
			for (int ch = 0; ch < mpegHeader.channels; ch++)
				mpegFrame.pcm[i++] = mpegFrame.samples[gr][ch][sample];

}
