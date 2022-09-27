#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <iostream>
#include <sstream>

#define LOG_SILENT 0
#define LOG_ERROR 1
#define LOG_WARNING 2
#define LOG_INFO 3
#define LOG_VERBOS_ERROR 4
#define LOG_VERBOSE 5
#define LOG_DEBUG 6

#define LOG_DEFAULT 3

#define debug(level) logger(level, __PRETTY_FUNCTION__, __FILE__, __LINE__)

#define BSLOG_TIME "\033[0;35m-TIME-\033[0;0m"
#define BSLOG_DEBUG "-DEBUG-"
#define BSLOG_VERBOSE_ERROR "\033[0;31m-VERBOSE ERROR-\033[0;0m"
#define BSLOG_ERROR "\033[0;31m-ERROR-\033[0;0m"
#define BSLOG_WARNING "\033[0;33m-WARNING-\033[0;0m"
#define BSLOG_INFO "\033[0;34m-INFO-\033[0;0m"
#define BSLOG_VERBOSE "\033[0;35m-VERBOSE-\033[0;0m"

#define CONSOLE_COLOR_RED "\033[0;31m"
#define CONSOLE_COLOR_GOLD "\033[0;33m"
#define CONSOLE_COLOR_NORMAL "\033[0;0m"

extern unsigned GlobalLogLevel;

class logger {
 public:

  logger() {};
  logger(unsigned loglevel, std::string name, std::string file, int line) :
        m_loglevel(loglevel),
        m_name(name),
        m_file(file),
        m_line(line) {}

  ~logger() {
    if(m_loglevel <= GlobalLogLevel) {
      m_Outstream << m_Stream.rdbuf();
    }
  }

  std::ostream& getOutStream() { return m_Outstream; }

  template <class T>
  std::ostream& operator<<(const T& thing) {
    // TODO: Get only filename and add to error:     <filename>:<line> -

    switch (m_loglevel) {
      case 1: { m_Stream << BSLOG_ERROR << "  " << thing; break; }
      case 2: { m_Stream << BSLOG_WARNING << "  " << thing; break; }
      case 3: { m_Stream << BSLOG_INFO << "  " << thing; break; }
      case 4: { m_Stream << BSLOG_VERBOSE_ERROR << "  " << thing; break; }
      case 5: { m_Stream << BSLOG_VERBOSE << "  " << thing; break; }
      // case 6: { m_Stream << BSLOG_DEBUG << " "<< m_file << ":" << m_line << "  " << thing; break; }
      case 6: { m_Stream << BSLOG_DEBUG << " " << thing; break; }
    }

    return m_Stream;
  }

private:
  unsigned m_loglevel = LOG_DEFAULT;
  std::stringstream m_Stream;
  std::ostream& m_Outstream = std::cout;
  std::string m_name = "";
  std::string m_file = "";
  int m_line = 0;
};
#endif
