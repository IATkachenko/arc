// -*- indent-tabs-mode: nil -*-

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>
#include <list>
#include <string>

#include <arc/ArcLocation.h>
#include <arc/IString.h>
#include <arc/Logger.h>
#include <arc/OptionParser.h>
#include <arc/StringConv.h>
#include <arc/UserConfig.h>
#include <arc/communication/ClientInterface.h>
#include <arc/message/MCC.h>

int main(int argc, char **argv) {

  setlocale(LC_ALL, "");

  Arc::Logger logger(Arc::Logger::getRootLogger(), "arcecho");
  Arc::LogStream logcerr(std::cerr);
  logcerr.setFormat(Arc::ShortFormat);
  Arc::Logger::getRootLogger().addDestination(logcerr);
  Arc::Logger::rootLogger.setThreshold(Arc::WARNING);

  Arc::ArcLocation::Init(argv[0]);

  Arc::OptionParser options(istring("service message"),
                            istring("The arcecho command is a client for "
                                    "the ARC echo service."),
                            istring("The service argument is a URL to an ARC "
                                    "echo service.\n"
                                    "The message argument is the message the "
                                    "service should return."));

  int timeout = -1;
  options.AddOption('t', "timeout", istring("timeout in seconds (default 20)"),
                    istring("seconds"), timeout);

  std::string conffile;
  options.AddOption('z', "conffile",
                    istring("configuration file (default ~/.arc/client.conf)"),
                    istring("filename"), conffile);

  std::string debug;
  options.AddOption('d', "debug",
                    istring("FATAL, ERROR, WARNING, INFO, VERBOSE or DEBUG"),
                    istring("debuglevel"), debug);

  bool version = false;
  options.AddOption('v', "version", istring("print version information"),
                    version);

  std::list<std::string> args = options.Parse(argc, argv);

  if (version) {
    std::cout << Arc::IString("%s version %s", "arcecho", VERSION)
              << std::endl;
    return 0;
  }

  // If debug is specified as argument, it should be set before loading the configuration.
  if (!debug.empty())
    Arc::Logger::getRootLogger().setThreshold(Arc::string_to_level(debug));

  Arc::UserConfig usercfg(conffile);
  if (!usercfg) {
    logger.msg(Arc::ERROR, "Failed configuration initialization");
    return 1;
  }

  if (debug.empty() && !usercfg.Verbosity().empty())
    Arc::Logger::getRootLogger().setThreshold(Arc::string_to_level(usercfg.Verbosity()));

  if (timeout > 0) {
    usercfg.Timeout(timeout);
  }

  if (args.size() != 2) {
    logger.msg(Arc::ERROR, "Wrong number of arguments!");
    return 1;
  }

  std::list<std::string>::iterator it = args.begin();

  Arc::URL service = *it++;
  std::string message = *it++;

  Arc::MCCConfig cfg;
  usercfg.ApplyToConfig(cfg);

  Arc::ClientSOAP client(cfg, service, usercfg.Timeout());

  std::string xml;

  Arc::NS ns("echo", "http://www.nordugrid.org/schemas/echo");
  Arc::PayloadSOAP request(ns);
  request.NewChild("echo:echo").NewChild("echo:say") = message;

  request.GetXML(xml, true);
  logger.msg(Arc::INFO, "Request:\n%s", xml);

  Arc::PayloadSOAP *response = NULL;

  Arc::MCC_Status status = client.process(&request, &response);

  if (!status) {
    logger.msg(Arc::ERROR, (std::string)status);
    if (response)
      delete response;
    return 1;
  }

  if (!response) {
    logger.msg(Arc::ERROR, "No SOAP response");
    return 1;
  }

  response->GetXML(xml, true);
  logger.msg(Arc::INFO, "Response:\n%s", xml);

  std::string answer = (std::string)((*response)["echoResponse"]["hear"]);

  delete response;

  std::cout << answer << std::endl;

  return 0;
}
