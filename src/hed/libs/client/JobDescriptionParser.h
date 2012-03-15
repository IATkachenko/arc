// -*- indent-tabs-mode: nil -*-

#ifndef __ARC_JOBDESCRIPTIONPARSER_H__
#define __ARC_JOBDESCRIPTIONPARSER_H__

#include <map>
#include <string>

#include <arc/loader/Loader.h>
#include <arc/loader/Plugin.h>


namespace Arc {

  class JobDescription;
  class Logger;

  class JobDescriptionParserResult {
  public:
    typedef enum {
      Success,
      Failure,
      WrongLanguage
    } Result;
    JobDescriptionParserResult(void):v_(Success) { };
    JobDescriptionParserResult(bool v):v_(v?Success:Failure) { };
    JobDescriptionParserResult(Result v):v_(v) { };
    operator bool(void) { return (v_ == Success); };
    bool operator!(void) { return (v_ != Success); };
    bool operator==(bool v) { return ((v_ == Success) == v); };
    bool operator==(Result v) { return (v_ == v); };
  private:
    Result v_;
  };

  /// Abstract class for the different parsers
  /**
   * The JobDescriptionParser class is abstract which provide a interface for job
   * description parsers. A job description parser should inherit this class and
   * overwrite the JobDescriptionParser::Parse and
   * JobDescriptionParser::UnParse methods.
   */
  class JobDescriptionParser
    : public Plugin {
  public:
    virtual ~JobDescriptionParser();

    virtual JobDescriptionParserResult Parse(const std::string& source, std::list<JobDescription>& jobdescs, const std::string& language = "", const std::string& dialect = "") const = 0;
    virtual JobDescriptionParserResult UnParse(const JobDescription& job, std::string& output, const std::string& language, const std::string& dialect = "") const = 0;
    const std::list<std::string>& GetSupportedLanguages() const { return supportedLanguages; }
    bool IsLanguageSupported(const std::string& language) const { return std::find(supportedLanguages.begin(), supportedLanguages.end(), language) != supportedLanguages.end(); }
    const std::string& GetError(void) { return error; };

  protected:
    JobDescriptionParser(PluginArgument* parg);

    std::string& SourceLanguage(JobDescription& j) const;

    std::list<std::string> supportedLanguages;

    std::string error;

    static Logger logger;
  };

  //! Class responsible for loading JobDescriptionParser plugins
  /// The JobDescriptionParser objects returned by a JobDescriptionParserLoader
  /// must not be used after the JobDescriptionParserLoader goes out of scope.
  class JobDescriptionParserLoader
    : public Loader {

  public:
    //! Constructor
    /// Creates a new JobDescriptionParserLoader.
    JobDescriptionParserLoader();

    //! Destructor
    /// Calling the destructor destroys all JobDescriptionParser object loaded
    /// by the JobDescriptionParserLoader instance.
    ~JobDescriptionParserLoader();

    //! Load a new JobDescriptionParser
    /// \param name    The name of the JobDescriptionParser to load.
    /// \return        A pointer to the new JobDescriptionParser (NULL on error).
    JobDescriptionParser* load(const std::string& name);

    //! Retrieve the list of loaded JobDescriptionParser objects.
    /// \return A reference to the list of JobDescriptionParser objects.
    const std::list<JobDescriptionParser*>& GetJobDescriptionParsers() const {
      return jdps;
    }

    class iterator {
    private:
      iterator(JobDescriptionParserLoader& jdpl);
      iterator& operator=(const iterator& it) {}
    public:
      ~iterator() {}
      //iterator& operator=(const iterator& it) { current = it.current; jdpl = it.jdpl; return *this; }
      JobDescriptionParser& operator*() { return **current; }
      const JobDescriptionParser& operator*() const { return **current; }
      JobDescriptionParser* operator->() { return *current; }
      const JobDescriptionParser* operator->() const { return *current; }
      iterator& operator++();
      operator bool() { return !jdpl->jdpDescs.empty() || current != jdpl->jdps.end(); }

      friend class JobDescriptionParserLoader;
    private:
      void LoadNext();

      std::list<JobDescriptionParser*>::iterator current;
      JobDescriptionParserLoader* jdpl;
    };

    iterator GetIterator() { return iterator(*this); }

  private:
    std::list<JobDescriptionParser*> jdps;
    std::list<ModuleDesc> jdpDescs;

    void scan();
    bool scaningDone;
  };
} // namespace Arc

#endif // __ARC_JOBDESCRIPTIONPARSER_H__
