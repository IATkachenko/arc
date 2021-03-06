"""
resource to fetch a bloom filter from
"""

import socket

from twisted.python import log
from twisted.web import resource



class CacheResource(resource.Resource):

    isLeaf = True

    def __init__(self, cache_service):
        resource.Resource.__init__(self)
        self.cache_service = cache_service


    def render_GET(self, request):
        try:
            clienthost = request.getClientAddress().host # twisted >= 18.4
        except:
            clienthost = request.getClientIP() # twisted < 18.4

        def getClient(host):
            try:
                name, names, addresses = socket.gethostbyaddr(host)
            except socket.error:
                return host
            names.insert(0, name)
            for name in names:
                if '.' in name:
                    return name
            return names[0]

        client = getClient(clienthost) + "/" + clienthost
        log.msg("GET request on cache from %s" % client)

        gen_time, hashes, cache, cache_url = self.cache_service.getCache()

        request.setHeader(b'Content-type', b'application/vnd.org.ndgf.acix.bloomfilter')
        
        if not cache:
            log.msg("Cache content has not been built yet")
            cache = b''
            gen_time = 0
            
        request.setHeader(b'Content-length', str(len(cache)).encode())

        request.setHeader(b'x-hashes', ','.join(hashes).encode())
        request.setHeader(b'x-cache-time', str(gen_time).encode())
        request.setHeader(b'x-cache-url', str(cache_url).encode())

        return cache

