from arcom.service import rbyteio_uri, byteio_simple_uri

from storage.common import create_checksum, upload_to_turl, download_from_turl
from storage.client import NotifyClient, ByteIOClient
import traceback
import arc
import base64
import os

from arcom.logger import Logger
log = Logger(arc.Logger(arc.Logger_getRootLogger(), 'Storage.ByteIO'))

class ByteIOBackend:

    public_request_names = ['notify']
    supported_protocols = ['byteio']

    def __init__(self, backendcfg, ns_uri, file_arrived, ssl_config):
        self.file_arrived = file_arrived
        self.ns = arc.NS('she', ns_uri)
        self.ssl_config = ssl_config
        self.datadir = str(backendcfg.Get('StoreDir'))
        self.transferdir = str(backendcfg.Get('TransferDir'))
        self.turlprefix = str(backendcfg.Get('TURLPrefix'))
        if not os.path.exists(self.datadir):
            os.mkdir(self.datadir)
        log.msg(arc.VERBOSE, "ByteIOBackend datadir:", self.datadir)
        if not os.path.exists(self.transferdir):
            os.mkdir(self.transferdir)
        else:
            for filename in os.listdir(self.transferdir):
                os.remove(os.path.join(self.transferdir, filename))
        log.msg(arc.VERBOSE, "ByteIOBackend transferdir:", self.transferdir)
        self.idstore = {}

    def copyTo(self, localID, turl, protocol):
        f = file(os.path.join(self.datadir, localID),'rb')
        log.msg(arc.VERBOSE, self.turlprefix, 'Uploading file to', turl)
        upload_to_turl(turl, protocol, f, ssl_config = self.ssl_config)
        f.close()
    
    def copyFrom(self, localID, turl, protocol):
        # TODO: download to a separate file, and if checksum OK, then copy the file 
        f = file(os.path.join(self.datadir, localID), 'wb')
        log.msg(arc.VERBOSE, self.turlprefix, 'Downloading file from', turl)
        download_from_turl(turl, protocol, f, ssl_config = self.ssl_config)
        f.close()

    def prepareToGet(self, referenceID, localID, protocol):
        if protocol not in self.supported_protocols:
            raise Exception, 'Unsupported protocol: ' + protocol
        turl_id = arc.UUID()
        try:
            os.link(os.path.join(self.datadir, localID), os.path.join(self.transferdir, turl_id))
            self.idstore[turl_id] = referenceID
            log.msg(arc.VERBOSE, self.turlprefix, '++', self.idstore)
            turl = self.turlprefix + turl_id
            return turl
        except:
            return None

    def prepareToPut(self, referenceID, localID, protocol):
        if protocol not in self.supported_protocols:
            raise Exception, 'Unsupported protocol: ' + protocol
        turl_id = arc.UUID()
        datapath = os.path.join(self.datadir, localID)
        f = file(datapath, 'wb')
        f.close()
        os.link(datapath, os.path.join(self.transferdir, turl_id))
        self.idstore[turl_id] = referenceID
        log.msg(arc.VERBOSE, self.turlprefix, '++', self.idstore)
        turl = self.turlprefix + turl_id
        return turl

    def remove(self, localID):
        try:
            fn = os.path.join(self.datadir, localID)
            os.remove(fn)
        except:
            return 'failed: ' + traceback.format_exc()
        return 'removed'

    def list(self):
        return os.listdir(os.datadir)

    def getAvailableSpace(self):
        return None

    def generateLocalID(self):
        return arc.UUID()

    def matchProtocols(self, protocols):
        return [protocol for protocol in protocols if protocol in self.supported_protocols]

    def notify(self, inpayload):
        request_node = inpayload.Get('notify')
        subject = str(request_node.Get('subject'))
        referenceID = self.idstore.get(subject, None)
        state = str(request_node.Get('state'))
        path = os.path.join(self.transferdir, subject)
        log.msg(arc.VERBOSE, self.turlprefix, 'Removing', path)
        os.remove(path)
        self.file_arrived(referenceID)
        out = arc.PayloadSOAP(self.ns)
        response_node = out.NewChild('she:notifyResponse').Set('OK')
        return out

    def checksum(self, localID, checksumType):
        return create_checksum(file(os.path.join(self.datadir, localID), 'rb'), checksumType)

from arcom.security import parse_ssl_config
from arcom.service import Service

class ByteIOService(Service):

    def __init__(self, cfg):
        self.service_name = 'ByteIO'
        # names of provided methods
        request_names = ['read', 'write']
        # call the Service's constructor
        Service.__init__(self, [{'request_names' : request_names, 'namespace_prefix': 'rb', 'namespace_uri': rbyteio_uri}], cfg)
        self.transferdir = str(cfg.Get('TransferDir'))
        log.msg(arc.VERBOSE, "ByteIOService transfer dir:", self.transferdir)
        ssl_config = parse_ssl_config(cfg)
        self.notify = NotifyClient(str(cfg.Get('NotifyURL')), ssl_config = ssl_config)

    def _filename(self, subject):
        return os.path.join(self.transferdir, subject)

    def write(self, inpayload, subject):
        request_node = inpayload.Child()
        transfer_node = request_node.Get('transfer-information')
        if str(transfer_node.Attribute(0)) != byteio_simple_uri:
            raise Exception, 'transfer-mechanism not supported'
        try:
            fn = self._filename(subject)
            file(fn) # check existance
            f = file(fn,'wb') # open for overwriting
        except:
            log.msg()
            raise Exception, 'denied'
        encoded_data = str(transfer_node)
        data = base64.b64decode(encoded_data)
        try:
            f.write(data)
            f.close()
        except:
            log.msg()
            raise Exception, 'write failed'
        self.notify.notify(subject, 'received')
        out = self._new_soap_payload()
        response_node = out.NewChild('rb:writeResponse').Set('OK')
        return out

    def read(self, inpayload, subject):
        try:
            data = file(self._filename(subject),'rb').read()
        except:
            log.msg()
            data = ''
        self.notify.notify(subject, 'sent')
        out = self._new_soap_payload()
        response_node = out.NewChild('rb:readResponse')
        transfer_node = response_node.NewChild('rb:transfer-information')
        transfer_node.NewAttribute('transfer-mechanism').Set(byteio_simple_uri)
        encoded_data = base64.b64encode(data)
        transfer_node.Set(encoded_data)
        return out


    def _call_request(self, request_name, inmsg):
        # gets the last part of the request url
        # TODO: somehow detect if this is just the path of the service which means: no subject
        subject = inmsg.Attributes().get('ENDPOINT').split('/')[-1]
        # the subject of the byteio request: reference to the file
        log.msg(arc.VERBOSE, 'Subject:', subject)
        inpayload = inmsg.Payload()
        return getattr(self,request_name)(inpayload, subject)

