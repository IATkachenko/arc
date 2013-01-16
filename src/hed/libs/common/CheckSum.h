// -*- indent-tabs-mode: nil -*-

#ifndef __ARC_CHECKSUM_H__
#define __ARC_CHECKSUM_H__

#include <cstring>
#include <cstdio>
#include <string>

#include <inttypes.h>
#include <sys/types.h>
#include <zlib.h>

namespace Arc {

  /// Interface for checksum manipulations.
  /** This class is an interface and is extended in the specialized classes
   * CRC32Sum, MD5Sum and Adler32Sum. The interface is among others used
   * during data transfers through DataBuffer class. The helper class
   * CheckSumAny can be used as an easier way of handling automatically the
   * different checksum types.
   *
   * @see CheckSumAny
   * @see CRC32Sum
   * @see MD5Sum
   * @see Adler32Sum
   * @headerfile CheckSum.h arc/CheckSum.h
   **/
  class CheckSum {
  public:
    /// Default constructor
    CheckSum(void) {}
    virtual ~CheckSum(void) {}

    /// Initiate the checksum algorithm.
    /**
     * This method must be called before starting a new checksum
     * calculation.
     **/
    virtual void start(void) = 0;

    /// Add data to be checksummed.
    /**
     * This method calculates the checksum of the passed data chunk, taking
     * into account the previous state of this object.
     *
     * @param buf pointer to data chuck to be checksummed.
     * @param len size of the data chuck.
     **/
    virtual void add(void *buf, unsigned long long int len) = 0;

    /// Finalize the checksumming.
    /**
     * This method finalizes the checksum algorithm, that is calculating the
     * final checksum result.
     **/
    virtual void end(void) = 0;

    /// Retrieve result of checksum as binary blob.
    virtual void result(unsigned char*& res, unsigned int& len) const = 0;

    /// Retrieve result of checksum into a string.
    /**
     * The passed string buf is filled with result of checksum algorithm in
     * base 16. At most len characters are filled into buffer buf. The
     * hexadecimal value is prepended with "algorithm:", where algorithm
     * is one of "cksum", "md5" or "adler32" respectively corresponding to
     * the result from the CRC32Sum, MD5Sum and Adler32 classes.
     *
     * @param buf pointer to buffer which should be filled with checksum
     *  result.
     * @param len max number of character filled into buffer.
     * @return 0 on success
     **/
    virtual int print(char *buf, int len) const {
      if (len > 0)
        buf[0] = 0;
      return 0;
    }

    /// Set internal checksum state
    /**
     * This method sets the internal state to that of the passed textual
     * representation. The format passed to this method must be the same as
     * retrieved from the CheckSum::print method.
     *
     * @param buf string containing textual representation of checksum
     * @see CheckSum::print
     **/
    virtual void scan(const char *buf) = 0;

    /// Indicates whether the checksum has been calculated
    virtual operator bool(void) const {
      return false;
    }

    /// Indicates whether the checksum has not been calculated
    virtual bool operator!(void) const {
      return true;
    }
  };

  /// Implementation of CRC32 checksum
  /**
   * This class is a specialized class of the CheckSum class. It provides an
   * implementation for the CRC-32 IEEE 802.3 standard.
   * @headerfile CheckSum.h arc/CheckSum.h
   **/
  class CRC32Sum
    : public CheckSum {
  private:
    uint32_t r;
    unsigned long long count;
    bool computed;
  public:
    CRC32Sum(void);
    virtual ~CRC32Sum(void) {}
    virtual void start(void);
    virtual void add(void *buf, unsigned long long int len);
    virtual void end(void);
    virtual void result(unsigned char*& res, unsigned int& len) const {
      res = (unsigned char*)&r;
      len = 4;
    }
    virtual int print(char *buf, int len) const;
    virtual void scan(const char *buf);
    virtual operator bool(void) const {
      return computed;
    }
    virtual bool operator!(void) const {
      return !computed;
    }
    uint32_t crc(void) const {
      return r;
    }
  };

  /// Implementation of MD5 checksum
  /**
   * This class is a specialized class of the CheckSum class. It provides an
   * implementation of the MD5 message-digest algorithm specified in RFC
   * 1321.
   * @headerfile CheckSum.h arc/CheckSum.h
   **/
  class MD5Sum
    : public CheckSum {
  private:
    bool computed;
    uint32_t A;
    uint32_t B;
    uint32_t C;
    uint32_t D;
    uint64_t count;
    uint32_t X[16];
    unsigned int Xlen;
    // uint32_t T[64];
  public:
    MD5Sum(void);
    virtual void start(void);
    virtual void add(void *buf, unsigned long long int len);
    virtual void end(void);
    virtual void result(unsigned char*& res, unsigned int& len) const {
      res = (unsigned char*)&A;
      len = 16;
    }
    virtual int print(char *buf, int len) const;
    virtual void scan(const char *buf);
    virtual operator bool(void) const {
      return computed;
    }
    virtual bool operator!(void) const {
      return !computed;
    }
  };

  /// Implementation of Adler32 checksum
  /**
   * This class is a specialized class of the CheckSum class. It provides an
   * implementation of the Adler-32 checksum algorithm.
   * @headerfile CheckSum.h arc/CheckSum.h
   **/
  class Adler32Sum
    : public CheckSum {
   private:
    uLong adler;
    bool computed;
   public:
    Adler32Sum(void) : computed(false) {
      start();
    }
    virtual void start(void) {
      adler = adler32(0L, Z_NULL, 0);
    }
    virtual void add(void* buf,unsigned long long int len) {
      adler = adler32(adler, (const Bytef *)buf, len);
    }
    virtual void end(void) {
      computed = true;
    }
    virtual void result(unsigned char*& res,unsigned int& len) const {
      res=(unsigned char*)&adler;
      len=4;
    }
    virtual int print(char* buf,int len) const {
      if(!computed) {
        if(len>0) {
          buf[0]=0;
          return 0;
        }
      }
      return snprintf(buf,len,"adler32:%08lx",adler);
    };
    virtual void scan(const char* /* buf */) { };
    virtual operator bool(void) const {
      return computed;
    }
    virtual bool operator!(void) const {
      return !computed;
    }
  };

  /// Wrapper for CheckSum class
  /**
   * To be used for manipulation of any supported checksum type in a
   * transparent way.
   * @headerfile CheckSum.h arc/CheckSum.h
   **/
  class CheckSumAny
    : public CheckSum {
  public:
    /// Type of checksum
    typedef enum {
      none,      ///< No checksum
      unknown,   ///< Unknown checksum
      undefined, ///< Undefined checksum
      cksum,     ///< CRC32 checksum
      md5,       ///< MD5 checksum
      adler32    ///< ADLER32 checksum
    } type;
  private:
    CheckSum *cs;
    type tp;
  public:
    /// Construct a new CheckSumAny from the given CheckSum.
    CheckSumAny(CheckSum *c = NULL)
      : cs(c),
        tp(none) {}
    /// Construct a new CheckSumAny using the given checksum type.
    CheckSumAny(type type);
    /// Construct a new CheckSumAny using the given checksum type represented as a string.
    CheckSumAny(const char *type);
    virtual ~CheckSumAny(void) {
      if (cs)
        delete cs;
    }
    virtual void start(void) {
      if (cs)
        cs->start();
    }
    virtual void add(void *buf, unsigned long long int len) {
      if (cs)
        cs->add(buf, len);
    }
    virtual void end(void) {
      if (cs)
        cs->end();
    }
    virtual void result(unsigned char*& res, unsigned int& len) const {
      if (cs) {
        cs->result(res, len);
        return;
      }
      len = 0;
    }
    virtual int print(char *buf, int len) const {
      if (cs)
        return cs->print(buf, len);
      if (len > 0)
        buf[0] = 0;
      return 0;
    }
    virtual void scan(const char *buf) {
      if (cs)
        cs->scan(buf);
    }
    virtual operator bool(void) const {
      if (!cs)
        return false;
      return *cs;
    }
    virtual bool operator!(void) const {
      if (!cs)
        return true;
      return !(*cs);
    }
    bool active(void) {
      return (cs != NULL);
    }
    static type Type(const char *crc);
    type Type(void) const {
      return tp;
    }
    void operator=(const char *type);
    bool operator==(const char *s);
    bool operator==(const CheckSumAny& ck);

    /// Get checksum of a file
    /**
     * This method provides an easy way to get the checksum of a file, by only
     * specifying the path to the file. Optionally the checksum type can be
     * specified, if not the MD5 algorithm will be used.
     *
     * @param filepath path to file of which checksum should be calculated
     * @param tp type of checksum algorithm to use, default is md5.
     * @param decimalbase specifies whether output should be in base 10 or
     *  16
     * @return a string containing the calculated checksum is returned.
     **/
    static std::string FileChecksum(const std::string& filepath, type tp = md5, bool decimalbase = false);
  };

} // namespace Arc

#endif // __ARC_CHECKSUM_H__
