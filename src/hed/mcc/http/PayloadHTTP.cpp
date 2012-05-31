// The code in this file is quate a mess. It is subject to cleaning 
// and simplification as soon as possible. Functions must be simplified
// and functionality to be split into more functions.
// Some methods of this class must be called in proper order to have
// it function properly. A lot of proptections to be added. 

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

#include "PayloadHTTP.h"
#include <arc/StringConv.h>

namespace ArcMCCHTTP {

using namespace Arc;

Arc::Logger PayloadHTTP::logger(Arc::Logger::getRootLogger(), "MCC.HTTP");

static std::string empty_string("");

static bool ParseHTTPVersion(const std::string& s,int& major,int& minor) {
  major=0; minor=0;
  const char* p = s.c_str();
  if(strncasecmp(p,"HTTP/",5) != 0) return false;
  p+=5;
  char* e;
  major=strtol(p,&e,10);
  if(*e != '.') { major=0; return false; };
  p=e+1;
  minor=strtol(p,&e,10);
  if(*e != 0) { major=0; minor=0; return false; };
  return true;
}

bool PayloadHTTP::readtbuf(void) {
  int l = (sizeof(tbuf_)-1) - tbuflen_;
  if(l > 0) {
    if(stream_->Get(tbuf_+tbuflen_,l)) {
      tbuflen_ += l;
      tbuf_[tbuflen_]=0;
    }
  }
  return (tbuflen_>0);
}

bool PayloadHTTP::readline(std::string& line) {
  line.resize(0);
  for(;line.length()<4096;) { // keeping line size sane
    char* p = (char*)memchr(tbuf_,'\n',tbuflen_);
    if(p) {
      *p=0;
      line.append(tbuf_,p-tbuf_);
      tbuflen_-=(p-tbuf_)+1;
      memmove(tbuf_,p+1,tbuflen_+1); 
      if((!line.empty()) && (line[line.length()-1] == '\r')) line.resize(line.length()-1);
      return true;
    };
    line.append(tbuf_,tbuflen_);
    tbuflen_=0;
    if(!readtbuf()) break;
  }; 
  tbuf_[tbuflen_]=0;
  return false;
}

bool PayloadHTTP::read(char* buf,int64_t& size) {
  if(tbuflen_ >= size) {
    memcpy(buf,tbuf_,size);
    memmove(tbuf_,tbuf_+size,tbuflen_-size+1);
    tbuflen_-=size;
  } else {
    memcpy(buf,tbuf_,tbuflen_);
    buf+=tbuflen_; 
    int64_t l = size-tbuflen_;
    size=tbuflen_; tbuflen_=0; tbuf_[0]=0; 
    for(;l;) {
      int l_ = (l>INT_MAX)?INT_MAX:l;
      if(!stream_->Get(buf,l_)) return (size>0);
      size+=l_; buf+=l_; l-=l_;
    };
  };
  return true;
}

bool PayloadHTTP::readline_chunked(std::string& line) {
  if(!chunked_) return readline(line);
  line.resize(0);
  for(;line.length()<4096;) { // keeping line size sane
    if(tbuflen_ <= 0) {
      if(!readtbuf()) break;
    };
    char c;
    int64_t l = 1;
    if(!read_chunked(&c,l)) break;
    if(c == '\n') {
      if((!line.empty()) && (line[line.length()-1] == '\r')) line.resize(line.length()-1);
      return true;
    };
    line.append(&c,1); // suboptimal
  };
  return false;
}

bool PayloadHTTP::read_chunked(char* buf,int64_t& size) {
  if(!chunked_) return read(buf,size);
  int64_t bufsize = size;
  size = 0;
  if(chunked_ == CHUNKED_ERROR) return false;
  for(;bufsize>0;) {
    if(chunked_ == CHUNKED_EOF) break;
    if(chunked_ == CHUNKED_START) { // reading chunk size
      std::string line;
      chunked_ = CHUNKED_ERROR;
      if(!readline(line)) break;
      char* e;
      chunk_size_ = strtoll(line.c_str(),&e,16);
      if((*e != ';') && (*e != 0)) break;
      if(e == line.c_str()) break;
      if(chunk_size_ == 0) {
        chunked_ = CHUNKED_EOF;
      } else {
        chunked_ = CHUNKED_CHUNK;
      };
    };
    if(chunked_ == CHUNKED_CHUNK) { // reading chunk data
      int64_t l = bufsize;
      if(chunk_size_ < l) l = chunk_size_;
      chunked_ = CHUNKED_ERROR;
      if(!read(buf,l)) break;
      chunk_size_ -= l;
      size += l;
      bufsize -= l;
      buf += l;
      if(chunk_size_ <= 0) {
        chunked_ = CHUNKED_END;
      } else {
        chunked_ = CHUNKED_CHUNK;
      };
    };
    if(chunked_ == CHUNKED_END) { // reading CRLF at end of chunk
      std::string line;
      chunked_ = CHUNKED_ERROR;
      if(!readline(line)) break;
      if(!line.empty()) break;
      chunked_ = CHUNKED_START;
    };
  };
  return (size>0);
}

bool PayloadHTTP::flush_chunked(void) {
  if(!chunked_) return true;
  if(chunked_ == CHUNKED_EOF) return true;
  if(chunked_ == CHUNKED_ERROR) return false;
  const int bufsize = 1024;
  char* buf = new char[bufsize];
  for(;;) {
    if(chunked_ == CHUNKED_EOF) break;
    if(chunked_ == CHUNKED_ERROR) break;
    int64_t l = bufsize;
    if(!read_chunked(buf,l)) break;
  };
  delete[] buf;
  return (chunked_ == CHUNKED_EOF);
}

char* PayloadHTTP::find_multipart(char* buf,int64_t size) {
  char* p = buf;
  for(;;++p) {
    p = (char*)memchr(p,'\r',size-(p-buf));
    if(!p) break; // no tag found
    int64_t l = (multipart_tag_.length()+2) - (size-(p-buf));
    if(l > 0) {
      // filling buffer with necessary amount of information
      if(l > multipart_buf_.length()) {
        int64_t ll = multipart_buf_.length();
        multipart_buf_.resize(l);
        l = l-ll;
        if(!read_chunked((char*)(multipart_buf_.c_str()+ll),l)) {
          p = NULL;
          break;
        };
        multipart_buf_.resize(ll+l);
      }
    }
    int64_t pos = p-buf;
    ++pos;
    char c = '\0';
    if(pos < size) {
      c = buf[pos];
    } else if((pos-size) < multipart_buf_.length()) {
      c = multipart_buf_[pos-size];
    };
    if(c != '\n') continue;
    int tpos = 0;
    for(;tpos<multipart_tag_.length();++tpos) {
      ++pos;
      c = '\0';
      if(pos < size) {
        c = buf[pos];
      } else if((pos-size) < multipart_buf_.length()) {
        c = multipart_buf_[pos-size];
      };
      if(c != multipart_tag_[tpos]) break;
    };
    if(tpos >= multipart_tag_.length()) break; // tag found
  }
  return p;
}

bool PayloadHTTP::read_multipart(char* buf,int64_t& size) {
  if(!multipart_) return read_chunked(buf,size);
  if(multipart_ == MULTIPART_END) return false;
  if(multipart_ == MULTIPART_EOF) return false;
  int64_t bufsize = size;
  size = 0;
  if(!multipart_buf_.empty()) {
    // pick up previously loaded data
    if(bufsize >= multipart_buf_.length()) {
      memcpy(buf,multipart_buf_.c_str(),multipart_buf_.length());
      size = multipart_buf_.length();
      multipart_buf_.resize(0);
    } else {
      memcpy(buf,multipart_buf_.c_str(),bufsize);
      size = bufsize;
      multipart_buf_.erase(0,bufsize);
    };
  }
  // read more data if needed
  if(size < bufsize) {
    int64_t l = bufsize - size;
    if(!read_chunked(buf+size,l)) return false;
    size += l;
  }
  // looking for tag
  const char* p = find_multipart(buf,size);
  if(p) {
    // tag found
    // hope nothing sends GBs in multipart
    multipart_buf_.insert(0,p,size-(p-buf));
    size = (p-buf);
    // TODO: check if it is last tag
    multipart_ = MULTIPART_END;
  };
}

bool PayloadHTTP::flush_multipart(void) {
  if(!multipart_) return true;
  if(multipart_ == MULTIPART_ERROR) return false;
  std::string::size_type pos = 0;
  for(;multipart_ != MULTIPART_EOF;) {
    pos = multipart_buf_.find('\r',pos);
    if(pos == std::string::npos) {
      pos = 0;
      // read just enough
      int64_t l = multipart_tag_.length()+4;
      multipart_buf_.resize(l);
      if(!read_chunked((char*)(multipart_buf_.c_str()),l)) return false;
      multipart_buf_.resize(l);
      continue;
    }
    multipart_buf_.erase(0,pos); // suboptimal
    pos = 0;
    int64_t l = multipart_tag_.length()+4;
    if(l > multipart_buf_.length()) {
      int64_t ll = multipart_buf_.length();
      multipart_buf_.resize(l);
      l = l - ll;
      if(!read_chunked((char*)(multipart_buf_.c_str()+ll),l)) return false;
      ll += l;
      if(ll < multipart_buf_.length()) return false; // can't read enough data
    };
    ++pos;
    if(multipart_buf_[pos] != '\n') continue;
    ++pos;
    if(strncmp(multipart_buf_.c_str()+pos,multipart_tag_.c_str(),multipart_tag_.length())
                 != 0) continue;
    pos+=multipart_tag_.length();
    if(multipart_buf_[pos] != '-') continue;
    ++pos;
    if(multipart_buf_[pos] != '-') continue;
    // end tag found
    multipart_ == MULTIPART_EOF;
  };
  return true;
}

bool PayloadHTTP::read_header(void) {
  std::string line;
  for(;readline_chunked(line) && (!line.empty());) {
    logger.msg(Arc::DEBUG,"< %s",line);
    std::string::size_type pos = line.find(':');
    if(pos == std::string::npos) continue;
    std::string name = line.substr(0,pos);
    for(++pos;pos<line.length();++pos) if(!isspace(line[pos])) break;
    if(pos<line.length()) {
      std::string value = line.substr(pos);
      // for(std::string::size_type p=0;p<value.length();++p) value[p]=tolower(value[p]);
      Attribute(name,value);
    } else {
      Attribute(name,"");
    };
  };
  std::map<std::string,std::string>::iterator it;
  it=attributes_.find("content-length");
  if(it != attributes_.end()) {
    length_=strtoll(it->second.c_str(),NULL,10);
  };
  it=attributes_.find("content-range");
  if(it != attributes_.end()) {
    const char* token = it->second.c_str();
    const char* p = token; for(;*p;p++) if(isspace(*p)) break;
    int64_t range_start,range_end,entity_size;
    if(strncasecmp("bytes",token,p-token) == 0) {
      for(;*p;p++) if(!isspace(*p)) break;
      char *e;
      range_start=strtoull(p,&e,10);
      if((*e) == '-') {
        p=e+1; range_end=strtoull(p,&e,10); p=e;
        if(((*e) == '/') || ((*e) == 0)) {
          if(range_start <= range_end) {
            offset_=range_start;
            end_=range_end+1;
          };
          if((*p) == '/') {
            p++; entity_size=strtoull(p,&e,10);
            if((*e) == 0) {
              size_=entity_size;
            };
          };
        };
      };
    };
  };
  it=attributes_.find("transfer-encoding");
  if(it != attributes_.end()) {
    if(strcasecmp(it->second.c_str(),"chunked") != 0) {
      // Non-implemented encoding
      return false;
    };
    chunked_= CHUNKED_START;
  };
  it=attributes_.find("connection");
  if(it != attributes_.end()) {
    if(strcasecmp(it->second.c_str(),"keep-alive") == 0) {
      keep_alive_=true;
    } else {
      keep_alive_=false;
    };
  };
  it=attributes_.find("content-type");
  if(it != attributes_.end()) {
    if(strncasecmp(it->second.c_str(),"multipart/",10) == 0) {
      // TODO: more bulletproof approach is needed
      std::string lline = lower(it->second);
      const char* boundary = strstr(lline.c_str()+10,"boundary=");
      if(!boundary) return false;
      boundary = it->second.c_str()+(boundary-lline.c_str());
      //const char* boundary = strcasestr(it->second.c_str()+10,"boundary=");
      const char* tag_start = strchr(boundary,'"');
      const char* tag_end = NULL;
      if(!tag_start) {
        tag_start = boundary + 9;
        tag_end = strchr(tag_start,' ');
        if(!tag_end) tag_end = tag_start + strlen(tag_start);
      } else {
        ++tag_start;
        tag_end = strchr(tag_start,'"');
      }
      if(!tag_end) return false;
      multipart_tag_ = std::string(tag_start,tag_end-tag_start);
      if(multipart_tag_.empty()) return false;
      multipart_ = MULTIPART_START;
      multipart_tag_.insert(0,"--");
      multipart_buf_.resize(0);
    }
  }
  return true;
}

bool PayloadHTTP::parse_header(void) {
  method_.resize(0);
  code_=0;
  keep_alive_=false;
  multipart_ = MULTIPART_NONE;
  multipart_tag_ = "";
  chunked_=CHUNKED_NONE;
  // Skip empty lines
  std::string line;
  for(;line.empty();) if(!readline(line)) {
    method_="END";  // Special name to represent closed connection
    length_=0;
    return true;
  };
  logger.msg(Arc::DEBUG,"< %s",line);
  // Parse request/response line
  std::string::size_type pos2 = line.find(' ');
  if(pos2 == std::string::npos) return false;
  std::string word1 = line.substr(0,pos2);
  // Identify request/response
  if(ParseHTTPVersion(line.substr(0,pos2),version_major_,version_minor_)) {
    // Response
    std::string::size_type pos3 = line.find(' ',pos2+1);
    if(pos3 == std::string::npos) return false;
    code_=strtol(line.c_str()+pos2+1,NULL,10);
    reason_=line.substr(pos3+1);
  } else {
    // Request
    std::string::size_type pos3 = line.rfind(' ');
    if((pos3 == pos2) || (pos2 == std::string::npos)) return false;
    if(!ParseHTTPVersion(line.substr(pos3+1),version_major_,version_minor_)) return false;
    method_=line.substr(0,pos2);
    uri_=line.substr(pos2+1,pos3-pos2-1);
  };
  if((version_major_ > 1) || ((version_major_ == 1) && (version_minor_ >= 1))) {
    keep_alive_=true;
  };
  // Parse header lines
  length_=-1; chunked_=CHUNKED_NONE;
  if(!read_header()) return false;
  if(multipart_ == MULTIPART_START) {
    attributes_.erase("content-type");
    // looking for multipart
    std::string line;
    for(;;) {
      if(!readline_chunked(line)) return false;
      if(line.length() == multipart_tag_.length()) {
        if(strncmp(line.c_str(),multipart_tag_.c_str(),multipart_tag_.length()) == 0) {
          multipart_ = MULTIPART_BODY;
          break;
        };
      };
    };
    // reading additional header lines
    chunked_t chunked = chunked_;
    if(!read_header()) return false;
    if(multipart_ !=  MULTIPART_BODY) return false; // nested multipart
    if(chunked_ != chunked) return false; // can't change transfer encoding per part
    // TODO:  check if content-length can be defined here
    // now real body follows
  }
  // In case of keep_alive (HTTP1.1) there must be length specified
  if(keep_alive_ && (!chunked_) && (length_ == -1)) length_=0;
  // If size of object was not reported then try to deduce it.
  if((size_ == 0) && (length_ != -1)) size_=offset_+length_;
  return true;
}

bool PayloadHTTP::get_body(void) {
  if(fetched_) return true; // Already fetched body
  fetched_=true; // Even attempt counts
  valid_=false; // But object is invalid till whole body is available
  // TODO: Check for methods and responses which can't have body
  char* result = NULL;
  int64_t result_size = 0;
  if(length_ == 0) {
    valid_=true;
    return true;
  } else if(length_ > 0) {
    // TODO: combination of chunked and defined length is probably impossible
    result=(char*)malloc(length_+1);
    if(!read_multipart(result,length_)) { free(result); return false; };
    result_size=length_;
  } else { // length undefined
    // Read till connection closed or some logic reports eof
    for(;;) {
      int64_t chunk_size = 4096;
      char* new_result = (char*)realloc(result,result_size+chunk_size+1);
      if(new_result == NULL) { free(result); return false; };
      result=new_result;
      if(!read_multipart(result+result_size,chunk_size)) break;
      // TODO: logical size is not always same as end of body
      result_size+=chunk_size;
    };
  };
  if (result == NULL) {
    return false;
  }
  result[result_size]=0;
  // Attach result to buffer exposed to user
  PayloadRawBuf b;
  b.data=result; b.size=result_size; b.length=result_size; b.allocated=true;
  buf_.push_back(b);
  // If size of object was not reported then try to deduce it.
  if(size_ == 0) size_=offset_+result_size;
  valid_=true;
  // allign to and of message
  flush_multipart();
  flush_chunked();
  return true;
}

const std::string& PayloadHTTP::Attribute(const std::string& name) {
  std::multimap<std::string,std::string>::iterator it = attributes_.find(name);
  if(it == attributes_.end()) return empty_string;
  return it->second;
}

const std::multimap<std::string,std::string>& PayloadHTTP::Attributes(void) {
  return attributes_;
}

void PayloadHTTP::Attribute(const std::string& name,const std::string& value) {
  attributes_.insert(std::pair<std::string,std::string>(lower(name),value));
}

PayloadHTTP::PayloadHTTP(PayloadStreamInterface& stream,bool own):
    valid_(false),fetched_(false),stream_(&stream),stream_own_(own),
    rbody_(NULL),sbody_(NULL),body_own_(false),
    code_(0),length_(0),end_(0),chunked_(CHUNKED_NONE),chunk_size_(0),
    multipart_(MULTIPART_NONE),keep_alive_(true),stream_offset_(0),
    head_response_(false) {
  tbuf_[0]=0; tbuflen_=0;
  if(!parse_header()) {
    error_ = IString("Failed to parse HTTP header").str();
    return;
  }
  // If stream_ is owned then body can be fetched later
  // if(!stream_own_) if(!get_body()) return;
  valid_=true;
}

PayloadHTTP::PayloadHTTP(const std::string& method,const std::string& url,PayloadStreamInterface& stream):
    valid_(true),fetched_(true),stream_(&stream),stream_own_(false),
    rbody_(NULL),sbody_(NULL),body_own_(false),uri_(url),method_(method),
    code_(0),length_(0),end_(0),chunked_(CHUNKED_NONE),chunk_size_(0),
    multipart_(MULTIPART_NONE),keep_alive_(true),stream_offset_(0),
    head_response_(false) {
  tbuf_[0]=0; tbuflen_=0;
  version_major_=1; version_minor_=1;
  // TODO: encode URI properly
}

PayloadHTTP::PayloadHTTP(int code,const std::string& reason,PayloadStreamInterface& stream,bool head_response):
    valid_(true),fetched_(true),stream_(&stream),stream_own_(false),
    rbody_(NULL),sbody_(NULL),body_own_(false),
    code_(code),length_(0),end_(0),reason_(reason),chunked_(CHUNKED_NONE),chunk_size_(0),
    multipart_(MULTIPART_NONE),keep_alive_(true),stream_offset_(0),
    head_response_(head_response) {
  tbuf_[0]=0; tbuflen_=0;
  version_major_=1; version_minor_=1;
  if(reason_.empty()) reason_="OK";
}

PayloadHTTP::PayloadHTTP(const std::string& method,const std::string& url):
    valid_(true),fetched_(true),stream_(NULL),stream_own_(false),
    rbody_(NULL),sbody_(NULL),body_own_(false),uri_(url),method_(method),
    code_(0),length_(0),end_(0),chunked_(CHUNKED_NONE),chunk_size_(0),
    multipart_(MULTIPART_NONE),keep_alive_(true),stream_offset_(0),
    head_response_(false) {
  tbuf_[0]=0; tbuflen_=0;
  version_major_=1; version_minor_=1;
  // TODO: encode URI properly
}

PayloadHTTP::PayloadHTTP(int code,const std::string& reason,bool head_response):
    valid_(true),fetched_(true),stream_(NULL),stream_own_(false),
    rbody_(NULL),sbody_(NULL),body_own_(false),
    code_(code),length_(0),end_(0),reason_(reason),chunked_(CHUNKED_NONE),chunk_size_(0),
    multipart_(MULTIPART_NONE),keep_alive_(true),stream_offset_(0),
    head_response_(head_response) {
  tbuf_[0]=0; tbuflen_=0;
  version_major_=1; version_minor_=1;
  if(reason_.empty()) reason_="OK";
}

PayloadHTTP::~PayloadHTTP(void) {
  // allign to end of message
  flush_multipart();
  flush_chunked();
  if(rbody_ && body_own_) delete rbody_;
  if(sbody_ && body_own_) delete sbody_;
  if(stream_ && stream_own_) delete stream_;
}

bool PayloadHTTP::Flush(void) {
  std::string header;
  bool to_stream = (stream_ != NULL);
  if(method_.empty() && (code_ == 0)) {
    error_ = IString("Invalid HTTP object can't produce result").str();
    return false;
  };
  // Computing length of Body part
  int64_t length = 0;
  std::string range_header;
  if((method_ != "GET") && (method_ != "HEAD")) {
    int64_t start = 0;
    if(head_response_ && (code_==HTTP_OK)) {
      length = Size();
    } else {
      if(sbody_) {
        if(sbody_->Limit() > sbody_->Pos()) {
          length = sbody_->Limit() - sbody_->Pos();
        };
        start=sbody_->Pos();
      } else {
        for(int n=0;;++n) {
          if(Buffer(n) == NULL) break;
          length+=BufferSize(n);
        };
        start=BufferPos(0);
      };
    };
    if(length != Size()) {
      // Add range definition if Body represents part of logical buffer size
      // and adjust HTTP code accordingly
      int64_t end = start+length;
      std::string length_str;
      std::string range_str;
      if(end <= Size()) {
        length_str=tostring(Size());
      } else {
        length_str="*";
      }; 
      if(end > start) {
        range_str=tostring(start)+"-"+tostring(end-1);
        if(code_ == HTTP_OK) {
          code_=HTTP_PARTIAL;
          reason_="Partial content";
        };
      } else {
        range_str="*";
        if(code_ == HTTP_OK) {
          code_=HTTP_RANGE_NOT_SATISFIABLE;
          reason_="Range not satisfiable";
        };
      };
      range_header="Content-Range: bytes "+range_str+"/"+length_str+"\r\n";
    };
    range_header+="Content-Length: "+tostring(length)+"\r\n";
  };
  // Starting header
  if(!method_.empty()) {
    header=method_+" "+uri_+
           " HTTP/"+tostring(version_major_)+"."+tostring(version_minor_)+"\r\n";
  } else if(code_ != 0) {
    char tbuf[256]; tbuf[255]=0;
    snprintf(tbuf,255,"HTTP/%i.%i %i",version_major_,version_minor_,code_);
    header="HTTP/"+tostring(version_major_)+"."+tostring(version_minor_)+" "+
           tostring(code_)+" "+reason_+"\r\n";
  };
  if((version_major_ == 1) && (version_minor_ == 1) && (!method_.empty())) {
    std::map<std::string,std::string>::iterator it = attributes_.find("host");
    if(it == attributes_.end()) {
      std::string host;
      if(!uri_.empty()) {
        std::string::size_type p1 = uri_.find("://");
        if(p1 != std::string::npos) {
          std::string::size_type p2 = uri_.find('/',p1+3);
          if(p2 == std::string::npos) p2 = uri_.length();
          host=uri_.substr(p1+3,p2-p1-3);
        };
      };
      header+="Host: "+host+"\r\n";
    };
  };
  // Adding previously generated range specifier
  header+=range_header;
  bool keep_alive = false;
  if((version_major_ == 1) && (version_minor_ == 1)) keep_alive=keep_alive_;
  if(keep_alive) {
    header+="Connection: keep-alive\r\n";
  } else {
    header+="Connection: close\r\n";
  };
  for(std::map<std::string,std::string>::iterator a = attributes_.begin();a!=attributes_.end();++a) {
    header+=(a->first)+": "+(a->second)+"\r\n";
  };
  header+="\r\n";
  logger.msg(Arc::DEBUG,"> %s",header);
  if(to_stream) {
    if(!stream_->Put(header)) {
      error_ = IString("Failed to write header to output stream").str();
      return false;
    };
    if(length > 0) {
      if(sbody_) {
        // stream to stream transfer
        // TODO: choose optimal buffer size
        // TODO: parallel read and write for better performance
        int tbufsize = (length>1024*1024)?(1024*1024):length;
        char* tbuf = new char[tbufsize];
        if(!tbuf) {
          error_ = IString("Memory allocation error").str();
          return false;
        };
        for(;;) {
          int lbuf = tbufsize;
          if(!sbody_->Get(tbuf,lbuf)) break;
          if(!stream_->Put(tbuf,lbuf)) {
            error_ = IString("Failed to write body to output stream").str();
            delete[] tbuf;
            return false;
          };
        };
        delete[] tbuf;
      } else {
        for(int n=0;;++n) {
          char* tbuf = Buffer(n);
          if(tbuf == NULL) break;
          int64_t lbuf = BufferSize(n);
          if(lbuf > 0) if(!stream_->Put(tbuf,lbuf)) {
            error_ = IString("Failed to write body to output stream").str();
            return false;
          };
        };
      };
    };
    //if(!keep_alive) stream_->Close();
  } else {
    Insert(header.c_str(),0,header.length());
  };
  return true;
}

void PayloadHTTP::Body(PayloadRawInterface& body,bool ownership) {
  if(rbody_ && body_own_) delete rbody_;
  if(sbody_ && body_own_) delete sbody_;
  sbody_ = NULL;
  rbody_=&body; body_own_=ownership;
}

void PayloadHTTP::Body(PayloadStreamInterface& body,bool ownership) {
  if(rbody_ && body_own_) delete rbody_;
  if(sbody_ && body_own_) delete sbody_;
  rbody_ = NULL;
  sbody_=&body; body_own_=ownership;
}

char PayloadHTTP::operator[](PayloadRawInterface::Size_t pos) const {
  if(!((PayloadHTTP*)this)->get_body()) return 0;
  if(pos < PayloadRaw::Size()) {
    return PayloadRaw::operator[](pos);
  };
  if(rbody_) {
    return rbody_->operator[](pos-Size());
  };
  if(sbody_) {
    // Not supporting direct read from stream body
  };
  return 0;
}

char* PayloadHTTP::Content(PayloadRawInterface::Size_t pos) {
  if(!get_body()) return NULL;
  if(pos < PayloadRaw::Size()) {
    return PayloadRaw::Content(pos);
  };
  if(rbody_) {
    return rbody_->Content(pos-Size());
  };
  if(sbody_) {
    // Not supporting content from stream body
  };
  return NULL;
}

PayloadRawInterface::Size_t PayloadHTTP::Size(void) const {
  if(!valid_) return 0;
  PayloadRawInterface::Size_t size = 0;
  if(size_ > 0) {
    size = size_;
  } else if(end_ > 0) {
    size = end_;
  } else if(length_ >= 0) {
    size = offset_ + length_;
  } else {
    // Only do it if no other way of determining size worked.
    if(!((PayloadHTTP*)this)->get_body()) return 0;
    size = PayloadRaw::Size();
  }
  if(rbody_) {
    return size + (rbody_->Size());
  };
  if(sbody_) {
    return size + (sbody_->Size());
  };
  return size;
}

char* PayloadHTTP::Insert(PayloadRawInterface::Size_t pos,PayloadRawInterface::Size_t size) {
  if(!get_body()) return NULL;
  return PayloadRaw::Insert(pos,size);
}

char* PayloadHTTP::Insert(const char* s,PayloadRawInterface::Size_t pos,PayloadRawInterface::Size_t size) {
  if(!get_body()) return NULL;
  return PayloadRaw::Insert(s,pos,size);
}

char* PayloadHTTP::Buffer(unsigned int num) {
  if(!get_body()) return NULL;
  if(num < buf_.size()) {
    return PayloadRaw::Buffer(num);
  };
  if(rbody_) {
    return rbody_->Buffer(num-buf_.size());
  };
  if(sbody_) {
    // Not supporting buffer access to stream body
  };
  return NULL;
}

PayloadRawInterface::Size_t PayloadHTTP::BufferSize(unsigned int num) const {
  if(!((PayloadHTTP*)this)->get_body()) return 0;
  if(num < buf_.size()) {
    return PayloadRaw::BufferSize(num);
  };
  if(rbody_) {
    return rbody_->BufferSize(num-buf_.size());
  };
  if(sbody_) {
    // Not supporting buffer access to stream body
  };
  return 0;
}

PayloadRawInterface::Size_t PayloadHTTP::BufferPos(unsigned int num) const {
  if((num == 0) && (buf_.size() == 0) && (!rbody_) && (!sbody_)) {
    return offset_;
  }
  if(!((PayloadHTTP*)this)->get_body()) return 0;
  if(num < buf_.size()) {
    return PayloadRaw::BufferPos(num);
  };
  if(rbody_) {
    return rbody_->BufferPos(num-buf_.size())+PayloadRaw::BufferPos(num);
  };
  if(sbody_) {
    // Not supporting buffer access to stream body
  };
  return PayloadRaw::BufferPos(num);
}

bool PayloadHTTP::Truncate(PayloadRawInterface::Size_t size) {
  if(!get_body()) return false;
  if(size < PayloadRaw::Size()) {
    if(rbody_ && body_own_) delete rbody_;
    if(sbody_ && body_own_) delete sbody_;
    rbody_=NULL; sbody_=NULL;
    return PayloadRaw::Truncate(size);
  };
  if(rbody_) {
    return rbody_->Truncate(size-Size());
  };
  if(sbody_) {
    // Stream body does not support Truncate yet
  };
  return false;
}

bool PayloadHTTP::Get(char* buf,int& size) {
  if(!valid_) return false;
  if(fetched_) {
    // Read from buffers
    uint64_t bo = 0;
    for(unsigned int num = 0;num<buf_.size();++num) {
      uint64_t bs = PayloadRaw::BufferSize(num);
      if((bo+bs) > stream_offset_) {
        char* p = PayloadRaw::Buffer(num);
        p+=(stream_offset_-bo);
        bs-=(stream_offset_-bo);
        if(bs>size) bs=size;
        memcpy(buf,p,bs);
        size=bs; stream_offset_+=bs;
        return true;
      };
      bo+=bs;
    };
    if(rbody_) {
      for(unsigned int num = 0;;++num) {
        char* p = PayloadRaw::Buffer(num);
        if(!p) break;
        uint64_t bs = PayloadRaw::BufferSize(num);
        if((bo+bs) > stream_offset_) {
          p+=(stream_offset_-bo);
          bs-=(stream_offset_-bo);
          if(bs>size) bs=size;
          memcpy(buf,p,bs);
          size=bs; stream_offset_+=bs;
          return true;
        };
        bo+=bs;
      };
    } else if(sbody_) {
      if(sbody_->Get(buf,size)) {
        stream_offset_+=size;
        return true;
      };
    };
    return false;
  };
  // Read directly from stream
  // TODO: Check for methods and responses which can't have body
  if(length_ == 0) {
    // No body
    size=0;
    return false;
  };
  if(length_ > 0) {
    // Ordinary stream with known length
    int64_t bs = length_-stream_offset_;
    if(bs == 0) { size=0; return false; }; // End of content
    if(bs > size) bs=size;
    if(!read_multipart(buf,bs)) {
      valid_=false; // This is not expected, hence invalidate object
      size=bs; return false;
    };
    size=bs; stream_offset_+=bs;
    return true;
  };
  // Ordinary stream with no length known
  int64_t tsize = size;
  bool r = read_multipart(buf,tsize);
  if(r) stream_offset_+=tsize;
  size=tsize;
  // TODO: adjust logical parameters of buffers
  return r;
}

bool PayloadHTTP::Get(std::string& buf) {
  int l = 1024;
  buf.resize(l);
  bool result = Get((char*)buf.c_str(),l);
  if(!result) l=0;
  buf.resize(l);
  return result;
}

std::string PayloadHTTP::Get(void) {
  std::string s;
  Get(s);
  return s;
}

// Stream interface is meant to be used only
// for reading HTTP body.
bool PayloadHTTP::Put(const char* /* buf */,PayloadStreamInterface::Size_t /* size */) {
  return false;
}

bool PayloadHTTP::Put(const std::string& /* buf */) {
  return false;
}

bool PayloadHTTP::Put(const char* /* buf */) {
  return false;
}

int PayloadHTTP::Timeout(void) const {
  if(!stream_) return 0;
  return stream_->Timeout();
}

void PayloadHTTP::Timeout(int to) {
  if(stream_) stream_->Timeout(to);
}

PayloadStreamInterface::Size_t PayloadHTTP::Pos(void) const {
  if(!stream_) return 0;
  return offset_+stream_offset_;
}

PayloadStreamInterface::Size_t PayloadHTTP::Limit(void) const {
  // TODO: logical size is not always same as end of body
  return Size();
}

} // namespace ArcMCCHTTP

