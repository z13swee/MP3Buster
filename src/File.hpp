#ifndef FILE_HPP
#define FILE_HPP

#include <iostream>
#include <vector>
#include <stdlib.h>

#include <sys/stat.h>
#include <filesystem> // C++17
#include "logger.hpp"

class FileWrapper
{
public:

  enum Position {
    Beginning,
    Current,
    End
  };

  FileWrapper();
  FileWrapper(std::filesystem::path path);

  ~FileWrapper();


  FILE* openFile(std::string path);

  void seek(long offset, Position p = Beginning); // Move pointer in file

  std::vector<uint8_t> readBlock(unsigned long length);
  size_t readFile(FILE* file, std::vector<uint8_t> &buffer);
  int readByte() { return getc(m_filestream); }

  void writeBlock(const std::vector<uint8_t> &data);
  size_t writeFile(FILE* file, const std::vector<uint8_t> &buffer);

  bool isOpen() const { return (m_filestream != 0); }
  bool isValid() const { return isOpen(); }


  void clear() { clearerr(m_filestream); }          // clears the end-of-file and error indicators for the given stream
  long tell() const { return ftell(m_filestream); } // Get position in current file
  long getSize() { return m_filesize; }             // Return the length/size of the file

  FILE* getStream() { return m_filestream; }

  std::filesystem::path getPath() { return m_path; }

protected:
  void setValid(bool valid) { m_valid = valid; }
  static unsigned int bufferSize() { return 1024; }

private:

  std::filesystem::path m_path;
  FILE* m_filestream = nullptr;
  long m_filesize = 0;
  bool m_valid = false;

};




#endif
