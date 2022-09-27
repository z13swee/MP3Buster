#ifndef ID3V1_HPP
#define ID3V1_HPP

#include "id3v1Genre.hpp"

class ID3v1
{
public:
  ID3v1(std::vector<uint8_t> &buffer)
  {
    // Stolen from taglib:

    int offset = 3;

    title = std::string(buffer.begin()+offset, buffer.begin()+offset+30).c_str();
    offset += 30;

    artist = std::string(buffer.begin()+offset, buffer.begin()+offset+30).c_str();
    offset += 30;

    album = std::string(buffer.begin()+offset, buffer.begin()+offset+30).c_str();
    offset += 30;

    year = std::string(buffer.begin()+offset, buffer.begin()+offset+30).c_str();
    offset += 4;


    // Check for ID3v1.1 -- Note that ID3v1 *does not* support "track zero" -- this
    // is not a bug in TagLib.  Since a zeroed byte is what we would expect to
    // indicate the end of a C-String, specifically the comment string, a value of
    // zero must be assumed to be just that.

    if(buffer[offset + 28] == 0 && buffer[offset + 29] != 0) {
      // ID3v1.1 detected

      comment = std::string(buffer.begin()+offset, buffer.begin()+offset+28).c_str();
      track = static_cast<unsigned char>(buffer[offset + 29]);
    }
    else
      comment = std::string(buffer.begin()+offset, buffer.begin()+offset+30).c_str();

    offset += 30;

    genre = static_cast<unsigned char>(buffer[offset]);

  }

  ~ID3v1() {};
  // version ??

  std::string title;
  std::string artist;
  std::string album;
  std::string year;
  std::string comment;
  unsigned char track;
  unsigned char genre;

  void Render(int loglevel = LOG_INFO) {

    if(!title.empty())
      debug(loglevel) << "Title: " << title << std::endl;

    if(!artist.empty())
      debug(loglevel) << "Artist: " << artist << std::endl;

    if(!album.empty())
      debug(loglevel) << "Album: " << album << std::endl;

    if(!year.empty())
      debug(loglevel) << "Year: " << year << std::endl;

    if(!comment.empty())
      debug(loglevel) << "Comment: " << comment << std::endl;

    if(track != 0)
      debug(loglevel) << "Track: " << std::hex << int(track) << std::endl;

    if(genre)
      debug(loglevel) << "Genre: " << std::hex << id2v1Genre[genre] << std::endl;
  }


private:
  int Parse();
};

#endif
