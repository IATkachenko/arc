#ifndef __ARC_JOBDESCRIPTIONPARSERTESTACC_H__
#define __ARC_JOBDESCRIPTIONPARSERTESTACC_H__

#include <arc/client/JobDescription.h>
#include <arc/client/JobDescriptionParser.h>

#include "TestACCControl.h"

class JobDescriptionParserTestACC
  : public Arc::JobDescriptionParser {
private:
  JobDescriptionParserTestACC()
    : Arc::JobDescriptionParser() {}

public:
  ~JobDescriptionParserTestACC() {}

  static Arc::Plugin* GetInstance(Arc::PluginArgument *arg);

  virtual Arc::JobDescriptionParserResult Parse(const std::string& /*source*/, std::list<Arc::JobDescription>& jobdescs, const std::string& /*language = ""*/, const std::string& /*dialect = ""*/) const { if (JobDescriptionParserTestACCControl::parsedJobDescriptions) jobdescs = *JobDescriptionParserTestACCControl::parsedJobDescriptions; return JobDescriptionParserTestACCControl::parseStatus; }
  virtual Arc::JobDescriptionParserResult UnParse(const Arc::JobDescription& /*job*/, std::string& output, const std::string& /*language*/, const std::string& /*dialect = ""*/) const { if (JobDescriptionParserTestACCControl::unparsedString) output = *JobDescriptionParserTestACCControl::unparsedString; return JobDescriptionParserTestACCControl::unparseStatus; }
};

#endif // __ARC_JOBDESCRIPTIONPARSERTESTACC_H__
