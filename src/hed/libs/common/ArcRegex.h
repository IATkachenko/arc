// -*- indent-tabs-mode: nil -*-

#ifndef __ARC_REGEX_H__
#define __ARC_REGEX_H__

#include <list>
#include <string>
#include <regex.h>

namespace Arc {

  /// A regular expression class.
  /** This class is a wrapper around the functions provided in regex.h.
      \ingroup common
      \headerfile ArcRegex.h arc/ArcRegex.h */
  class RegularExpression {
  public:

    /// Default constructor
    RegularExpression();

    /// Creates a regex from a pattern string.
    RegularExpression(std::string pattern);

    /// Copy constructor.
    RegularExpression(const RegularExpression& regex);

    /// Destructor
    ~RegularExpression();

    /// Assignment operator.
    RegularExpression& operator=(const RegularExpression& regex);

    /// Returns true if the pattern of this regex is ok.
    bool isOk();

    /// Returns true if this regex has the pattern provided.
    bool hasPattern(std::string str);

    /// Returns true if this regex matches whole string provided.
    bool match(const std::string& str) const;

    /// Returns true if this regex matches the string provided.
    /** Unmatched parts of the string are stored in 'unmatched'.
     *  Matched parts of the string are stored in 'matched'. The
     *  first entry in matched is the string that matched the regex,
     *  and the following entries are parenthesised elements
     *  of the regex.
     */
    bool match(const std::string& str, std::list<std::string>& unmatched, std::list<std::string>& matched) const;

    /// Returns pattern
    std::string getPattern() const;

  private:
    std::string pattern;
    regex_t preg;
    int status;
  };
}

#endif
