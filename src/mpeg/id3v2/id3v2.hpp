#ifndef ID3V2_HPP
#define ID3V2_HPP

#include <stdint.h>
#include <stdio.h>
#include <vector>
#include <iterator>
#include <algorithm>
#include <map>

#include "logger.hpp"
// #include "id3v2frame.hpp"
// #include "id3v2header.hpp"
// #include "mpegutils.hpp"
#include "Utils.hpp"

// #include "frames/attachedpictureframe.h"


enum TextEncodingType {
  Latin1 = 0,  // IS08859-1, or <i>Latin1</i> encoding.  8 bit characters.
  UTF16 = 1,   // UTF16 with a <i>byte order mark</i>.  16 bit characters.
  UTF16BE = 2, // UTF16 <i>big endian</i>.  16 bit characters.
  UTF8 = 3,    // UTF8 encoding.  Characters are usually 8 bits but can be up to 32.
  UTF16LE = 4  // UTF16 <i>little endian</i>.  16 bit characters.
};


struct ID3v2Header
{
  unsigned int majorVersion = 0;
  unsigned int revisionNumber = 0;

  unsigned int extendedHeaderSize = 0;
  unsigned int extendedHeaderSizeofPadding = 0;
  unsigned int extendedHeaderFlags= 0;

  unsigned int tagSize = 0;
  unsigned int frameHeaderSize = 0;


  bool unsynchronisation = false;
  bool extendedHeader = false;
  bool experimentalIndicator = false;
  bool footerPresent = false;

  bool isValid = false;

};

struct ID3v2FrameHeader
{
  std::vector<uint8_t> m_frameID;
  unsigned int m_frameSize = 0;
  unsigned int m_version = 4;
  unsigned int m_frameHeaderSize = 0;

  bool m_tagAlterPreservation = false;
  bool m_fileAlterPreservation = false;
  bool m_readOnly = false;
  bool m_groupingIdentity = false;
  bool m_compression = false;
  bool m_encryption = false;
  bool m_unsynchronisation = false;
  bool m_dataLengthIndicator = false;

  // Pointer to frame data?

  bool m_isValid = false;
};

// Borrowed from taglib ;)

static std::unordered_map<std::string, std::string> frameTranslation = {
  // Text information frames
  {"TALB", "ALBUM"},
  {"TBPM", "BPM"},
  {"TCOM", "COMPOSER"},
  {"TCON", "GENRE"},
  {"TCOP", "COPYRIGHT"},
  {"TDEN", "ENCODINGTIME"},
  {"TDLY", "PLAYLISTDELAY"},
  {"TDOR", "ORIGINALDATE"},
  {"TDRC", "DATE"},
  // {"TRDA", "DATE"}, // id3 v2.3, replaced by TDRC in v2.4
  // {"TDAT", "DATE"}, // id3 v2.3, replaced by TDRC in v2.4
  // {"TYER", "DATE"}, // id3 v2.3, replaced by TDRC in v2.4
  // {"TIME", "DATE"}, // id3 v2.3, replaced by TDRC in v2.4
  {"TDRL", "RELEASEDATE"},
  {"TDTG", "TAGGINGDATE"},
  {"TENC", "ENCODEDBY"},
  {"TEXT", "LYRICIST"},
  {"TFLT", "FILETYPE"},
  //{"TIPL", "INVOLVEDPEOPLE"}, handled separately
  {"TIT1", "WORK"}, // 'Work' in iTunes
  {"TIT2", "TITLE"},
  {"TIT3", "SUBTITLE"},
  {"TKEY", "INITIALKEY"},
  {"TLAN", "LANGUAGE"},
  {"TLEN", "LENGTH"},
  //{"TMCL", "MUSICIANCREDITS"}, handled separately
  {"TMED", "MEDIA"},
  {"TMOO", "MOOD"},
  {"TOAL", "ORIGINALALBUM"},
  {"TOFN", "ORIGINALFILENAME"},
  {"TOLY", "ORIGINALLYRICIST"},
  {"TOPE", "ORIGINALARTIST"},
  {"TOWN", "OWNER"},
  {"TPE1", "ARTIST"},
  {"TPE2", "ALBUMARTIST"}, // id3's spec says 'PERFORMER', but most programs use 'ALBUMARTIST'
  {"TPE3", "CONDUCTOR"},
  {"TPE4", "REMIXER"}, // could also be ARRANGER
  {"TPOS", "DISCNUMBER"},
  {"TPRO", "PRODUCEDNOTICE"},
  {"TPUB", "LABEL"},
  {"TRCK", "TRACKNUMBER"},
  {"TRSN", "RADIOSTATION"},
  {"TRSO", "RADIOSTATIONOWNER"},
  {"TSOA", "ALBUMSORT"},
  {"TSOC", "COMPOSERSORT"},
  {"TSOP", "ARTISTSORT"},
  {"TSOT", "TITLESORT"},
  {"TSO2", "ALBUMARTISTSORT"}, // non-standard, used by iTunes
  {"TSRC", "ISRC"},
  {"TSSE", "ENCODING"},
  // URL frames
  {"WCOP", "COPYRIGHTURL"},
  {"WOAF", "FILEWEBPAGE"},
  {"WOAR", "ARTISTWEBPAGE"},
  {"WOAS", "AUDIOSOURCEWEBPAGE"},
  {"WORS", "RADIOSTATIONWEBPAGE"},
  {"WPAY", "PAYMENTWEBPAGE"},
  {"WPUB", "PUBLISHERWEBPAGE"},
  //{"WXXX", "URL"}, handled specially
  // Other frames
  {"COMM", "COMMENT"},
  //{"USLT", "LYRICS"}, handled specially
  // Apple iTunes proprietary frames
  {"PCST", "PODCAST"},
  {"TCAT", "PODCASTCATEGORY"},
  {"TDES", "PODCASTDESC"},
  {"TGID", "PODCASTID"},
  {"WFED", "PODCASTURL"},
  {"MVNM", "MOVEMENTNAME"},
  {"MVIN", "MOVEMENTNUMBER"},
  {"GRP1", "GROUPING"},
  {"TCMP", "COMPILATION"}
};



const std::unordered_map<std::string, std::string> txxxFrameTranslation = {
  {"MUSICBRAINZ ALBUM ID",         "MUSICBRAINZ_ALBUMID"},
  {"MUSICBRAINZ ARTIST ID",        "MUSICBRAINZ_ARTISTID"},
  {"MUSICBRAINZ ALBUM ARTIST ID",  "MUSICBRAINZ_ALBUMARTISTID"},
  {"MUSICBRAINZ ALBUM RELEASE COUNTRY", "RELEASECOUNTRY"},
  {"MUSICBRAINZ ALBUM STATUS",     "RELEASESTATUS"},
  {"MUSICBRAINZ ALBUM TYPE",       "RELEASETYPE"},
  {"MUSICBRAINZ RELEASE GROUP ID", "MUSICBRAINZ_RELEASEGROUPID"},
  {"MUSICBRAINZ RELEASE TRACK ID", "MUSICBRAINZ_RELEASETRACKID"},
  {"MUSICBRAINZ WORK ID",          "MUSICBRAINZ_WORKID"},
  {"ACOUSTID ID",                  "ACOUSTID_ID"},
  {"ACOUSTID FINGERPRINT",         "ACOUSTID_FINGERPRINT"},
  {"MUSICIP PUID",                 "MUSICIP_PUID"},
};

// list of deprecated frames and their successors
const std::unordered_map<std::string, std::string> deprecatedFrames = {
  {"TRDA", "TDRC"}, // 2.3 -> 2.4 (http://en.wikipedia.org/wiki/ID3)
  {"TDAT", "TDRC"}, // 2.3 -> 2.4
  {"TYER", "TDRC"}, // 2.3 -> 2.4
  {"TIME", "TDRC"}, // 2.3 -> 2.4
};


// class Frame;
class ID3v2
{
public:

  // ID3v2();
  ID3v2(const std::vector<uint8_t> &data, uint8_t offset = 0);
  // ID3v2(MPEG::File* file);

  ~ID3v2() {
    // for (auto it = m_frames.begin(); it != m_frames.end(); ++it){
    //   delete *it;
    // }
    // m_frames.clear();
  }

  long GetTagSize();

  ID3v2Header* Header;

  ID3v2Header* ParseHeader(const std::vector<uint8_t> &data);
  // std::vector<ID3v2FrameHeader*> ParseFrames(const std::vector<uint8_t> &frameData);
  ID3v2FrameHeader* ParseFrame(std::vector<uint8_t>& data, long offset, unsigned int majorVersion);

  unsigned int GetSynchsafeInteger(unsigned int dwValue);

  bool isValid = false;

  // unsigned int completeTagSize() const;

  /*
  5. The unsynchronisation scheme
  The only purpose of the 'unsynchronisation scheme' is to make the ID3v2 tag as compatible
  as possible with existing software. There is no use in 'unsynchronising' tags if the file
  is only to be processed by new software. Unsynchronisation may only be made with MPEG 2 layer
  I, II and III and MPEG 2.5 files.

  Whenever a false synchronisation is found within the tag, one zeroed byte is inserted after
  the first false synchronisation byte. The format of a correct sync that should be altered by
  ID3 encoders is as follows:

  %11111111 111xxxxx
  And should be replaced with:


  %11111111 00000000 111xxxxx
  This has the side effect that all $FF 00 combinations have to be altered, so they won't be
  affected by the decoding process. Therefore all the $FF 00 combinations have to be replaced
  with the $FF 00 00 combination during the unsynchronisation.

  To indicate usage of the unsynchronisation, the first bit in 'ID3 flags' should be set.
  This bit should only be set if the tag contains a, now corrected, false synchronisation.
  The bit should only be clear if the tag does not contain any false synchronisations.

  Do bear in mind, that if a compression scheme is used by the encoder, the unsynchronisation
  scheme should be applied *afterwards*. When decoding a compressed, 'unsynchronised' file,
  the 'unsynchronisation scheme' should be parsed first, decompression afterwards.

  If the last byte in the tag is $FF, and there is a need to eliminate false synchronisations
  in the tag, at least one byte of padding should be added.
  */
  std::vector<uint8_t> unsynchronisationSchemeDecode(const std::vector<uint8_t> &data);

  // bool isValid() { return m_isValid; }

  void Render(int loglevel);

protected:
  /*!
   * Called by setData() to parse the header data.  It makes this information
   * available through the public API.
   */
  void parse(const std::vector<uint8_t> &data);

private:

  std::vector<ID3v2FrameHeader*> m_frames;

  // std::string frameIDToKey(const std::vector<uint8_t> &id)
  // {
  //   std::vector<uint8_t> id24 = id;
  //   for(size_t i = 0; i < deprecatedFramesSize; ++i) {
  //     if(id24 == deprecatedFrames[i].first) {
  //       id24 = deprecatedFrames[i].second;
  //       break;
  //     }
  //   }
  //   for(size_t i = 0; i < frameTranslationSize; ++i) {
  //     if(id24 == frameTranslation[i].first)
  //       return frameTranslation[i].second;
  //   }
  //   return std::string();
  // }

};

#endif
