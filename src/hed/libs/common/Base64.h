// -*- indent-tabs-mode: nil -*-

// Base64 encoding and decoding

#ifndef ARCLIB_BASE64
#define ARCLIB_BASE64

#include <string>

namespace Arc {

  /// Base64 encoding and decoding.
  /** \ingroup common
   *  \headerfile Base64.h arc/Base64.h */
  class Base64 {
  public:
    /// Constructor is not implemented. Use static methods instead.
    Base64();
    ~Base64();

    /// Encode a string to base 64
    /**
     * \since Added in 3.0.1.
     **/
    static std::string encode(const std::string& bufplain);

    /// Encode a string to base 64
    static std::string encode(const char* bufplain);

    /// Encode a string to base 64
    static std::string encode(const char* bufplain, int size);

    /// Encode a string to base 64 using URL-safe character set
    static std::string encodeURLSafe(const std::string& bufplain);

    /// Encode a string to base 64 using URL-safe character set
    static std::string encodeURLSafe(const char* bufplain);

    /// Encode a string to base 64 using URL-safe character set
    static std::string encodeURLSafe(const char* bufplain, int size);

    /// Decode a string from base 64
    /**
     * \since Added in 3.0.1.
     **/
    static std::string decode(const std::string& bufcoded);

    /// Decode a string from base 64
    static std::string decode(const char* bufcoded);

    /// Decode a string from base 64
    static std::string decode(const char* bufcoded, int size);

    /// Decode a string from base 64 with URL-safe character set
    static std::string decodeURLSafe(const std::string& bufcoded);

    /// Decode a string from base 64
    static std::string decodeURLSafe(const char* bufcoded);

    /// Decode a string from base 64
    static std::string decodeURLSafe(const char* bufcoded, int size);

    // The next 4 methods are legacy API kept for backwards compatibility. They
    // can be removed in the next major version.
    static int encode_len(int len);
    /// Encode a string to base 64
    /** \deprecated Use encode(std::string&) instead */
    static int encode(char *encoded, const char *string, int len);
    static int decode_len(const char *bufcoded);
    /// Decode a string from base 64
    /** \deprecated Use decode(std::string&) instead */
    static int decode(char *bufplain, const char *bufcoded);
  };
} // namespace Arc

#endif // ARCLIB_BASE64
