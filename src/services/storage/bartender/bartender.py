import urlparse
import httplib
import arc
import random
import time
import traceback
import os
import base64
import threading
from arcom import import_class_from_string, get_child_nodes, get_child_values_by_name
from arcom.service import librarian_uri, bartender_uri, librarian_servicetype, gateway_uri, true, parse_node, create_response, node_to_data, bartender_servicetype
from arcom.xmltree import XMLTree
from storage.client import LibrarianClient, ShepherdClient, ISISClient
from storage.common import parse_metadata, create_metadata, splitLN, remove_trailing_slash, global_root_guid, serialize_ids, deserialize_ids, sestore_guid, make_decision_metadata
import traceback

from storage.common import ALIVE, CREATING, THIRDWHEEL


from arcom.logger import Logger
log = Logger(arc.Logger(arc.Logger_getRootLogger(), 'Storage.Bartender'))

class Bartender:

    def __init__(self, cfg, ssl_config, service_state):
        """ Constructor of the Bartender business-logic class.
        
        """
        self.service_state = service_state
        gatewayclass = str(cfg.Get('GatewayClass'))
        self.gateway = None
        if gatewayclass :
            cl = import_class_from_string(gatewayclass) 
            gatewaycfg = cfg.Get('GatewayCfg')
            self.gateway = cl(gatewaycfg)
            log.msg(arc.INFO, 'accessing gateway: %s', gatewayclass)
        else:
            log.msg(arc.INFO, 'This bartender does not support gateway') 
            log.msg(arc.INFO, 'cannot connect to gateway. Access of third party store required gateway.')
        self.ssl_config = ssl_config
        
        try:
            self.isis_checking_period = float(str(cfg.Get('ISISCheckginPeriod')));
        except:
            self.isis_checking_period = 120;
            
        # get the URLs of the Librarians from the config file
        librarian_urls =  get_child_values_by_name(cfg, 'LibrarianURL')
        if librarian_urls:
            log.msg(arc.INFO,'Got Librarian URLs from the config:', librarian_urls)
            self.librarian = LibrarianClient(librarian_urls, ssl_config = self.ssl_config)
            self.service_state.running = True
        else:
            isis_urls = get_child_values_by_name(cfg, 'ISISURL')
            if not isis_urls:
                log.msg(arc.ERROR, "Librarian URL or ISIS URL not found in the configuration.")
                raise Exception, 'Bartender cannot run with no Librarian'
            log.msg(arc.INFO,'Got ISIS URL, starting initThread')
            threading.Thread(target = self.initThread, args = [isis_urls]).start()

    def initThread(self, isis_urls):
        librarian_found = False
        while not librarian_found:
            try:
                log.msg(arc.INFO,'Getting Librarians from ISISes')
                for isis_url in isis_urls:
                    log.msg(arc.INFO,'Trying to get Librarian from', isis_url)
                    isis = ISISClient(isis_url, ssl_config = self.ssl_config)
                    try:
                        librarian_urls = isis.getServiceURLs(librarian_servicetype)
                        log.msg(arc.INFO,'Got Librarian from ISIS:', librarian_urls)
                        if librarian_urls:
                            self.librarian = LibrarianClient(librarian_urls, ssl_config = self.ssl_config)
                            librarian_found = True
                    except:
                        log.msg(arc.VERBOSE, 'Error connecting to ISIS %(iu)s, reason: %(r)s' % {'iu' : isis_url, 'r' : traceback.format_exc()})
                time.sleep(3)
            except Exception, e:
                log.msg(arc.WARNING, 'Error in initThread: %s' % e)                
        log.msg(arc.INFO,'initThread finished, starting isisThread')
        self.service_state.running = True
        threading.Thread(target = self.isisThread, args = [isis_urls]).start()
            
    def isisThread(self, isis_urls):
        while self.service_state.running:
            try:
                time.sleep(self.isis_checking_period)
                log.msg(arc.INFO,'Getting Librarians from ISISes')
                for isis_url in isis_urls:
                    if not self.service_state.running:
                        return
                    log.msg(arc.INFO,'Trying to get Librarian from', isis_url)
                    isis = ISISClient(isis_url, ssl_config = self.ssl_config)        
                    librarian_urls = isis.getServiceURLs(librarian_servicetype)
                    log.msg(arc.INFO, 'Got Librarian from ISIS:', librarian_urls)
                    if librarian_urls:
                        self.librarian = LibrarianClient(librarian_urls, ssl_config = self.ssl_config)
                        break
            except Exception, e:
                log.msg(arc.WARNING, 'Error in isisThread: %s' % e)                

    def stat(self, auth, requests):
        """ Returns stat information about entries.
        
        stat(auth, requests)
        
        auth is an AuthRequest object containing information about the user's identity
        requests is a dictionary with requestIDs as keys, and Logical Names as values.
    
        Returns a dictionary with requestIDs as keys, and metadata as values.
        The 'metadata' is a dictionary with (section, property) pairs as keys.
        """
        response = {}
        # get the information from the librarian
        requests, traverse_response = self._traverse(requests)
        # we are only interested in the metadata and if the traversing was complete or not
        for requestID, (metadata, _, _, _, wasComplete, _) in traverse_response.items():
            if wasComplete: # if it was complete, then we found the entry and got the metadata
                try:
                    decision = make_decision_metadata(metadata, auth.get_request('read'))
                    if decision != arc.DECISION_PERMIT:
                        metadata = {('error','permission denied') : 'you are not allowed to read'}
                except:
                    #log.msg()
                    metadata = {('error', 'error checking permission') : traceback.format_exc()}
                response[requestID] = metadata
            else: # if it was not complete, then we didn't found the entry, so metadata will be empty
                response[requestID] = {}
        return response
   
    def delFile(self, auth, requests):
        """ Delete a file from the storage: initiate the process.
        
        delFile(requests)
        
        requests is a dictionary with requestID as key and (Logical Name, child metadata, protocols) as value
        """
        auth_request = auth.get_request('delete')
        import time
        response = {}
        # get the information from the librarian
        #requests, traverse_response = self._traverse(requests)
        traverse_response = self.librarian.traverseLN(requests)
        cat_rem_requests = {}
        cat_mod_requests = {}
        check_again = []
        for requestID, (metadata, GUID, LN, restLN, wasComplete, traversedList) in traverse_response.items():
            decision = make_decision_metadata(metadata, auth_request)
            if decision != arc.DECISION_PERMIT:
                response[requestID] = 'denied'
            elif wasComplete and metadata[('entry', 'type')]=='file': # if it was complete, then we found the entry and got the metadata
                # remove the file
                cat_rem_requests[requestID] = GUID
                # if this entry has a parent:
                parents = [p for (s,p) in metadata.keys() if s == 'parents']
                for parent in parents:
                    parent_GUID, child_name = parent.split('/')
                    cat_mod_requests[requestID + '-' + parent] = (parent_GUID, 'unset', 'entries', child_name, GUID)
                    cat_mod_requests[requestID + '-' + parent + '-closed?'] = (parent_GUID, 'setifvalue=yes', 'states', 'closed', 'broken')
                response[requestID] = 'deleted'
            elif metadata.get(('entry', 'type'), '') == 'mountpoint' :
                url = metadata[('mountpoint', 'externalURL')]+'/'+restLN
                #print url
                res = self._externalStore(auth ,url,'delFile')
                #print res
                #print res[url]['status']  
                if 'successfully' in res[url]['status']:
                    response[requestID] = 'deleted'
                else:
                    response[requestID] = 'nosuchLN'       
            else: # if it was not complete, then we didn't find the entry, so metadata will be empty
                response[requestID] = 'nosuchLN'
        #print cat_rem_requests
        #print cat_mod_requests
        success = self.librarian.remove(cat_rem_requests)
        modify_success = self.librarian.modifyMetadata(cat_mod_requests)
        #print success
        #print modify_success 
        #print response
        return response

    def _traverse(self, requests):
        """ Helper method which connects the librarian, and traverses the LNs of the requests.
        
        _traverse(requests)
        
        Removes the trailing slash from  all the LNs in the request.
        Returns the requests and the traverse response.
        """
        # in each request the requestID is the key and the value is a list
        # the first item of the list is the Logical Name, we want to remove the trailing slash, and
        # leave the other items intact
        requests = [(rID, [remove_trailing_slash(data[0])] + list(data[1:])) for rID, data in requests.items()]
        log.msg(arc.VERBOSE, '//// _traverse request trailing slash removed:', dict(requests))
        # then we do the traversing. a traverse request contains a requestID and the Logical Name
        # so we just need the key (which is the request ID) and the first item of the value (which is the LN)
        traverse_request = dict([(rID, data[0]) for rID, data in requests])
        # call the librarian service
        traverse_response = self.librarian.traverseLN(traverse_request)
        if not traverse_response:
            raise Exception, 'Empty response from the Librarian'
        # return the requests as list (without the trailing slashes) and the traverse response from the librarian
        return requests, traverse_response


    def _new(self, auth, child_metadata, child_name = None, parent_GUID = None, parent_metadata = {}):
        """ Helper method which create a new entry in the librarian.
        
        _new(child_metadata, child_name = None, parent_GUID = None)
        
        child_metadata is a dictionary with {(section, property) : values} containing the metadata of the new entry
        child_name is the name of the new entry 
        parent_GUID is the GUID of the parent of the new entry
        
        This method creates a new librarian-entry with the given metadata.
        If child_name and parent_GUID are both given, then this method adds a new entry to the parent collection.
        """
        try:
            # set creation time stamp
            child_metadata[('timestamps', 'created')] = str(time.time())
            if child_name and parent_GUID:
                child_metadata[('parents', '%s/%s' % (parent_GUID, child_name))] = 'parent' # this 'parent' string is never used
            # call the new method of the librarian with the child's metadata (requestID is '_new')
            new_response = self.librarian.new({'_new' : child_metadata})
            # we can access the response with the requestID, so we get the GUID of the newly created entry
            (child_GUID, new_success) = new_response['_new']
            # if the new method was not successful
            if new_success != 'success':
                return 'failed to create new librarian entry', child_GUID
            else:
                # if it was successful and we have a parent collection
                if child_name and parent_GUID:
                    decision = make_decision_metadata(parent_metadata, auth.get_request('addEntry'))
                    if decision == arc.DECISION_PERMIT:
                        # we need to add the newly created librarian-entry to the parent collection
                        log.msg(arc.VERBOSE, 'adding', child_GUID, 'to parent', parent_GUID)
                        # this modifyMetadata request adds a new (('entries',  child_name) : child_GUID) element to the parent collection
                        modify_response = self.librarian.modifyMetadata({'_new' : (parent_GUID, 'add', 'entries', child_name, child_GUID),
                                                                        '_new_closed?' : (parent_GUID, 'setifvalue=yes', 'states', 'closed', 'broken')})
                        log.msg(arc.VERBOSE, 'modifyMetadata response', modify_response)
                        # get the 'success' value
                        modify_success = modify_response['_new']
                    else:
                        modify_success = 'denied'
                    # if the new element was not set, we have a problem
                    if modify_success != 'set':
                        log.msg(arc.VERBOSE, 'modifyMetadata failed, removing the new librarian entry', child_GUID)
                        # remove the newly created librarian-entry
                        self.librarian.remove({'_new' : child_GUID})
                        return 'failed to add child to parent', child_GUID
                    else:
                        return 'done', child_GUID
                else: # no parent given, skip the 'adding child to parent' part
                    return 'done', child_GUID
        except Exception, e:
            log.msg(arc.ERROR, "Error creating new entry in Librarian: %s" % e)
            return 'internal error', None
        
    def _externalStore(self, auth, url, flag=''):
        """ This method calles the gateway backend class to get/check the full URL of the externally stored file"""
        response = {}
        if self.gateway != None:
            if flag == 'list':      
                response = self.gateway.list(auth, url, flag)
            elif flag == 'getFile':
                response = self.gateway.get(auth, url, flag)
            elif flag == 'delFile':
                response = self.gateway.remove(auth, url, flag)
            elif flag == 'putFile': 
                 response = self.gateway.put(auth, url, flag)
        else:
            if flag == 'list':
                response[url] = {'list': '', 'status': 'gateway is not configured. Bartender does not support mount points', 'protocol':''}            
            else:
                response[url]={'turl': '' ,'status': 'gateway is not configured. Bartender does not support mount points', 'protocol':''}
        log.msg(arc.VERBOSE, '//// response from the external store:', response)
        return response

    def getFile(self, auth, requests):
        """ Get files from the storage.
        
        getFile(requests)
        
        requests is a dictionary with requestID as key, and (Logical Name, protocol list) as value
        """
        auth_request = auth.get_request('read')
        # call the _traverse helper method the get the information about the requested Logical Names
        requests, traverse_response = self._traverse(requests)
        response = {}
        #print traverse_response
        # for each requested LN
        for rID, (LN, protocols) in requests:
            turl = ''
            protocol = ''
            success = 'unknown'
            try:
                log.msg(arc.VERBOSE, traverse_response[rID])
                # split the traverse response
                metadata, GUID, traversedLN, restLN, wasComplete, traversedList = traverse_response[rID]
                # wasComplete is true if the given LN was found, so it could have been fully traversed
                if not wasComplete:
                    if metadata.get(('entry', 'type'), '') == 'mountpoint':
                        url = metadata[('mountpoint', 'externalURL')]
                        res = self._externalStore(auth ,url+'/'+restLN,'getFile')
                        if res:
                            for key in res.keys():
                                turl = res[key]['turl']
                                protocol = res[key]['protocol'] 
                                if res[key]['status'] == 'successful':
                                    success = 'done'
                                else:
                                    success = res[key]['status']  
                        else:   
                            success = 'not found'
                    else:       
                        success = 'not found'
                else:
                    # metadata contains all the metadata of the given entry
                    # ('entry', 'type') is the type of the entry: file, collection, etc.
                    decision = make_decision_metadata(metadata, auth_request)
                    if decision != arc.DECISION_PERMIT:
                        success = 'denied'
                    else:
                        type = metadata[('entry', 'type')]
                        if type != 'file':
                            success = 'is not a file'
                        else:
                            # if it is a file,  then we need all the locations where it is stored and alive
                            # this means all the metadata entries with in the 'locations' sections whose value is ALIVE
                            # the location itself serialized from the ID of the service and the ID of the replica within the service
                            # so the location needs to be deserialized into two ID with deserialize_ids()
                            # valid_locations will contain a list if (serviceID, referenceID, state)
                            valid_locations = [deserialize_ids(location) + [state] for (section, location), state in metadata.items() if section == 'locations' and state == ALIVE]
                            # if this list is empty
                            if not valid_locations:
                                success = 'file has no valid replica'
                            else:
                                ok = False
                                while not ok and len(valid_locations) > 0:
                                    # if there are more valid_locations, randomly select one
                                    location = valid_locations.pop(random.choice(range(len(valid_locations))))
                                    log.msg(arc.VERBOSE, 'location chosen:', location)
                                    # split it to serviceID, referenceID - serviceID currently is just a plain URL of the service
                                    url, referenceID, _ = location
                                    # create an ShepherdClient with this URL, then send a get request with the referenceID
                                    try:
                                        get_response = dict(ShepherdClient(url, ssl_config = self.ssl_config).get({'getFile' :
                                            [('referenceID', referenceID)] + [('protocol', proto) for proto in protocols]})['getFile'])
                                    except:
                                        get_response = {'error' : traceback.format_exc()}
                                    # get_response is a dictionary with keys such as 'TURL', 'protocol' or 'error'
                                    if get_response.has_key('error'):
                                        # if there was an error
                                        log.msg(arc.VERBOSE, 'ERROR from the chosen Shepherd', get_response['error'])
                                        success = 'error while getting TURL (%s)' % get_response['error']
                                    else:
                                        # get the TURL and the choosen protocol, these will be set as reply for this requestID
                                        turl = get_response['TURL']
                                        protocol = get_response['protocol']
                                        success = 'done'
                                        ok = True
            except:
                success = 'internal error (%s)' % traceback.format_exc()
            # set the success, turl, protocol for this request
            response[rID] = (success, turl, protocol)
        return response
   
    def addReplica(self, auth, requests, protocols):
        """ This method initiates the addition of a new replica to a file.
        
        addReplica(requests, protocols)
        
        requests is a dictionary with requestID-GUID pairs
        protocols is a list of supported protocols
        """
        # currently we permit everyone to upload a new replica,
        #   if it not matches the checksum or the size, then it will be removed
        # get the size and checksum information about all the requested GUIDs (these are in the 'states' section)
        #   the second argument of the get method specifies that we only need metadata from the 'states' section
        data = self.librarian.get(requests.values(), [('states',''),('locations',''),('policy','')])
        response = {}
        for rID, GUID in requests.items():
            # for each requested GUID
            metadata = data[GUID]
            log.msg(arc.VERBOSE, 'addReplica', 'requestID', rID, 'GUID', GUID, 'metadata', metadata, 'protocols', protocols)
            # get the size and checksum information of the file
            size = metadata[('states','size')]
            checksumType = metadata[('states','checksumType')]
            checksum = metadata[('states','checksum')]
            # list of shepherds with an alive or creating replica of this file (to avoid using one shepherd twice)
            exceptedSEs = [deserialize_ids(location)[0] 
                           for (section, location), status in metadata.items() if section == 'locations' and status in [ALIVE, CREATING, THIRDWHEEL]]
            # initiate replica addition of this file with the given protocols 
            success, turl, protocol = self._add_replica(size, checksumType, checksum, GUID, protocols, exceptedSEs)
            # set the response of this request
            response[rID] = (success, turl, protocol)
        return response

    def _find_alive_ses(self, except_these=[]):
        """  Get the list of currently alive Shepherds.
        
        _find_alive_ses()
        """
        # sestore_guid is the GUID of the librarian entry which the list of Shepherds registered by the Librarian
        SEs = self.librarian.get([sestore_guid])[sestore_guid]
        # SEs contains entries such as {(serviceID, 'nextHeartbeat') : timestamp} which indicates
        #   when a specific Shepherd service should report next
        #   if this timestamp is not a positive number, that means the Shepherd have not reported in time, probably it is not alive
        log.msg(arc.VERBOSE, 'Registered Shepherds in Librarian', SEs)
        # get all the Shepherds which has a positiv nextHeartbeat timestamp and which has not already been used
        alive_SEs = [s for (s, p), v in SEs.items() if p == 'nextHeartbeat' and int(v) > 0 and not s in except_these]
        log.msg(arc.VERBOSE, 'Alive Shepherds:', alive_SEs)
        response = []
        for se in alive_SEs:
            response.append(ShepherdClient(se, ssl_config = self.ssl_config))
        return response

    def _add_replica(self, size, checksumType, checksum, GUID, protocols, exceptedSEs=[]):
        """ Helper method to initiate addition of a replica to a file.
        
        _add_replica(size, checksumType, checksum, GUID, protocols)
        
        size is the size of the file
        checksumType indicates the type of the checksum
        checksum is the checksum itself
        GUID is the GUID of the file
        protocols is a list of protocols
        """
        turl = ''
        protocol = ''
        # prepare the 'put' request for the shepherd
        put_request = [('size', size), ('checksumType', checksumType),
            ('checksum', checksum), ('GUID', GUID)] + \
            [('protocol', protocol) for protocol in protocols]
        # find an alive Shepherd
        shepherds = self._find_alive_ses(exceptedSEs)
        random.shuffle(shepherds)
        while len(shepherds) > 0:
            shepherd = shepherds.pop()
            # call the SE's put method with the prepared request
            try:
                put_response = dict(shepherd.put({'putFile': put_request})['putFile'])
            except:
                put_response = {'error' : traceback.format_exc()}
            if put_response.has_key('error'):
                log.msg(arc.VERBOSE, 'ERROR from the chosen Shepherd', put_response['error'])
            else:
                # if the put request was successful then we have a transfer URL, a choosen protocol and the referenceID of the file
                turl = put_response['TURL']
                protocol = put_response['protocol']
                return 'done', turl, protocol
        return 'no suitable shepherd found', turl, protocol
            
    def putFile(self, auth, requests):
        """ Put a new file to the storage: initiate the process.
        
        putFile(requests)
        
        requests is a dictionary with requestID as key and (Logical Name, child metadata, protocols) as value
        """
        # get all the information about the requested Logical Names from the librarian
        requests, traverse_response = self._traverse(requests)
        response = {}
        for rID, (LN, child_metadata, protocols) in requests:
            # for each request
            turl = ''
            protocol = ''
            metadata_ok = False
            try:
                # get the size and checksum of the new file
                log.msg(arc.VERBOSE, protocols)
                size = child_metadata[('states', 'size')]
                checksum = child_metadata[('states', 'checksum')]
                checksumType = child_metadata[('states', 'checksumType')]
                # need neededReplicas to see if we should call _add_replica
                neededReplicas = child_metadata[('states', 'neededReplicas')]
                metadata_ok = True
            except Exception, e:
                success = 'missing metadata ' + str(e)
            if metadata_ok:
                # if the metadata of the new file is OK
                child_metadata[('entry','type')] = 'file'
                child_metadata[('entry','owner')] = auth.get_identity()
                try:
                    # split the Logical Name, rootguid will be the GUID of the root collection of this LN,
                    #   child_name is the name of the new file withing the parent collection
                    rootguid, _, child_name = splitLN(LN)
                    log.msg(arc.VERBOSE, 'LN', LN, 'rootguid', rootguid, 'child_name', child_name, 'real rootguid', rootguid or global_root_guid)
                    # get the traverse response corresponding to this request
                    #   metadata is the metadata of the last element which could been traversed, e.g. the parent of the new file
                    #   GUID is the GUID of the same
                    #   traversedLN indicates which part of the requested LN could have been traversed
                    #   restLN is the non-traversed part of the Logical Name, e.g. the filename of a non-existent file whose parent does exist
                    #   wasComplete indicates if the traverse was complete or not, if it was complete means that this LN exists
                    #   traversedlist contains the GUID and metadata of each element along the path of the LN
                    metadata, GUID, traversedLN, restLN, wasComplete, traversedlist = traverse_response[rID]
                    log.msg(arc.VERBOSE, 'metadata', metadata, 'GUID', GUID, 'traversedLN', traversedLN, 'restLN', restLN, 'wasComplete',wasComplete, 'traversedlist', traversedlist)
                    # if the traversing stopped at a mount point:
                    if metadata.get(('entry','type'), '') == 'mountpoint':
                        url = metadata[('mountpoint','externalURL')] + '/' + restLN                
                        res = self._externalStore(auth, url, 'putFile') 
                        response[rID] = (res[url]['status'],res[url]['turl'],res[url]['protocol']) 
                        return response    
                    if wasComplete: # this means the LN already exists, so we couldn't put a new file there
                        success = 'LN exists'
                    elif child_name == '': # this only can happen if the LN was a single GUID
                        # this means that the new file will have no parent
                        # we don't want to allow this:
                        success = 'cannot create a file without a parent collection'
                        # # set the type and GUID of the new file
                        # child_metadata[('entry','GUID')] = rootguid or global_root_guid
                        # # create the new entry
                        # success, GUID = self._new(auth, child_metadata)
                    elif restLN != child_name or GUID == '':
                        # if the non-traversed part of the Logical Name is not actully the name of the new file
                        #   or we have no parent guid
                        success = 'parent does not exist'
                    else:
                        # if everything is OK, then we create the new entry
                        success, GUID = self._new(auth, child_metadata, child_name, GUID, metadata)
                    if success == 'done':
                        # if the file was successfully created, it still has no replica, so we initiate creating one
                        # if neededReplicas is 0, we do nothing
                        if int(neededReplicas) > 0: # this will call shepherd.put()
                            success, turl, protocol = self._add_replica(size, checksumType, checksum, GUID, protocols)
                except:
                    success = 'internal error (%s)' % traceback.format_exc()
            response[rID] = (success, turl, protocol)
        return response
        
    def unlink(self, auth, requests):
        """docstring for unlink"""
        auth_request = auth.get_request('removeEntry')
        requests, traverse_response = self._traverse(requests)
        response = {}
        for rID, [LN] in requests:
            metadata, GUID, traversedLN, restLN, wasComplete, traversedlist = traverse_response[rID]
            #print 'metadata', metadata, 'GUID', GUID, 'traversedLN', traversedLN, 'restLN', restLN, 'wasComplete',wasComplete, 'traversedlist', traversedlist
            if not wasComplete:
                success = 'no such LN'
            else:
                if len(traversedlist) < 2:
                    success = 'nothing to unlink'
                else:
                    parent_GUID = traversedlist[-2][1]
                    child_name = traversedlist[-1][0]
                    parent_metadata = self.librarian.get([parent_GUID])[parent_GUID]
                    decision = make_decision_metadata(parent_metadata, auth_request)
                    if decision != arc.DECISION_PERMIT:
                        success = 'denied'
                    else:
                        mod_requests = {'unlink' : (parent_GUID, 'unset', 'entries', child_name, ''),
                                        'unlink-closed?' : (parent_GUID, 'setifvalue=yes', 'states', 'closed', 'broken')}
                        mod_response = self.librarian.modifyMetadata(mod_requests)
                        success = mod_response['unlink']
            response[rID] = success
        return response

    def unmakeCollection(self, auth, requests):
        """docstring for unmakeCollection"""
        auth_request = auth.get_request('delete')
        requests, traverse_response = self._traverse(requests)
        response = {}
        for rID, [LN] in requests:
            metadata, GUID, traversedLN, restLN, wasComplete, traversedlist = traverse_response[rID]
            log.msg(arc.VERBOSE, 'metadata', metadata, 'GUID', GUID, 'traversedLN', traversedLN, 'restLN', restLN, 'wasComplete',wasComplete, 'traversedlist', traversedlist)
            if not wasComplete:
                success = 'no such LN'
            else:
                decision = make_decision_metadata(metadata, auth_request)
                if decision != arc.DECISION_PERMIT:
                    success = 'denied'
                else:
                    number_of_entries = len([section for (section, _), _ in metadata.items() if section == 'entries'])
                    if number_of_entries > 0:
                        success = 'collection is not empty'
                    else:
                        try:
                            parentLN, parentGUID = traversedlist[-2]
                            # TODO: get the metadata of the parent, and check if the user has permission to removeEntry from it
                            mod_requests = {'unmake' : (parentGUID, 'unset', 'entries', traversedlist[-1][0], ''),
                                            'unmake-closed?' : (parentGUID, 'setifvalue=yes', 'states', 'closed', 'broken')}
                            mod_response = self.librarian.modifyMetadata(mod_requests)
                            success = mod_response['unmake']
                        except IndexError:
                            # it has no parent
                            success = 'unset'
                        if success == 'unset':
                            # TODO: handle hardlinks to collections
                            success = self.librarian.remove({'unmake' : GUID})['unmake']
            response[rID] = success
        return response

    def makeCollection(self, auth, requests):
        """ Create a new collection.
        
        makeCollection(requests)
        
        requests is dictionary with requestID as key and (Logical Name, metadata) as value
        """
        # do traverse all the requests
        requests, traverse_response = self._traverse(requests)
        response = {}
        for rID, (LN, child_metadata) in requests:
            # for each request first split the Logical Name
            rootguid, _, child_name = splitLN(LN)
            metadata, GUID, traversedLN, restLN, wasComplete, traversedlist = traverse_response[rID]
            #print metadata[('entry', 'type')]
            if metadata.get(('entry', 'type'), '') == 'mountpoint':
                success = 'cannot create collection in mountpoint'      
                response[rID] = success    
                return response
            log.msg(arc.VERBOSE, 'metadata', metadata, 'GUID', GUID, 'traversedLN', traversedLN, 'restLN', restLN, 'wasComplete',wasComplete, 'traversedlist', traversedlist)
            child_metadata[('entry','owner')] = auth.get_identity()
            child_metadata[('entry','type')] = 'collection'
            if wasComplete: # this means the LN exists
                success = 'LN exists'
            elif child_name == '': # this only can happen if the LN was a single GUID
                # this means the collection has no parent
                # we only allow this for the global root
                if rootguid != global_root_guid and rootguid != '':
                    success = 'cannot create collection without a parent collection'
                else:
                    child_metadata[('entry','GUID')] = global_root_guid
                    success, _ = self._new(auth, child_metadata)
            elif restLN != child_name or GUID == '':
                success = 'parent does not exist'
            else:
                # if everything is OK, create the new collection
                #   here GUID is of the parent collection
                success, _ = self._new(auth, child_metadata, child_name, GUID, metadata)
            response[rID] = success
        return response

    ### Created by Salman Toor ###
    
    def unmakeMountpoint(self, auth, requests):        
        """docstring for unmakeMountpoint"""  
        auth_request = auth.get_request('delete')      
        requests, traverse_response = self._traverse(requests)
        response = {}
        for rID, [LN] in requests:
            metadata, GUID, traversedLN, restLN, wasComplete, traversedlist = traverse_response[rID]
            log.msg(arc.VERBOSE, 'metadata', metadata, 'GUID', GUID, 'traversedLN', traversedLN, 'restLN', restLN, 'wasComplete',wasComplete, 'traversedlist', traversedlist)
            if not wasComplete:
                success = 'no such LN'
            else:
                decision = make_decision_metadata(metadata, auth_request)
                if decision != arc.DECISION_PERMIT:
                    success = 'denied'
                else:
                    try:
                        parentLN, parentGUID = traversedlist[-2]
                        # TODO: get the metadata of the parent, and check if the user has permission to removeEntry from it
                        mod_requests = {'unmake' : (parentGUID, 'unset', 'entries', traversedlist[-1][0], '')}
                        mod_response = self.librarian.modifyMetadata(mod_requests)
                        success = mod_response['unmake']
                    except IndexError:
                        # it has no parent
                        success = 'unset'
                    if success == 'unset':
                        success = self.librarian.remove({'unmake' : GUID})['unmake']
            response[rID] = success
        return response

    def makeMountpoint(self, auth, requests):
        """ Create a new Mountpoint.
                makeMountpoint(requests)
                requests is dictionary with requestID as key and (Logical Name, metadata) as value        """
        # do traverse all the requests
        requests, traverse_response = self._traverse(requests)
        response = {}
        
        for rID, (LN, child_metadata, URL) in requests:
            # for each request first split the Logical Name
            rootguid, _, child_name = splitLN(LN)
            metadata, GUID, traversedLN, restLN, wasComplete, traversedlist = traverse_response[rID]
            #print metadata[('entry', 'type')]
            if metadata.get(('entry', 'type'), '') == 'mountpoint':
                success = 'cannot create anything in mountpoint'
                response[rID] = success
                return response

            log.msg(arc.VERBOSE, 'LN', LN, 'URL', URL, 'metadata', metadata, 'GUID', GUID, 'traversedLN', traversedLN, 'restLN', restLN, 'wasComplete',wasComplete, 'traversedlist', traversedlist)
            child_metadata[('entry','owner')] = auth.get_identity()
            child_metadata[('entry','type')] = 'mountpoint'
            child_metadata[('mountpoint','externalURL')] = URL 
            if wasComplete: # this means the LN exists
                success = 'LN exists'
            elif child_name == '': # this only can happen if the LN was a single GUID
                # this means the collection has no parent
                child_metadata[('entry','GUID')] = rootguid or global_root_guid
                success, _ = self._new(auth, child_metadata)
            elif restLN != child_name or GUID == '':
                success = 'parent does not exist'
            else:
                # if everything is OK, create the new collection
                #   here GUID is of the parent collection
                success, _ = self._new(auth, child_metadata, child_name, GUID, metadata)
            response[rID] = success
        return response

    ###   ###

    def _listLN(self, auth_request, LN, metadata, neededMetadata, dirname = ''):
        """ list one LN """
        decision = make_decision_metadata(metadata, auth_request)
        if decision != arc.DECISION_PERMIT:
            entries = {}
            status = 'denied'
        else:
            # let's get the type
            type = metadata[('entry', 'type')]
            if type == 'file': # files have no contents, we do not list them
                status = 'is a file'
                entries = {}
            elif type == 'mountpoint':
                url = metadata[('mountpoint', 'externalURL')]
                res = self._externalStore(auth, url, 'list')[url]
                status = res['status']
                entries = dict([(name, ('', {})) for name in res['list']])
            elif type == 'collection':
                # get all the properties and values from the 'entries' metadata section of the collection
                #   these are the names and GUIDs: the contents of the collection
                status = 'found'
                GUIDs = dict([(name, GUID)
                              for (section, name), GUID in metadata.items() if section == 'entries'])
                metadata = self.librarian.get(GUIDs.values(), neededMetadata)
                if dirname:
                    entries = dict([(os.path.join(LN.split(dirname+'/', 1)[-1], entry), (GUID, metadata[GUID]))
                                    for entry, GUID in GUIDs.items()])
                else:
                    entries = dict([(name, (GUID, metadata[GUID])) for name, GUID in GUIDs.items()])
            else:
                status = 'unknown'
                entries = {}
        return entries, status

    def _listRecursive(self, auth_request, LN, metadata, neededMetadata):
        """ recursive listing of collection """
        entries, status = self._listLN(auth_request, LN, metadata, neededMetadata)
        dirname = LN
        subentries = entries
        more_requests = True
        while more_requests:
            requests = {}
            for entry in subentries.keys():
                try:
                    if subentries[entry][1][('entry', 'type')] == 'collection': 
                        requests[entry] = [os.path.join(dirname, entry)]
                except:
                    log.msg(arc.WARNING, "Could not read entry", entry, "recursively")
            if requests:
                subentries = {}
                requests, traverse_response = self._traverse(requests)
                for requestID, [LN] in requests:
                    try:
                        # for each LN
                        metadata, GUID, traversedLN, restLN, wasComplete, traversedlist = traverse_response[requestID]
                        if wasComplete:
                            subsubentries, status = self._listLN(auth_request, LN, metadata, neededMetadata, dirname = dirname)
                            subentries = dict(subentries.items() + subsubentries.items())
                    except Exception, e:
                        status = 'internal error (%s)' % traceback.format_exc()
                        log.msg(arc.WARNING, status)
                entries = dict(entries.items()+subentries.items())
            else:
                # no more collections to recurse through
                more_requests = False
        return entries, status

    def list(self, auth, requests, neededMetadata = [], recursive = False):
        """ List the contents of a collection.
        
        list(requests, neededMetadata = [])
        
        requests is a dictionary with requestID as key and Logical Name as value
        neededMetadata is a list of (section, property) where property could be empty which means all properties of that section
            if neededMetadata is empty it means we need everything
        """
        auth_request = auth.get_request('read')
        #print 'ID: '+auth.get_identity()
        # do traverse the requested Logical Names
        requests, traverse_response = self._traverse(requests)
        response = {}
        for requestID, [LN] in requests:
            try:
                # for each LN
                metadata, GUID, traversedLN, restLN, wasComplete, traversedlist = traverse_response[requestID]
                #print 'metadata'
                #print metadata
                if wasComplete:
                    # this means that the LN was found
                    entries, status = recursive and self._listRecursive(auth_request, LN, metadata, neededMetadata) \
                                      or self._listLN(auth_request, LN, metadata, neededMetadata)
                elif metadata.get(('entry', 'type'), '') == 'mountpoint':
                    # todo: recursive listing of mountpoint
                    url = metadata[('mountpoint', 'externalURL')] + '/' + restLN
                    res = self._externalStore(auth, url, 'list')[url]
                    status = res['status']
                    entries = dict([(name, ('', {})) for name in res['list']])
                else:
                    entries = {}
                    status = 'not found'
            except Exception, e:
                entries = {}
                status = 'internal error (%s)' % traceback.format_exc()
            response[requestID] = (entries, status)
        return response

    def move(self, auth, requests):
        """ Move a file or collection within the global namespace.
        
        move(requests)
        
        requests is a dictionary with requestID as key and
            (sourceLN, targetLN, preserverOriginal) as value
        if preserverOriginal is true this method creates a hard link instead of moving
        """
        traverse_request = {}
        # create a traverse request, each move request needs two traversing: source and target
        for requestID, (sourceLN, targetLN, _) in requests.items():
            # from one requestID we create two: one for the source and one for the target
            traverse_request[requestID + 'source'] = remove_trailing_slash(sourceLN)
            traverse_request[requestID + 'target'] = targetLN
        traverse_response = self.librarian.traverseLN(traverse_request)
        log.msg(arc.VERBOSE, '\/\/', traverse_response)
        response = {}
        for requestID, (sourceLN, targetLN, preserveOriginal) in requests.items():
            sourceLN = remove_trailing_slash(sourceLN)
            # for each request
            log.msg(arc.VERBOSE, requestID, sourceLN, targetLN, preserveOriginal)
            # get the old and the new name of the entry, these are the last elements of the Logical Names
            _, _, old_child_name = splitLN(sourceLN)
            _, _, new_child_name = splitLN(targetLN)
            # get the GUID of the source LN from the traverse response
            _, sourceGUID, _, _, sourceWasComplete, sourceTraversedList \
                = traverse_response[requestID + 'source']
            # get the GUID form the target's traverse response, this should be the parent of the target LN
            targetMetadata, targetGUID, _, targetRestLN, targetWasComplete, targetTraversedList \
                = traverse_response[requestID + 'target']
            if targetMetadata.get(('entry', 'type'), '') == 'mountpoint':
                success = 'moving into a mountpoint is not supported'
                response[requestID] = success
                return response
            # if the source traverse was not complete: the source LN does not exist
            if not sourceWasComplete:
                success = 'nosuchLN'
            # if the target traverse was complete: the target LN already exists
            #   but if the new_child_name was empty, then the target is considered to be a collection, so it is OK
            elif targetWasComplete and new_child_name != '':
                success = 'targetexists'
            # if the target traverse was not complete, and the non-traversed part is not just the new name
            # (which means that the parent collection does not exist)
            # or the target's traversed list is empty (which means that it has no parent: it's just a single GUID)
            elif not targetWasComplete and (targetRestLN != new_child_name or len(targetTraversedList) == 0):
                success = 'invalidtarget'
            elif sourceGUID in [guid for (_, guid) in targetTraversedList]:
                # if the the target is within the source's subtree, we cannot move it
                success = 'invalidtarget'
            else:
                # if the new child name is empty that means that the target LN has a trailing slash
                #   so we just put the old name after it
                if new_child_name == '':
                    new_child_name = old_child_name
                decision = make_decision_metadata(targetMetadata, auth.get_request('addEntry'))
                if decision != arc.DECISION_PERMIT:
                    success = 'adding child to parent denied'
                else:
                    log.msg(arc.VERBOSE, 'adding', sourceGUID, 'to parent', targetGUID)
                    # adding the entry to the new parent
                    mm_resp = self.librarian.modifyMetadata(
                        {'move-target' : (targetGUID, 'add', 'entries', new_child_name, sourceGUID),
                        'move-target-parent' : (sourceGUID, 'set', 'parents', '%s/%s' % (targetGUID, new_child_name), 'parent'),
                        'move-target-closed?' : (targetGUID, 'setifvalue=yes', 'states', 'closed', 'broken')})
                    mm_succ = mm_resp['move-target']
                    if mm_succ != 'set':
                        success = 'failed adding child to parent'
                    else:
                        if preserveOriginal == true:
                            success = 'moved'
                        else:
                            # then we need to remove the source LN
                            # get the parent of the source: the source traverse has a list of the GUIDs of all the element along the path
                            source_parent_guid = sourceTraversedList[-2][1]
                            try:
                                source_parent_metadata = self.librarian.get([source_parent_guid])[source_parent_guid]
                                decision = make_decision_metadata(source_parent_metadata, auth.get_request('removeEntry'))
                                if decision == arc.DECISION_PERMIT:
                                    log.msg(arc.VERBOSE, 'removing', sourceGUID, 'from parent', source_parent_guid)
                                    # delete the entry from the source parent
                                    mm_resp = self.librarian.modifyMetadata(
                                        {'move-source' : (source_parent_guid, 'unset', 'entries', old_child_name, ''),
                                        'move-source-parent' : (sourceGUID, 'unset', 'parents', '%s/%s' % (source_parent_guid, old_child_name), ''),
                                        'move-source-closed?' : (source_parent_guid, 'setifvalue=yes', 'states', 'closed', 'broken')})
                                    mm_succ = mm_resp['move-source']
                                    if mm_succ != 'unset':
                                        success = 'failed to remove original link'
                                        # TODO: need some handling; remove the new entry or something
                                    else:
                                        success = 'moved'
                                else:
                                    success = 'denied to remove original link'
                            except:
                                success = 'error while removing original link'
            response[requestID] = success
        return response

    def modify(self, auth, requests):
        requests, traverse_response = self._traverse(requests)
        librarian_requests = {}
        not_found = []
        denied = []
        for changeID, (LN, changeType, section, property, value) in requests:
            metadata, GUID, _, _, wasComplete, _ = traverse_response[changeID]
            if wasComplete:
                if section == 'states':
                    decision = make_decision_metadata(metadata, auth.get_request('modifyStates'))
                    # TODO: do this instead with conditions in the Librarian
                    if property == 'closed':
                        current_state = metadata.get(('states','closed'),'no')
                        if value == 'no':
                            if current_state != 'no':
                                decision = arc.DECISION_DENY
                        elif value == 'yes':
                            if current_state not in ['no', 'yes']:
                                decision = arc.DECISION_DENY
                        else:
                            decision = arc.DECISION_DENY
                elif section == 'metadata':
                    decision = make_decision_metadata(metadata, auth.get_request('modifyMetadata'))
                elif section == 'policy':
                    decision = make_decision_metadata(metadata, auth.get_request('modifyPolicy'))
                else:
                    decision = arc.DECISION_DENY
                if decision == arc.DECISION_PERMIT:
                    librarian_requests[changeID] = (GUID, changeType, section, property, value)
                else:
                    denied.append(changeID)
            else:
                not_found.append(changeID)
        response = self.librarian.modifyMetadata(librarian_requests)
        for changeID in not_found:
            response[changeID] = 'no such LN'
        for changeID in denied:
            response[changeID] = 'denied'
        return response

from arcom.service import Service

class BartenderService(Service):

    def __init__(self, cfg):
        self.service_name = 'Bartender'
        #bar_request_names is the list of the names of the provided methods
        bar_request_names = ['stat','makeMountpoint','unmakeMountpoint', 'unmakeCollection', 'makeCollection', 'list', 'move', 'putFile', 'getFile', 'addReplica', 'delFile', 'modify', 'unlink', 'removeCredentials']
        # bartender_uri is the URI of the Bartender service namespace, and 'bar' is the prefix we want to use for this namespace
        bar_request_type = {'request_names' : bar_request_names,
            'namespace_prefix': 'bar', 'namespace_uri': bartender_uri} 
        # deleg_request_names contains the methods for delegation
        deleg_request_names = ['DelegateCredentialsInit', 'UpdateCredentials']
        # 'deleg' is the prefix for the delegation
        deleg_request_type = {'request_names' : deleg_request_names,
            'namespace_prefix': 'deleg', 'namespace_uri': 'http://www.nordugrid.org/schemas/delegation'}
        request_config = [deleg_request_type, bar_request_type]
        # call the Service's constructor
        Service.__init__(self, request_config, cfg, start_service = False)
        # get the path to proxy store
        self.proxy_store = str(cfg.Get('ProxyStore'))
        try:
            if not os.path.exists(self.proxy_store):
                os.mkdir(self.proxy_store)
            log.msg(arc.VERBOSE, 'Proxy store:', self.proxy_store)
        except:
            self.proxy_store = ''
        if not self.proxy_store:
            log.msg(arc.ERROR, 'The directory for storing proxies is not available. Proxy delegation disabled.')
        self.bartender = Bartender(cfg, self.ssl_config, self.state)
        
    def stat(self, inpayload):
        # incoming SOAP message example:
        #
        #   <bar:stat>
        #       <bar:statRequestList>
        #           <bar:statRequestElement>
        #               <bar:requestID>0</bar:requestID>
        #               <bar:LN>/</bar:LN>
        #           </bar:statRequestElement>
        #       </bar:statRequestList>
        #   </bar:stat>
        #
        # outgoing SOAP message example:
        #
        #   <bar:statResponse>
        #       <bar:statResponseList>
        #           <bar:statResponseElement>
        #              <bar:requestID>0</bar:requestID>
        #               <bar:metadataList>
        #                   <bar:metadata>
        #                       <bar:section>states</bar:section>
        #                       <bar:property>closed</bar:property>
        #                       <bar:value>0</bar:value>
        #                   </bar:metadata>
        #                   <bar:metadata>
        #                       <bar:section>entries</bar:section>
        #                       <bar:property>testfile</bar:property>
        #                       <bar:value>cf05727b-73f3-4318-8454-16eaf10f302c</bar:value>
        #                   </bar:metadata>
        #                   <bar:metadata>
        #                       <bar:section>entry</bar:section>
        #                       <bar:property>type</bar:property>
        #                       <bar:value>collection</bar:value>
        #                   </bar:metadata>
        #               </bar:metadataList>
        #           </bar:statResponseElement>
        #       </bar:statResponseList>
        #   </bar:statResponse>

        # get all the requests
        request_nodes = get_child_nodes(inpayload.Child().Child())
        # get the requestID and LN of each request and create a dictionary where the requestID is the key and the LN is the value
        requests = dict([(
            str(request_node.Get('requestID')),
            [str(request_node.Get('LN'))]
            ) for request_node in request_nodes
        ])
        # call the Bartender class
        response = self.bartender.stat(inpayload.auth, requests)
        # create the metadata XML structure of each request
        for requestID, metadata in response.items():
            response[requestID] = create_metadata(metadata, 'bar')
        # create the response message with the requestID and the metadata for each request
        return create_response('bar:stat',
            ['bar:requestID', 'bar:metadataList'], response, self._new_soap_payload(), single = True)

    def delFile(self, inpayload):
        # incoming SOAP message example:
        #
        #   <bar:delFile>
        #       <bar:delFileRequestList>
        #           <bar:delFileRequestElement>
        #               <bar:requestID>0</bar:requestID>
        #               <bar:LN>/</bar:LN>
        #           </bar:delFileRequestElement>
        #       </bar:delFileRequestList>
        #   </bar:delFile>
        #
        # outgoing SOAP message example:
        #
        #   <soap-env:Envelope>
        #       <soap-env:Body>
        #           <bar:delFileResponse>
        #               <bar:delFileResponseList>
        #                   <bar:delFileResponseElement>
        #                       <bar:requestID>0</bar:requestID>
        #                       <bar:success>deleted</bar:success>
        #                   </bar:delFileResponseElement>
        #               </bar:delFileResponseList>
        #           </bar:delFileResponse>
        #       </soap-env:Body>
        #   </soap-env:Envelope>

        request_nodes = get_child_nodes(inpayload.Child().Child())
        # get the requestID and LN of each request and create a dictionary where the requestID is the key and the LN is the value
        requests = dict([
            (str(request_node.Get('requestID')), str(request_node.Get('LN')))
                for request_node in request_nodes
        ])
        response = self.bartender.delFile(inpayload.auth, requests)
        return create_response('bar:delFile',
            ['bar:requestID', 'bar:success'], response, self._new_soap_payload(), single=True)


    def getFile(self, inpayload):
        # incoming SOAP message example:
        #
        #   <soap-env:Body>
        #       <bar:getFile>
        #           <bar:getFileRequestList>
        #               <bar:getFileRequestElement>
        #                   <bar:requestID>0</bar:requestID>
        #                   <bar:LN>/testfile</bar:LN>
        #                   <bar:protocol>byteio</bar:protocol>
        #               </bar:getFileRequestElement>
        #           </bar:getFileRequestList>
        #       </bar:getFile>
        #   </soap-env:Body>
        #
        # outgoing SOAP message example:
        #
        #   <soap-env:Envelope>
        #       <soap-env:Body>
        #           <bar:getFileResponse>
        #               <bar:getFileResponseList>
        #                   <bar:getFileResponseElement>
        #                       <bar:requestID>0</bar:requestID>
        #                       <bar:success>done</bar:success>
        #                       <bar:TURL>http://localhost:60000/byteio/12d86ba3-99d5-408e-91d1-35f7c47774e4</bar:TURL>
        #                       <bar:protocol>byteio</bar:protocol>
        #                   </bar:getFileResponseElement>
        #               </bar:getFileResponseList>
        #           </bar:getFileResponse>
        #       </soap-env:Body>
        #   </soap-env:Envelope>

        request_nodes = get_child_nodes(inpayload.Child().Child())
        requests = dict([
            (
                str(request_node.Get('requestID')), 
                ( # get the LN and all the protocols for each request and put them in a list
                    str(request_node.Get('LN')),
                    [str(node) for node in request_node.XPathLookup('//bar:protocol', self.ns)]
                )
            ) for request_node in request_nodes
        ])
        response = self.bartender.getFile(inpayload.auth, requests)
        return create_response('bar:getFile',
            ['bar:requestID', 'bar:success', 'bar:TURL', 'bar:protocol'], response, self._new_soap_payload())
    
    def addReplica(self, inpayload):
        # incoming SOAP message example:
        #
        #   <soap-env:Body>
        #       <bar:addReplica>
        #           <bar:addReplicaRequestList>
        #               <bar:putReplicaRequestElement>
        #                   <bar:requestID>0</bar:requestID>
        #                   <bar:GUID>cf05727b-73f3-4318-8454-16eaf10f302c</bar:GUID>
        #               </bar:putReplicaRequestElement>
        #           </bar:addReplicaRequestList>
        #           <bar:protocol>byteio</bar:protocol>
        #       </bar:addReplica>
        #   </soap-env:Body>
        #
        # outgoing SOAP message example:
        #
        #   <soap-env:Envelope>
        #       <soap-env:Body>
        #           <bar:addReplicaResponse>
        #               <bar:addReplicaResponseList>
        #                   <bar:addReplicaResponseElement>
        #                       <bar:requestID>0</bar:requestID>
        #                       <bar:success>done</bar:success>
        #                       <bar:TURL>http://localhost:60000/byteio/f568be18-26ae-4925-ae0c-68fe023ef1a5</bar:TURL>
        #                       <bar:protocol>byteio</bar:protocol>
        #                   </bar:addReplicaResponseElement>
        #               </bar:addReplicaResponseList>
        #           </bar:addReplicaResponse>
        #       </soap-env:Body>
        #   </soap-env:Envelope>
        
        # get the list of supported protocols
        protocols = [str(node) for node in inpayload.XPathLookup('//bar:protocol', self.ns)]
        # get the GUID of each request
        request_nodes = get_child_nodes(inpayload.Child().Get('addReplicaRequestList'))
        requests = dict([(str(request_node.Get('requestID')), str(request_node.Get('GUID')))
                for request_node in request_nodes])
        response = self.bartender.addReplica(inpayload.auth, requests, protocols)
        return create_response('bar:addReplica',
            ['bar:requestID', 'bar:success', 'bar:TURL', 'bar:protocol'], response, self._new_soap_payload())
    
    def putFile(self, inpayload):
        # incoming SOAP message example:
        #
        #   <soap-env:Body>
        #       <bar:putFile>
        #           <bar:putFileRequestList>
        #               <bar:putFileRequestElement>
        #                   <bar:requestID>0</bar:requestID>
        #                   <bar:LN>/testfile2</bar:LN>
        #                   <bar:metadataList>
        #                       <bar:metadata>
        #                           <bar:section>states</bar:section>
        #                           <bar:property>neededReplicas</bar:property>
        #                           <bar:value>2</bar:value>
        #                       </bar:metadata>
        #                       <bar:metadata>
        #                           <bar:section>states</bar:section>
        #                           <bar:property>size</bar:property>
        #                           <bar:value>11</bar:value>
        #                       </bar:metadata>
        #                   </bar:metadataList>
        #                   <bar:protocol>byteio</bar:protocol>
        #               </bar:putFileRequestElement>
        #           </bar:putFileRequestList>
        #       </bar:putFile>
        #   </soap-env:Body>
        #
        # outgoing SOAP message example:
        #
        #   <soap-env:Envelope>
        #       <soap-env:Body>
        #           <bar:putFileResponse>
        #               <bar:putFileResponseList>
        #                   <bar:putFileResponseElement>
        #                       <bar:requestID>0</bar:requestID>
        #                       <bar:success>done</bar:success>
        #                       <bar:TURL>http://localhost:60000/byteio/b8f2987b-a718-47b3-82bb-e838470b7e00</bar:TURL>
        #                       <bar:protocol>byteio</bar:protocol>
        #                   </bar:putFileResponseElement>
        #               </bar:putFileResponseList>
        #           </bar:putFileResponse>
        #       </soap-env:Body>
        #   </soap-env:Envelope>

        request_nodes = get_child_nodes(inpayload.Child().Child())
        requests = dict([
            (
                str(request_node.Get('requestID')), 
                (
                    str(request_node.Get('LN')),
                    parse_metadata(request_node.Get('metadataList')),
                    [str(node) for node in request_node.XPathLookup('//bar:protocol', self.ns)]
                )
            ) for request_node in request_nodes
        ])
        response = self.bartender.putFile(inpayload.auth, requests)
        return create_response('bar:putFile',
            ['bar:requestID', 'bar:success', 'bar:TURL', 'bar:protocol'], response, self._new_soap_payload())

    def unlink(self, inpayload):
        """docstring for unlink"""
        request_nodes = get_child_nodes(inpayload.Child().Child())
        requests = dict([(
                str(request_node.Get('requestID')),
                [str(request_node.Get('LN'))]
            ) for request_node in request_nodes
        ])
        response = self.bartender.unlink(inpayload.auth, requests)
        return create_response('bar:unlink',
            ['bar:requestID', 'bar:success'], response, self._new_soap_payload(), single = True)
    

    def unmakeCollection(self, inpayload):
        request_nodes = get_child_nodes(inpayload.Child().Child())
        requests = dict([(
                str(request_node.Get('requestID')),
                [str(request_node.Get('LN'))]
            ) for request_node in request_nodes
        ])
        response = self.bartender.unmakeCollection(inpayload.auth, requests)
        return create_response('bar:unmakeCollection',
            ['bar:requestID', 'bar:success'], response, self._new_soap_payload(), single = True)

    def makeCollection(self, inpayload):
        # incoming SOAP message example:
        # 
        #   <soap-env:Body>
        #       <bar:makeCollection>
        #           <bar:makeCollectionRequestList>
        #               <bar:makeCollectionRequestElement>
        #                   <bar:requestID>0</bar:requestID>
        #                   <bar:LN>/testdir</bar:LN>
        #                   <bar:metadataList>
        #                       <bar:metadata>
        #                           <bar:section>states</bar:section>
        #                           <bar:property>closed</bar:property>
        #                           <bar:value>no</bar:value>
        #                       </bar:metadata>
        #                   </bar:metadataList>
        #               </bar:makeCollectionRequestElement>
        #           </bar:makeCollectionRequestList>
        #       </bar:makeCollection>
        #   </soap-env:Body>
        #
        # outgoing SOAP message example
        #
        #   <soap-env:Envelope>
        #       <soap-env:Body>
        #           <bar:makeCollectionResponse>
        #               <bar:makeCollectionResponseList>
        #                   <bar:makeCollectionResponseElement>
        #                       <bar:requestID>0</bar:requestID>
        #                       <bar:success>done</bar:success>
        #                   </bar:makeCollectionResponseElement>
        #               </bar:makeCollectionResponseList>
        #           </bar:makeCollectionResponse>
        #       </soap-env:Body>
        #   </soap-env:Envelope>

        request_nodes = get_child_nodes(inpayload.Child().Child())
        requests = dict([
            (str(request_node.Get('requestID')), 
                (str(request_node.Get('LN')), parse_metadata(request_node.Get('metadataList')))
            ) for request_node in request_nodes
        ])
        response = self.bartender.makeCollection(inpayload.auth, requests)
        return create_response('bar:makeCollection',
            ['bar:requestID', 'bar:success'], response, self._new_soap_payload(), single = True)

    ### Created by Salman Toor. ###
    
    def unmakeMountpoint(self, inpayload):
        request_nodes = get_child_nodes(inpayload.Child().Child())
        requests = dict([(
                str(request_node.Get('requestID')),
                [str(request_node.Get('LN'))]
            ) for request_node in request_nodes
        ])
        response = self.bartender.unmakeMountpoint(inpayload.auth, requests)
        return create_response('bar:unmakeMountpoint',
            ['bar:requestID', 'bar:success'], response, self._new_soap_payload(), single = True)

    def makeMountpoint(self, inpayload):
        # incoming SOAP message example:
        # 
        #   <soap-env:Body>
        #       <bar:makeMountpoint>
        #           <bar:makeMountpointRequestList>
        #               <bar:makeMountpointRequestElement>
        #                   <bar:requestID>0</bar:requestID>
        #                   <bar:LN>/testdir</bar:LN>
        #                   <bar:URL>URL</bar:URL>
        #                   <bar:metadataList>
        #                       <bar:metadata>
        #                           <bar:section>any</bar:section>
        #                           <bar:property>additional</bar:property>
        #                           <bar:value>metadata</bar:value>
        #                       </bar:metadata>
        #                   </bar:metadataList>
        #               </bar:makeMountpointRequestElement>
        #           </bar:makeMountpointRequestList>
        #       </bar:makeMountpoint>
        #   </soap-env:Body>
        #
        # outgoing SOAP message example
        #
        #   <soap-env:Envelope>
        #       <soap-env:Body>
        #           <bar:makeMountpointResponse>
        #               <bar:makeMountpointResponseList>
        #                   <bar:makeMountpointResponseElement>
        #                       <bar:requestID>0</bar:requestID>
        #                       <bar:success>done</bar:success>
        #                   </bar:makeMountpointResponseElement>
        #               </bar:makeMountpointResponseList>
        #           </bar:makeMountpointResponse>
        #       </soap-env:Body>
        #   </soap-env:Envelope>

        request_nodes = get_child_nodes(inpayload.Child().Child())
        requests = dict([
            (str(request_node.Get('requestID')),
                (str(request_node.Get('LN')), parse_metadata(request_node.Get('metadataList')), str(request_node.Get('URL')))
            ) for request_node in request_nodes
        ])
        response = self.bartender.makeMountpoint(inpayload.auth, requests)
        return create_response('bar:makeMountpoint',
            ['bar:requestID', 'bar:success'], response, self._new_soap_payload(), single = True)
        
    ###     ####        

    def list(self, inpayload):
        # incoming SOAP message example:
        #
        #   <soap-env:Body>
        #       <bar:list>
        #           <bar:listRequestList>
        #               <bar:listRequestElement>
        #                   <bar:requestID>0</bar:requestID>
        #                   <bar:LN>/</bar:LN>
        #               </bar:listRequestElement>
        #           </bar:listRequestList>
        #           <bar:neededMetadataList>
        #               <bar:neededMetadataElement>
        #                   <bar:section>entry</bar:section>
        #                   <bar:property></bar:property>
        #               </bar:neededMetadataElement>
        #           </bar:neededMetadataList>
        #       </bar:list>
        #   </soap-env:Body>
        #
        # outgoing SOAP message example:
        #
        #   <soap-env:Envelope>
        #       <soap-env:Body>
        #           <bar:listResponse>
        #               <bar:listResponseList>
        #                   <bar:listResponseElement>
        #                       <bar:requestID>0</bar:requestID>
        #                       <bar:entries>
        #                           <bar:entry>
        #                               <bar:name>testfile</bar:name>
        #                               <bar:GUID>cf05727b-73f3-4318-8454-16eaf10f302c</bar:GUID>
        #                               <bar:metadataList>
        #                                   <bar:metadata>
        #                                       <bar:section>entry</bar:section>
        #                                       <bar:property>type</bar:property>
        #                                       <bar:value>file</bar:value>
        #                                   </bar:metadata>
        #                               </bar:metadataList>
        #                           </bar:entry>
        #                           <bar:entry>
        #                               <bar:name>testdir</bar:name>
        #                               <bar:GUID>4cabc8cb-599d-488c-a253-165f71d4e180</bar:GUID>
        #                               <bar:metadataList>
        #                                   <bar:metadata>
        #                                       <bar:section>entry</bar:section>
        #                                       <bar:property>type</bar:property>
        #                                       <bar:value>collection</bar:value>
        #                                   </bar:metadata>
        #                               </bar:metadataList>
        #                           </bar:entry>
        #                       </bar:entries>
        #                       <bar:status>found</bar:status>
        #                   </bar:listResponseElement>
        #               </bar:listResponseList>
        #           </bar:listResponse>
        #       </soap-env:Body>
        #   </soap-env:Envelope>
        
        requests = parse_node(inpayload.Child().Get('listRequestList'),
            ['requestID', 'LN'], single = False)
        neededMetadata = [
            node_to_data(node, ['section', 'property'], single = True)
                for node in get_child_nodes(inpayload.Child().Get('neededMetadataList'))
        ]
        recursive = ['yes'] in parse_node(inpayload.Child().Get('listRecursive'), ['requestID', 'doRecursive'], single = False).values()
        response0 = self.bartender.list(inpayload.auth, requests, neededMetadata, recursive)
        response = dict([
            (requestID,
            ([('bar:entry', [
                ('bar:name', name),
                ('bar:GUID', GUID),
                ('bar:metadataList', create_metadata(metadata))
            ]) for name, (GUID, metadata) in entries.items()],
            status)
        ) for requestID, (entries, status) in response0.items()])
        return create_response('bar:list',
            ['bar:requestID', 'bar:entries', 'bar:status'], response, self._new_soap_payload())

    def move(self, inpayload):
        # incoming SOAP message example:
        #   <soap-env:Body>
        #       <bar:move>
        #           <bar:moveRequestList>
        #               <bar:moveRequestElement>
        #                   <bar:requestID>0</bar:requestID>
        #                   <bar:sourceLN>/testfile2</bar:sourceLN>
        #                   <bar:targetLN>/testdir/</bar:targetLN>
        #                   <bar:preserveOriginal>0</bar:preserveOriginal>
        #               </bar:moveRequestElement>
        #           </bar:moveRequestList>
        #       </bar:move>
        #   </soap-env:Body>
        #
        # outgoing SOAP message example:
        #
        #   <soap-env:Envelope>
        #       <soap-env:Body>
        #           <bar:moveResponse>
        #               <bar:moveResponseList>
        #                   <bar:moveResponseElement>
        #                       <bar:requestID>0</bar:requestID>
        #                       <bar:status>moved</bar:status>
        #                   </bar:moveResponseElement>
        #               </bar:moveResponseList>
        #           </bar:moveResponse>
        #       </soap-env:Body>
        #   </soap-env:Envelope>

        requests = parse_node(inpayload.Child().Child(),
            ['requestID', 'sourceLN', 'targetLN', 'preserveOriginal'])
        response = self.bartender.move(inpayload.auth, requests)
        return create_response('bar:move',
            ['bar:requestID', 'bar:status'], response, self._new_soap_payload(), single = True)

    def modify(self, inpayload):
        requests = parse_node(inpayload.Child().Child(), ['bar:changeID',
            'bar:LN', 'bar:changeType', 'bar:section', 'bar:property', 'bar:value'])
        response = self.bartender.modify(inpayload.auth, requests)
        return create_response('bar:modify', ['bar:changeID', 'bar:success'],
            response, self._new_soap_payload(), single = True)

    def DelegateCredentialsInit(self,inpayload):
        ns = arc.NS('delegation','http://www.nordugrid.org/schemas/delegation')
        outpayload = arc.PayloadSOAP(ns)
        if self.proxy_store:
            #print inpayload.GetXML()
            # Delegation Credentials(NEED TO FIX IT)  
            self.delegSOAP = arc.DelegationContainerSOAP()
            self.delegSOAP.DelegateCredentialsInit(inpayload,outpayload)
            #print "\n outpayload"
            #print outpayload.GetXML()
        return outpayload
        
    def UpdateCredentials(self,inpayload):
        #print inpayload.GetXML()
        ns = arc.NS('delegation','http://www.nordugrid.org/schemas/delegation')
        outpayload = arc.PayloadSOAP(ns)
        credAndid = self.delegSOAP.UpdateCredentials(inpayload,outpayload)
        #print credAndid
        if credAndid[0] == True:
            #print "\n ---Delegated Credentials--- "
            #print credAndid[1]
            log.msg(arc.INFO,'Delegation status: ', credAndid[0])
            if (os.path.isdir(self.proxy_store)):
                #print "ProxyStore: "+self.proxy_store 
                filePath = self.proxy_store+'/'+base64.b64encode(inpayload.auth.get_identity())+'.proxy'
                log.msg(arc.VERBOSE,'creating proxy file : ', filePath)
                proxyfile = open(filePath, 'w') 
                proxyfile.write(credAndid[1])
                proxyfile.close()
                log.msg(arc.VERBOSE,'created successfully, ID: %s',credAndid[2]) 
                os.system('chmod 600 '+filePath)
            else:
                log.msg(arc.VERBOSE,'cannot access proxy_store, Check the configuration file (service.xml)\n Need to have a <ProxyStore>')
        else:
            log.msg(arc.INFO,'Delegation failed: ')
        return outpayload
        
    def removeCredentials(self, inpayload):
        response = {}
        if self.proxy_store:
            log.msg(arc.INFO, 'ID: '+inpayload.auth.get_identity())
            message = ''
            if (os.path.isdir(self.proxy_store)):
                log.msg(arc.VERBOSE, 'ProxyStore: %s',self.proxy_store)
                filePath = self.proxy_store+'/'+base64.b64encode(inpayload.auth.get_identity())+'.proxy'
                if (os.path.isfile(filePath)):    
                    os.system('rm '+filePath)
                    message = 'Credential removed successfully'
                    status = 'successful' 
                else:
                    message =  'cannot access proxy file: '+filePath
                    status = 'failed'
            else:
                message = 'cannot access proxy_store, Check the configuration file (service.xml)\n Need to have a <ProxyStore>'
                status = 'failed'
        else: 
            message = 'cannot access proxy_store, Check the configuration file (service.xml)\n Need to have a <ProxyStore>'                
            status = 'failed'
        #print message
        response['message'] = message
        response['status'] = status
        log.msg(arc.VERBOSE, 'removeCredentials: %s', message)
        log.msg(arc.VERBOSE, 'removeCredentials: %s', status)  
        return create_response('bar:removeCredentials', ['bar:message','bar:status'],response, self._new_soap_payload(), single = True) 

    def RegistrationCollector(self, doc):
        regentry = arc.XMLNode('<RegEntry />')
        regentry.NewChild('SrcAdv').NewChild('Type').Set(bartender_servicetype)
        #Place the document into the doc attribute
        doc.Replace(regentry)
        return True

    def GetAdditionalLocalInformation(self, service_node):
        service_node.NewChild('Type').Set(bartender_servicetype)
