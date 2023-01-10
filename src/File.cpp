#include <stdio.h>
#include <unistd.h>
#include "File.hpp"

FileWrapper::FileWrapper()
{
}

FileWrapper::FileWrapper(std::filesystem::path path)
{
  m_path = path;

  // m_FileHandleHandle = fopen(path.c_str(), readOnly ? "rb" : "rb+");
  debug(LOG_VERBOSE) << "Opening file " << path << std::endl;
  m_filestream = openFile(path);
}

FileWrapper::~FileWrapper()
{
  if(m_filestream){
    debug(LOG_VERBOSE) << "Closing file " << m_path << std::endl;
    fclose(m_filestream);
  }
}

FILE* FileWrapper::openFile(std::string path)
{
  FILE* fp = fopen(path.c_str(), "rb");

  if (fp == NULL) {
      std::perror("Error");
      return nullptr;
  }

  // Tydligen inte bra att använda fseek för att avgöra filens storlek:
  // https://wiki.sei.cmu.edu/confluence/display/c/FIO19-C.+Do+not+use+fseek%28%29+and+ftell%28%29+to+compute+the+size+of+a+regular+file

  // // fseek(fp, 0L, SEEK_END);
  // if(fseek(fp, 0L, SEEK_END) < 0) {
  //   std::perror("Seek Error");
  //   return nullptr;
  // }

  // // calculating the size of the file
  // m_filesize = ftell(fp);

  // Använder fstat() istället ..
  char *buffer;
  struct stat st;
  int fd = fileno(fp);    // get file descriptor
  fstat(fd, &st);

  if ((fstat(fd, &st) != 0) || (!S_ISREG(st.st_mode))) {
    /* Handle error */
    debug(LOG_ERROR) << "Error calculateing file size" << std::endl;
    return nullptr;
  }

  m_filesize = st.st_size;

  buffer = (char*)malloc(m_filesize);
  if (buffer == NULL) {
    debug(LOG_ERROR) << "Error something" << std::endl;
    return nullptr;
  }


  return fp;
}

size_t FileWrapper::readFile(FILE* file, std::vector<uint8_t> &buffer)
{
  // size_t fread (void *ptr, size_t size, size_t count, FILE * stream );

  // ptr - Pointer to a block of memory with a size of at least (size*count) bytes, converted to a void*.
  // size -Size, in bytes, of each element to be read.
  // count - Number of elements, each one with a size of size bytes.
  // stream - Pointer to a FILE object that specifies an input stream.

  return fread(&buffer[0], sizeof(buffer[0]), buffer.size(), file);
}

size_t FileWrapper::writeFile(FILE* file, const std::vector<uint8_t> &buffer)
{
  return fwrite(&buffer[0], sizeof(buffer[0]), buffer.size(), file);
}

std::vector<uint8_t> FileWrapper::readBlock(unsigned long length)
{
  if(!isOpen()) {
    debug(LOG_ERROR) << "readBlock() -- invalid file." << std::endl;
    return std::vector<uint8_t>();
  }

  if(length == 0) {
    debug(LOG_ERROR) << "readBlock() -- given length is 0" << std::endl;
    return std::vector<uint8_t>();
  }

  if(length > bufferSize() && length > m_filesize)
    length = m_filesize;

  std::vector<uint8_t> buffer(length);

  const size_t count = readFile(m_filestream, buffer);
  return buffer;
}

void FileWrapper::writeBlock(const std::vector<uint8_t> &data)
{
  if(!isOpen()) {
    debug(LOG_ERROR) << "writeBlock() -- invalid file." << std::endl;
    return;
  }

  writeFile(m_filestream, data);
}

void FileWrapper::seek(long offset, Position p)
{
  if(!isOpen()) {
    debug(LOG_ERROR) << "seek() -- stream is not open." << std::endl;
    return;
  }

  int whence;
  switch(p) {
    case Beginning:
      whence = SEEK_SET;
      break;
    case Current:
      whence = SEEK_CUR;
      break;
    case End:
      whence = SEEK_END;
      break;
    default:
      debug(LOG_ERROR) << "seek() -- Invalid Position value." << std::endl;
      return;
  }

  fseek(m_filestream, offset, whence);
}
