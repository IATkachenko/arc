// apstat.cpp

#include <iostream>
#include <fstream>
#include <string>
#include <stdexcept>
#include "arex_client.h"

//! A prototype client for service status queries.
/*! A prototype command line tool for job status queries to an A-REX
  service. In the name, "ap" means "Arc Prototype".
  
  Usage:
  apsstat

  Configuration:
  Which A-REX service to use is specified in a configuration file. The
  configuration file also specifies how to set up the communication
  chain for the client. The location of the configuration file is
  specified by the environment variable ARC_AREX_CONFIG. If there is
  no such environment variable, the configuration file is assumed to
  be "arex_client.xml" in the current working directory.
*/
int main(int argc, char* argv[]){
  Arc::LogStream logcerr(std::cerr, "AREXClient");
  Arc::Logger::getRootLogger().addDestination(logcerr);
  try{
    if (argc!=1)
      throw std::invalid_argument("Wrong number of arguments!");
    Arc::AREXClient ac;
    std::cout << "Service status: \n" << ac.sstat() << std::endl;
    return EXIT_SUCCESS;
  }
  catch (std::exception& err){
    std::cerr << "ERROR: " << err.what() << std::endl;
    return EXIT_FAILURE;
  }
}
