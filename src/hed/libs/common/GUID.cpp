// -*- indent-tabs-mode: nil -*-

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WIN32
#define NOGDI
#include <objbase.h>
#include <cstdio>
#else
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <cstdlib>
#include <cstdio>
#endif

#include "GUID.h"

#ifdef WIN32
void Arc::GUID(std::string& guid) {
  ::GUID g;
  HRESULT r;
  r = CoCreateGuid(&g);
  if (r != S_OK) {
    printf("There is an error during GUID generation!\n");
    return;
  }
  char s[32];
  snprintf(s, 32, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x\n", g.Data1, g.Data2, g.Data3, g.Data4[0], g.Data4[1], g.Data4[2], g.Data4[3], g.Data4[4], g.Data4[5], g.Data4[6], g.Data4[7]);
  guid = s;
}

std::string Arc::UUID(void) {
  std::string ret;
  GUID(ret);
  return ret;
}
#else
static bool initialized = false;

static char guid_chars[] = {
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
  'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
  'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
  'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'
};

static uint32_t guid_counter;

static void guid_add_string(std::string& guid, uint32_t n) {
  uint32_t max = 0xFFFFFFFF;
  for (; max;) {
    uint32_t i = n % sizeof(guid_chars);
    guid += guid_chars[i];
    n /= sizeof(guid_chars);
    max /= sizeof(guid_chars);
    n += 0x55555555;
  }
}

void Arc::GUID(std::string& guid) {
  if (!initialized) {
    srandom(time(NULL));
    initialized = true;
  }
  struct timeval tv;
  struct timezone tz;
  gettimeofday(&tv, &tz);
  // Use up to 4 IP addresses
  uint32_t hostid[4] = {
    INADDR_ANY, INADDR_ANY, INADDR_ANY, INADDR_ANY
  };
  hostid[0] = gethostid();
  if (htonl(INADDR_LOOPBACK) == hostid[0])
    hostid[0] = INADDR_ANY;
  char hostname[1024];
  // Local addresses
  if (gethostname(hostname, sizeof(hostname) - 1) == 0) {
    hostname[sizeof(hostname) - 1] = 0;
    struct addrinfo* res = NULL; 
    if(getaddrinfo(hostname,NULL,NULL,&res) == 0) {
      for(struct addrinfo* r=res;r;r=r->ai_next) {
        if(!(r->ai_addr)) continue;
        uint32_t s_address = INADDR_ANY;
        if(r->ai_addr->sa_family == AF_INET) {
          struct sockaddr_in* addr = (struct sockaddr_in*)(r->ai_addr);
          s_address = addr->sin_addr.s_addr;
          if(s_address == htonl(INADDR_LOOPBACK)) continue;
        } else if(r->ai_addr->sa_family == AF_INET6) {
          struct sockaddr_in6* addr = (struct sockaddr_in6*)(r->ai_addr);
          s_address = 0;
          for(int i=0;i<16;++i) {
            s_address ^= addr->sin6_addr.s6_addr[i];
            s_address <<= 2;
          }
        }
        if(s_address == INADDR_ANY) continue;
        int i;
        for (i = 0; i < 3; i++) {
          if (hostid[i] == INADDR_ANY) break;
          if (s_address == hostid[i]) break;
        }
        if (i >= 3) continue;
        if (hostid[i] != INADDR_ANY) continue;
        hostid[i] = s_address;
      }
      freeaddrinfo(res);  
    }
  }
  // External address (TODO)

  // Use collected information
  guid_add_string(guid, tv.tv_usec);
  guid_add_string(guid, tv.tv_sec);
  guid_add_string(guid, hostid[0]);
  guid_add_string(guid, hostid[1]);
  guid_add_string(guid, hostid[2]);
  guid_add_string(guid, hostid[3]);
  guid_add_string(guid, getpid());
  guid_add_string(guid, guid_counter++);
  guid_add_string(guid, random());
}

#if HAVE_UUID_UUID_H
#include <uuid/uuid.h>
std::string Arc::UUID(void) {
  uuid_t uu;
  uuid_generate(uu);
  char uustr[37];
  uuid_unparse(uu, uustr);
  return uustr;
}
#else
std::string Arc::UUID(void) {
  if (!initialized) {
    srandom(time(NULL));
    initialized = true;
  }
  std::string uuid_str("");
  int rnd[10];
  char buffer[20];
  unsigned long uuid_part;
  for (int j = 0; j < 4; j++) {
    for (int i = 0; i < 16; i++)
      rnd[i] = random() % 256;
    rnd[6] = (rnd[6] & 0x4F) | 0x40;
    rnd[8] = (rnd[8] & 0xBF) | 0x80;
    uuid_part = 0L;
    for (int i = 0; i < 16; i++)
      uuid_part = (uuid_part << 8) + rnd[i];
    snprintf(buffer, sizeof(buffer), "%08lx", uuid_part);
    uuid_str.append(buffer, 8);
  }

  uuid_str.insert(8, "-");
  uuid_str.insert(13, "-");
  uuid_str.insert(18, "-");
  uuid_str.insert(23, "-");
  return uuid_str;
}
#endif
#endif  // non WIN32
