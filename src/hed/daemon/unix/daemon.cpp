#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>
#include <fstream>

#include <cstdio>
#include <cstdlib>
#include <cstring>
// NOTE: On Solaris errno is not working properly if cerrno is included first
#include <cerrno>

#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif /* HAVE_SYS_FILE_H */

#include <arc/Utils.h>

#include "daemon.h"

namespace Arc {

Logger Daemon::logger(Logger::rootLogger, "Daemon");

Daemon::Daemon(const std::string& pid_file_, const std::string& log_file_) : pid_file(pid_file_),log_file(log_file_)
{
    pid_t pid = fork();
    switch(pid) {
        case -1: // parent fork error
            logger.msg(ERROR, "Daemonization fork failed: %s", StrError(errno));
            exit(1);
        case 0: // child
            /* clear inherited umasks */
            ::umask(0);
            /*
             * Become a session leader: setsid must succeed because child is
             * guaranteed not to be a process group leader (it belongs to the
             * process group of the parent.)
             * The goal is to have no controlling terminal.
             * As we now don't have a controlling terminal we will not receive
             * tty-related signals - no need to ignore them.
             */
            setsid();
            /* redirect standard input to /dev/null */
            if (std::freopen("/dev/null", "r", stdin) == NULL) fclose(stdin);
            if(!log_file.empty()) {
                /* forward stdout and stderr to log file */
                if (std::freopen(log_file.c_str(), "a", stdout) == NULL) fclose(stdout);
                if (std::freopen(log_file.c_str(), "a", stderr) == NULL) fclose(stderr);
            } else {
                if (std::freopen("/dev/null", "a", stdout) == NULL) fclose(stdout);
                if (std::freopen("/dev/null", "a", stderr) == NULL) fclose(stderr);
            }
            break;
        default:
            if(!pid_file.empty()) {
                /* write pid to pid file */
                std::fstream pf(pid_file.c_str(), std::fstream::out);
                pf << pid << std::endl;
                pf.close();
            }
            /* succesful exit from parent */
            _exit(0);
    }
}

Daemon::~Daemon() {
    // Remove pid file
    unlink(pid_file.c_str());
    Daemon::logger.msg(INFO, "Shutdown daemon");
}

void Daemon::logreopen(void) {
    if(!log_file.empty()) {
        if (std::freopen(log_file.c_str(), "a", stdout) == NULL) fclose(stdout);
        if (std::freopen(log_file.c_str(), "a", stderr) == NULL) fclose(stderr);
    }
}

} // namespace Arc
