#!/usr/bin/env python
import arc, sys, os
root_logger = arc.Logger_getRootLogger()
root_logger.addDestination(arc.LogStream(sys.stdout))
root_logger.setThreshold(arc.VERBOSE)

from arcom import datapoint_from_url

try:
    src = sys.argv[1]
    dst = sys.argv[2]
except:
    print "Usage: mover.py fromURL toURL"
    sys.exit(-1)

ssl_config = {}
user_config = arc.UserConfig("")
key_file = user_config.KeyPath()
cert_file = user_config.CertificatePath()
proxy_file = user_config.ProxyPath()
ca_file = user_config.CACertificatePath()
ca_dir = user_config.CACertificatesDirectory()
key_file = os.environ.get('ARC_KEY_FILE', key_file)
cert_file = os.environ.get('ARC_CERT_FILE', cert_file)
proxy_file = os.environ.get('ARC_PROXY_FILE', proxy_file)
ca_file = os.environ.get('ARC_CA_FILE', ca_file)
ca_dir = os.environ.get('ARC_CA_DIR', ca_dir)
if proxy_file:
    ssl_config['proxy_file'] = proxy_file
    print '- The proxy certificate file:', ssl_config['proxy_file']
else:
    if key_file and cert_file:                
        ssl_config['key_file'] = key_file
        ssl_config['cert_file'] = cert_file
        print '- The key file:', ssl_config['key_file']
        print '- The cert file:', ssl_config['cert_file']
if ca_file:
    ssl_config['ca_file'] = ca_file
    print '- The CA file:', ssl_config['ca_file']
elif ca_dir:
    ssl_config['ca_dir'] = ca_dir
    print '- The CA dir:', ssl_config['ca_dir']
    
src = datapoint_from_url(src)
src.AssignCredentials(ssl_config.get('proxy_file',''), ssl_config.get('cert_file', '') ,ssl_config.get('key_file', ''), ssl_config.get('ca_dir', ''))
dst = datapoint_from_url(dst)
dst.AssignCredentials(ssl_config.get('proxy_file',''), ssl_config.get('cert_file', '') ,ssl_config.get('key_file', ''), ssl_config.get('ca_dir', ''))
            
mover = arc.DataMover()
mover.verbose(True)
mover.retry(False)
status = mover.Transfer(src, dst, arc.FileCache(), arc.URLMap())
print status
