// -*- indent-tabs-mode: nil -*-

#ifndef __ARC_FILELOCK_H__
#define __ARC_FILELOCK_H__

#include <string>

#include <arc/Logger.h>

namespace Arc {

  /// A general file locking class.
  /**
   * This class can be used when protected access is required to files
   * which are used by multiple processes or threads. Call acquire() to
   * obtain a lock and release() to release it when finished. check() can
   * be used to verify if a lock is valid for the current process. Locks are
   * independent of FileLock objects - locks are only created and destroyed
   * through acquire() and release(), not on creation or destruction of
   * FileLock objects.
   *
   * Unless use_pid is set false, the process ID and hostname of the calling
   * process are stored in a file filename.lock in the form pid\@hostname.
   * This information is used to determine whether a lock is still valid.
   * It is also possible to specify a timeout on the lock.
   *
   * To ensure an atomic locking operation, acquire() first creates a
   * temporary lock file filename.lock.XXXXXX, then attempts to rename
   * this file to filename.lock. After a successful rename the lock file
   * is checked to make sure the correct process ID and hostname are inside.
   * This eliminates race conditions where multiple processes compete to
   * obtain the lock.
   * @headerfile FileLock.h arc/FileLock.h
   */
  class FileLock {
  public:
    /// Default timeout for a lock
    const static int DEFAULT_LOCK_TIMEOUT;
    /// Suffix added to file name to make lock file
    const static std::string LOCK_SUFFIX;

    /// Create a new FileLock object.
    /**
     * @param filename The name of the file to be locked
     * @param timeout The timeout of the lock
     * @param use_pid If true, use process id in the lock and to
     * determine lock validity
     */
    FileLock(const std::string& filename,
             unsigned int timeout=DEFAULT_LOCK_TIMEOUT,
             bool use_pid=true);

    /// Acquire the lock.
    /**
     * Returns true if the lock was acquired successfully. Locks are acquired
     * if no lock file currently exists, or if the current lock file is
     * invalid. A lock is invalid if the process ID inside the lock no longer
     * exists on the host inside the lock, or the age of the lock file is
     * greater than the lock timeout.
     *
     * @param lock_removed Set to true if an existing lock was removed due
     * to being invalid. In this case the caller may decide to check or
     * delete the file as it is potentially corrupted.
     * @return True if lock is successfully acquired
     */
    bool acquire(bool& lock_removed);

    /// Acquire the lock.
    /**
     * Callers can use this version of acquire() if they do not care whether
     * an invalid lock was removed in the process of obtaining the lock.
     * @return True if lock is successfully acquired
     */
    bool acquire();

    /// Release the lock.
    /**
     * @param force Remove the lock without checking ownership or timeout
     * @return True if lock is successfully released
     */
    bool release(bool force=false);

    /// Check the lock is valid.
    /**
     * @param log_error may be set to false to log error messages at INFO
     * level, in cases where the lock not existing or being owned by another
     * host are not errors.
     * @return 0 if the lock is valid for the current process, the pid inside
     * the lock if the lock is owned by another process on the same host, or
     * -1 if the lock is owned by another host or any other error occurred.
     */
    int check(bool log_error = true);

    /// Get the lock suffix used
    static std::string getLockSuffix();

  private:
    /// File to apply lock to
    std::string filename;
    /// Lock file name: filename + lock suffix
    std::string lock_file;
    /// Lock timeout
    int timeout;
    /// Whether to use process ID in the lock file
    bool use_pid;
    /// Process ID to use in the lock file
    std::string pid;
    /// Hostname to use in the lock file
    std::string hostname;
    /// Logger object
    static Logger logger;

    /// private acquire method.
    bool acquire_(bool& lock_removed);

    /// convenience method for writing pid@hostname to file
    bool write_pid(int h);
  };

} // namespace Arc

#endif
