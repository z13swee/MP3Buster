#ifndef MP3_HPP
#define MP3_HPP

#include <iostream>
#include <cmath>
#include <cstring>
#include <vector>
#include <bitset>
#include <iomanip>
#include <filesystem> // C++17
#include <algorithm>
#include <alsa/asoundlib.h> /* dnf install alsa-lib-devel */ /* apt install libasound2-dev */

#include "logger.hpp"

#include "tables.h"
#include "Utils.hpp"
#include "File.hpp"

#include "id3v2/id3v2.hpp"

struct MPEGHeader
{
  bool isHeaderValid = false;
  bool protectionEnabled;
  bool CRCpassed = false;
  bool isPadded;
  bool isCopyrighted;
  bool isOriginal;

  int version = -1;
  int layer = -1;
  int samplesPerFrame = -1;
  int channel_mode = -1;
  int channels = -1;
  int bitrate = -1;
  int samplingRate = -1;
  int modeExtension[2] = {-1,-1};
  int emphasis = -1;

  struct {
    const unsigned *long_win;
    const unsigned *short_win;
  } band_index;
  struct {
    const unsigned *long_win;
    const unsigned *short_win;
  } band_width;

  // Xing/VBRI
  bool xingHeader = false;
  std::string xingString;

  int xing_frames = -1;
  int xing_bytes = -1;
  int xing_quality = -1;
  std::vector<uint8_t> xing_TOCData;


  bool VBRIHeader = false;
  int VBRI_version = -1;
  int VBRI_delay = -1; // Float?
  int VBRI_quality = -1;
  int VBRI_bytes = -1;
  int VBRI_frames = -1;
  int VBRI_TOC_entries = -1;
  int VBRI_TOC_scale = -1;
  int VBRI_TOC_sizetable = -1;
  int VBRI_TOC_framestable= -1;
  int VBRI_TOC_size = -1;

};

struct MPEGFrame
{
  static const int num_prev_frames = 9;
	int prev_frame_size[9];
	int frame_size;

	int main_data_begin;
	bool scfsi[2][4];

	/* Allocate space for two granules and two channels. */
	int part2_3_length[2][2];
	int part2_length[2][2]; // Not used?
	int big_value[2][2];
	int global_gain[2][2];
	int scalefac_compress[2][2];
	int slen1[2][2];
	int slen2[2][2];
	bool window_switching[2][2];
	int block_type[2][2];
	bool mixed_block_flag[2][2];
	int switch_point_l[2][2];
	int switch_point_s[2][2];
	int table_select[2][2][3];
	int subblock_gain[2][2][3];
	int region0_count[2][2];
	int region1_count[2][2];
	int preflag[2][2];
	int scalefac_scale[2][2];
	int count1table_select[2][2];

	int scalefac_l[2][2][22];
	int scalefac_s[2][2][3][13];

	float prev_samples[2][32][18];
	float fifo[2][1024];

	std::vector<unsigned char> main_data;
	float samples[2][2][576];
	float pcm[576 * 4];
};

class ID3v2;
class MP3
{
public:

  // Borde flyttas till respektive class/fil
  const std::vector<uint8_t> ID3v1_Identifier = { 'T', 'A', 'G' };
  const std::vector<uint8_t> ID3v2_Identifier = { 'I', 'D', '3' };
  const std::vector<uint8_t> APE_Identifier = { 'A', 'P', 'E', 'T', 'A', 'G', 'E', 'X' };

  // Xing header ( Unicode )
  const std::vector<uint8_t> xingPattern1 = {L'X',L'i',L'n',L'g'};
  const std::vector<uint8_t> xingPattern2 = {L'I',L'n',L'f',L'o'};
  const std::vector<uint8_t> vbriPattern1 = {L'V',L'B',L'R',L'I'};

  // enum ChannelMode {
	// 	Stereo = 0,
	// 	JointStereo = 1,
	// 	DualChannel = 2,
	// 	Mono = 3
	// };

  // enum Emphasis {
  //   None = 0,
  //   MS5015 = 1,
  //   Reserved = 2,
  //   CCITJ17 = 3
  // };


  MP3(std::filesystem::path path, myConfig& cfg);
  ~MP3();

  void Analyze();
  int InitializeHeader(FileWrapper* file);
  void InitializeFrame(unsigned char *buffer);

  void FindBeginningTags(unsigned amount);  // amount = range of bytes to look in
  void FindEndTags(unsigned offset);        // offset = end of audio section in file

  void printHeaderInformation(int loglevel);
  void BatchmodeOutput(std::filesystem::path path, myConfig& cfg);

  void CalculateDuration();

  inline bool isFrameSync(const std::vector<uint8_t> &bytes, unsigned int offset = 0);

  std::filesystem::path getPath() {
    if(m_File)
     return m_File->getPath();
    else
     return std::filesystem::path();
   }

  myConfig cfg;

  // File information
  // ----------------
  FileWrapper* m_File = nullptr;

  std::vector<uint8_t> m_filebuffer;
  unsigned m_FileSize = 0;
  unsigned m_FrameIndex = 1;
  unsigned m_badframes = 0;
  unsigned m_DurationInSeconds = 0;

  bool errors = false;
  bool lastframe = false;
  bool isVBRI = false;
  int prevBitrate = -1;

  // TAGs
  // ----
  ID3v2* id3v2 = nullptr;

  bool foundID3v2 = false;
  bool foundAPEv2 = false;

  // ID3v1 and APEv1 we check if offset > 0 to se
  // if we found them

  long id3v1_offset = 0;
  long id3v2_offset = 0;
  long apev1_offset = 0;
  long apev2_offset = 0;

  // If tag in the beginning of the file is found, we will add its
  // size here. To start from when looking for MPEG audio frame
  long endOfStartTagsOffset = 0;

  long firstvalidframe = 0;
  long CurrentHeaderOffset;
  long offsetToNextValidFrame = 0;
  long offsetToEndOfAudioFrames = 0;

  int xingHeaderOffset = -1;

  MPEGHeader mpegHeader;
  MPEGFrame mpegFrame;

  unsigned long madTimeTest = 0;

  // MPEG Header
  // -----------
  // bool isHeaderValid = false;
  // bool protectionEnabled;
  // bool CRCpassed = false;
  // bool isPadded;
  // bool isCopyrighted;
  // bool isOriginal;
  //
  // int version;
  //
  // int layer;
  // // int frameLength = 0;
  // int samplesPerFrame;
  // int channels = 0;
  //
  // unsigned bitrate;
  // unsigned samplingRate;
  // unsigned modeExtension[2];
  //
  // long firstvalidframe = 0;
  // long CurrentHeaderOffset;
  // long offsetToNextValidFrame = 0;
  // long offsetToEndOfAudioFrames = 0;
  //
  // Emphasis emphasis;
  // ChannelMode channel_mode;
  //
  // struct {
  //   const unsigned *long_win;
  //   const unsigned *short_win;
  // } band_index;
  // struct {
  //   const unsigned *long_win;
  //   const unsigned *short_win;
  // } band_width;
  //
  // // Xing/VBRI
  // bool xingHeader = false;
  // std::string xingString;
  // int xingHeaderOffset = -1;
  // int xing_frames = -1;
  // int xing_bytes = -1;
  // int xing_quality = -1;
  // std::vector<uint8_t> xing_TOCData;
  //
  //
  // bool VBRIHeader = false;
  // int VBRI_version = -1;
  // int VBRI_delay = -1; // Float?
  // int VBRI_quality = -1;
  // int VBRI_bytes = -1;
  // int VBRI_frames = -1;
  // int VBRI_TOC_entries = -1;
  // int VBRI_TOC_scale = -1;
  // int VBRI_TOC_sizetable = -1;
  // int VBRI_TOC_framestable= -1;
  // int VBRI_TOC_size = -1;

  // MPEG Frame
  // ----------
  // static const int num_prev_frames = 9;
	// int prev_frame_size[9];
	// int frame_size;
  //
	// int main_data_begin;
	// bool scfsi[2][4];
  //
	// /* Allocate space for two granules and two channels. */
	// int part2_3_length[2][2];
	// int part2_length[2][2];
	// int big_value[2][2];
	// int global_gain[2][2];
	// int scalefac_compress[2][2];
	// int slen1[2][2];
	// int slen2[2][2];
	// bool window_switching[2][2];
	// int block_type[2][2];
	// bool mixed_block_flag[2][2];
	// int switch_point_l[2][2];
	// int switch_point_s[2][2];
	// int table_select[2][2][3];
	// int subblock_gain[2][2][3];
	// int region0_count[2][2];
	// int region1_count[2][2];
	// int preflag[2][2];
	// int scalefac_scale[2][2];
	// int count1table_select[2][2];
  //
	// int scalefac_l[2][2][22];
	// int scalefac_s[2][2][3][13];
  //
	// float prev_samples[2][32][18];
	// float fifo[2][1024];
  //
	// std::vector<unsigned char> main_data;
	// float samples[2][2][576];
	// float pcm[576 * 4];

	void set_frame_size();
	void set_side_info(unsigned char *buffer);
	void set_main_data(unsigned char *buffer);
	void unpack_scalefac(unsigned char *bit_stream, int gr, int ch, int &bit);
	void unpack_samples(unsigned char *bit_stream, int gr, int ch, int bit, int max_bit);
	void requantize(int gr, int ch);
	void ms_stereo(int gr);
	void reorder(int gr, int ch);
	void alias_reduction(int gr, int ch);
	void imdct(int gr, int ch);
	void frequency_inversion(int gr, int ch);
	void synth_filterbank(int gr, int ch);
	void interleave();
};

#endif
