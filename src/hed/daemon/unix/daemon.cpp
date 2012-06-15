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
#include <sys/wait.h>

#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif /* HAVE_SYS_FILE_H */

#include <arc/Utils.h>
#include <arc/Watchdog.h>

#include "daemon.h"

namespace Arc {

Logger Daemon::logger(Logger::rootLogger, "Daemon");


static void init_child(const std::string& log_file) {
    /* clear inherited umasks */
    ::umask(0022);
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
}

static void init_parent(pid_t pid,const std::string& pid_file) {
            if(!pid_file.empty()) {
                /* write pid to pid file */
                std::fstream pf(pid_file.c_str(), std::fstream::out);
                pf << pid << std::endl;
                pf.close();
            }
}

Daemon::Daemon(const std::string& pid_file_, const std::string& log_file_, bool watchdog) : pid_file(pid_file_),log_file(log_file_),watchdog_pid(0)
{
    pid_t pid = ::fork();
    switch(pid) {
        case -1: // parent fork error
            logger.msg(ERROR, "Daemonization fork failed: %s", StrError(errno));
            exit(1);
        case 0: { // child
            /* And another process is left for watchdoging */
            /* Watchdog need to be initialized before fork to make sure it is shared */
            WatchdogListener wdl;
            while(true) { // stay in loop waiting for watchdog alarm
                if(watchdog) pid = ::fork();
                switch(pid) {
                    case -1: // parent fork error
                        logger.msg(ERROR, "Watchdog fork failed: %s", StrError(errno));
                        exit(1);
                    case 0: // real child
                        if(watchdog) watchdog_pid = ::getppid();
                        init_child(log_file);
                        break;
                    default: // watchdog
                        init_parent(pid,pid_file);
                        bool error;
                        int status = 0;
                        pid_t rpid = 0;
                        for(;;) {
                            if(!wdl.Listen(30,error)) {
                                if(error) _exit(1); // watchdog gone
                                // check if child is still there
                                rpid = ::waitpid(pid,&status,WNOHANG);
                                if(rpid != 0) break;
                            } else {
                                // watchdog timeout - but check child too
                                rpid = ::waitpid(pid,&status,WNOHANG);
                                break;
                            }
                        }
                        /* check if child already exited */
                        if(rpid == pid) {
                            /* child exited */
                            if(WIFSIGNALED(status) && 
                               ((WTERMSIG(status) == SIGSEGV) ||
                                (WTERMSIG(status) == SIGFPE) ||
                                (WTERMSIG(status) == SIGABRT) ||
                                (WTERMSIG(status) == SIGILL))) {
                            } else {
                                /* child either exited itself or was asked to */
                                _exit(1);
                            }
                        } else {
                            /* watchdog timeouted - kill process */
                            // TODO: more sophisticated killing
                            //sighandler_t old_sigterm = ::signal(SIGTERM,SIG_IGN);
                            sig_t old_sigterm = ::signal(SIGTERM,SIG_IGN);
                            int patience = 600; // how long can we wait? Maybe configure it.
                            ::kill(pid,SIGTERM);
                            while(waitpid(pid,&status,WNOHANG) == 0) {
                              if(patience-- < 0) break;
                              sleep(1);
                            }
                            ::kill(pid,SIGKILL);
                            sleep(1);
                            ::waitpid(pid,&status,0);
                            ::signal(SIGTERM,old_sigterm);
                        }
                        break; // go in loop and do another fork
                }
                if(pid == 0) break; // leave watchdog loop because it is child now
            }
            }; break;
        default: // original parent
            if(!watchdog) init_parent(pid,pid_file);
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

void Daemon::shutdown(void) {
    if(watchdog_pid) kill(watchdog_pid,SIGTERM);
}

} // namespace Arc
