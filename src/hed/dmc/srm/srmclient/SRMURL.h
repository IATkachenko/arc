#ifndef __HTTPSD_SRM_URL_H__
#define __HTTPSD_SRM_URL_H__

#include <arc/URL.h>

//namespace Arc {
  
  class SRMURL:public Arc::URL {
   public:
   
    enum SRM_URL_VERSION {
      SRM_URL_VERSION_1,
      SRM_URL_VERSION_2_2,
      SRM_URL_VERSION_UNKNOWN
    };
   
    /**
     * Examples shown for functions below assume the object was initiated with
     * srm://srm.ndgf.org/pnfs/ndgf.org/data/atlas/disk/user/user.mlassnig.dataset.1/dummyfile3
     */
    SRMURL(std::string url);
  
    /**
     * eg /srm/managerv2
     */
    const std::string& Endpoint(void) const { return Path(); };
  
    /**
    * Possible values of version are "1" and "2.2"
    */
    void SetSRMVersion(const std::string& version);
  
    /**
     * eg pnfs/ndgf.org/data/atlas/disk/user/user.mlassnig.dataset.1/dummyfile3
     */
    const std::string& FileName(void) const { if(!valid) return empty; return filename; };
  
    /**
     * eg httpg://srm.ndgf.org:8443/srm/managerv2
     */
    std::string ContactURL(void) const ;
  
    /**
     * eg srm://srm.ndgf.org:8443/srm/managerv2?SFN=
     */
    std::string BaseURL(void) const;
  
    /**
     * eg srm://srm.ndgf.org:8443/pnfs/ndgf.org/data/atlas/disk/user/user.mlassnig.dataset.1/dummyfile3
     */
    std::string ShortURL(void) const;
  
    /**
     * eg srm://srm.ndgf.org:8443/srm/managerv2?SFN=pnfs/ndgf.org/data/atlas/disk/user/user.mlassnig.dataset.1/dummyfile3
     */
    std::string FullURL(void) const;
  
    enum SRM_URL_VERSION SRMVersion() { return srm_version; };
    bool Short(void) const { return isshort; };
    void GSSAPI(bool gssapi);
    bool GSSAPI(void) const;
    void SetPort(int portno) { port = portno; };
    /** Was the port number given in the constructor? */
    bool PortDefined() { return portdefined; };
    operator bool(void) { return valid; };
    bool operator!(void) { return !valid; };

   private:
    static std::string empty;
    std::string filename;
    bool isshort;
    bool valid;
    bool portdefined;
    enum SRM_URL_VERSION srm_version;
    

  };

//} // namespace Arc

#endif // __HTTPSD_SRM_URL_H__
