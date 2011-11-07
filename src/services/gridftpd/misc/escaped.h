#ifndef __ARC_GRIDFTPD_ESCAPED_H__
#define __ARC_GRIDFTPD_ESCAPED_H__

#include <string>

namespace gridftpd {

  /*
    Reads keyword from string at 'buf' separated by 'separator' and
    stores it in 'str'. Each couple of characters starting from \ is
    replaced by second character. \x## is replaced by code corresponding
    to hexadecimal number ##.
    Returns number of first character in 'buf', which is not in read keyword.
  */
  int input_escaped_string(const char* buf,std::string &str,char separator = ' ',char quotes = '"');
  /*
    Unescape content in same buffer. e is treated as end of string.
  */
  char* make_unescaped_string(char* str,char e = 0);
  void make_unescaped_string(std::string &str);

} // namespace gridftpd

#endif // __ARC_GRIDFTPD_ESCAPED_H__
