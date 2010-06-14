// -*- indent-tabs-mode: nil -*-

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <cstdlib>
// NOTE: On Solaris errno is not working properly if cerrno is included first
#include <cerrno>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <glibmm.h>

#include "FileLock.h"

namespace Arc {

  FileLock::FileLock(const std::string& filename)
    : lockfile(filename + ".lock") {

    fd = ::open(lockfile.c_str(), O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    while ((fd == -1) && (errno == EEXIST)) {
      if (fd != -1)
        close(fd);
      Glib::usleep(10000);
      fd = ::open(lockfile.c_str(), O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    }
    if (fd != -1)
      close(fd);
  }

  FileLock::~FileLock() {
    unlink(lockfile.c_str());
  }

  FileLock::operator bool() {
    return (fd != -1);
  }

  bool FileLock::operator!() {
    return (fd == -1);
  }

} // namespace Arc
