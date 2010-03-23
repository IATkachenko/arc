#include <glib.h>

#include "SRM22Client.h"

//namespace Arc {
  
  //Logger SRM22Client::logger(SRMClient::logger, "SRM22Client");

  SRM22Client::SRM22Client(SRMURL url) {
    version = "v2.2";
    implementation = SRM_IMPLEMENTATION_UNKNOWN;
    service_endpoint = url.ContactURL();
    csoap = new Arc::HTTPSClientSOAP(service_endpoint.c_str(),
                                     &soapobj,
                                     url.GSSAPI(),
                                     request_timeout,
                                     false);
    if(!csoap) { csoap=NULL; return; };
    if(!*csoap) { delete csoap; csoap=NULL; return; };
    soapobj.namespaces=srm2_2_soap_namespaces;
  }
  
  SRM22Client::~SRM22Client(void) {
    if(csoap) { csoap->disconnect(); delete csoap; };
  }
  
  
  static const char* Supported_Protocols[] = {
    "gsiftp","https","httpg","http","ftp","se"
  };
  
  static const int size_of_supported_protocols = 6;
  
  
  SRMReturnCode SRM22Client::ping(std::string& version, bool report_error) {
  
    SRMReturnCode rc = connect();
    if (rc != SRM_OK) return rc;
  
    SRMv2__srmPingRequest * request = new SRMv2__srmPingRequest;
    struct SRMv2__srmPingResponse_ response_struct;
    
    // do the call
    int soap_err = SOAP_OK;
    if((soap_err=soap_call_SRMv2__srmPing(&soapobj, csoap->SOAP_URL(), "srmPing", request, response_struct)) != SOAP_OK){
      logger.msg(Arc::VERBOSE, "SOAP request failed (%s)", "srmPing");
      if(report_error) soap_print_fault(&soapobj, stderr);
      csoap->disconnect();
      return SRM_ERROR_SOAP;
    };
  
    // get the version info
    if (response_struct.srmPingResponse->versionInfo) {
      version = response_struct.srmPingResponse->versionInfo;
      logger.msg(Arc::VERBOSE, "Server SRM version: %s", version);
  
      // get the implementation
      if (response_struct.srmPingResponse->otherInfo) {
        // look through otherInfo for the backend_type
        for(int i=0; i<response_struct.srmPingResponse->otherInfo->__sizeextraInfoArray; i++) {
          SRMv2__TExtraInfo * extrainfo = response_struct.srmPingResponse->otherInfo->extraInfoArray[i];
          if(strcmp((char*)extrainfo->key, "backend_type") != 0) continue;
          if(strcmp((char*)extrainfo->value, "dCache") == 0) {
            implementation = SRM_IMPLEMENTATION_DCACHE;
            logger.msg(Arc::VERBOSE, "Server implementation: %s", "dCache");
          }
          else if(strcmp((char*)extrainfo->value, "CASTOR") == 0) {
            implementation = SRM_IMPLEMENTATION_CASTOR;
            logger.msg(Arc::VERBOSE, "Server implementation: %s", "CASTOR");
          }
          else if(strcmp((char*)extrainfo->value, "DPM") == 0) {
            implementation = SRM_IMPLEMENTATION_DPM;
            logger.msg(Arc::VERBOSE, "Server implementation: %s", "DPM");
          }
          else if(strcmp((char*)extrainfo->value, "StoRM") == 0) {
            implementation = SRM_IMPLEMENTATION_STORM;
            logger.msg(Arc::VERBOSE, "Server implementation: %s", "StoRM");
          };
        };
      };
      return SRM_OK;
    };
    logger.msg(Arc::ERROR, "Could not determine version of server");
    return SRM_ERROR_OTHER;
  };
  
  SRMReturnCode SRM22Client::getSpaceTokens(std::list<std::string>& tokens,
                                            std::string description) {
  
    SRMReturnCode rc = connect();
    if (rc != SRM_OK) return rc;
  
    SRMv2__srmGetSpaceTokensRequest * request = new SRMv2__srmGetSpaceTokensRequest;
    if(description.compare("") != 0) request->userSpaceTokenDescription = (char*)description.c_str();
    struct SRMv2__srmGetSpaceTokensResponse_ response_struct;
  
    // do the call
    int soap_err = SOAP_OK;
    if((soap_err=soap_call_SRMv2__srmGetSpaceTokens(&soapobj, csoap->SOAP_URL(), "srmGetSpaceTokens", request, response_struct)) != SOAP_OK){
      logger.msg(Arc::INFO, "SOAP request failed (%s)", "srmGetSpaceTokens");
      soap_print_fault(&soapobj, stderr);
      csoap->disconnect();
      return SRM_ERROR_SOAP;
    };
  
    SRMv2__srmGetSpaceTokensResponse * response_inst = response_struct.srmGetSpaceTokensResponse;
  
    if (response_inst->returnStatus->statusCode != SRMv2__TStatusCode__SRM_USCORESUCCESS) {
      char * msg = response_inst->returnStatus->explanation;
      logger.msg(Arc::ERROR, "Error: %s", msg);
      return SRM_ERROR_OTHER;
    };
  
    for(int i = 0; i < response_inst->arrayOfSpaceTokens->__sizestringArray; i++) {
  
      std::string token(response_inst->arrayOfSpaceTokens->stringArray[i]);
      logger.msg(Arc::VERBOSE, "Adding space token %s", token);
      tokens.push_back(token);
    };
  
    return SRM_OK;
  };
  
  
  SRMReturnCode SRM22Client::getRequestTokens(std::list<std::string>& tokens,
                                              std::string description) {
  
    SRMReturnCode rc = connect();
    if (rc != SRM_OK) return rc;
  
    SRMv2__srmGetRequestTokensRequest * request = new SRMv2__srmGetRequestTokensRequest;
    if(description.compare("") != 0) request->userRequestDescription = (char*)description.c_str();
  
    struct SRMv2__srmGetRequestTokensResponse_ response_struct;
  
    // do the call
    int soap_err = SOAP_OK;
    if((soap_err=soap_call_SRMv2__srmGetRequestTokens(&soapobj, csoap->SOAP_URL(), "srmGetRequestTokens", request, response_struct)) != SOAP_OK){
      logger.msg(Arc::INFO, "SOAP request failed (%s)", "srmGetRequestTokens");
      soap_print_fault(&soapobj, stderr);
      csoap->disconnect();
      return SRM_ERROR_SOAP;
    };
  
    SRMv2__srmGetRequestTokensResponse * response_inst = response_struct.srmGetRequestTokensResponse;
  
    if (response_inst->returnStatus->statusCode == SRMv2__TStatusCode__SRM_USCOREINVALID_USCOREREQUEST) {
      // no tokens found
      logger.msg(Arc::INFO, "No request tokens found");
      return SRM_OK;
    }
    else if (response_inst->returnStatus->statusCode != SRMv2__TStatusCode__SRM_USCORESUCCESS) {
      char * msg = response_inst->returnStatus->explanation;
      logger.msg(Arc::ERROR, "Error: %s", msg);
      return SRM_ERROR_OTHER;
    };
  
    for(int i = 0; i < response_inst->arrayOfRequestTokens->__sizetokenArray; i++) {
  
      std::string token(response_inst->arrayOfRequestTokens->tokenArray[i]->requestToken);
      logger.msg(Arc::VERBOSE, "Adding request token %s", token);
      tokens.push_back(token);
    };
  
    return SRM_OK;
  };
  
  
  SRMReturnCode SRM22Client::getTURLs(SRMClientRequest& req,
                                      std::list<std::string>& urls) {
    SRMReturnCode rc = connect();
    if (rc != SRM_OK) return rc;
  
    // call get
    
    // construct get request - only one file requested at a time
    SRMv2__TGetFileRequest * req_array = new SRMv2__TGetFileRequest[1];
  
    SRMv2__TGetFileRequest * r = new SRMv2__TGetFileRequest;
    r->sourceSURL=(char*)req.surls().front().c_str();
  
    req_array[0] = *r;
  
    SRMv2__ArrayOfTGetFileRequest * file_requests = new SRMv2__ArrayOfTGetFileRequest;
    file_requests->__sizerequestArray=1;
    file_requests->requestArray=&req_array;
  
    // transfer parameters with protocols
    SRMv2__TTransferParameters * transfer_params = new SRMv2__TTransferParameters;
    SRMv2__ArrayOfString * prot_array = new SRMv2__ArrayOfString;
    prot_array->__sizestringArray=size_of_supported_protocols;
    prot_array->stringArray=(char**)Supported_Protocols;
    transfer_params->arrayOfTransferProtocols=prot_array;
    
    SRMv2__srmPrepareToGetRequest * request = new SRMv2__srmPrepareToGetRequest;
    request->transferParameters=transfer_params;
    request->arrayOfFileRequests=file_requests;
  
    struct SRMv2__srmPrepareToGetResponse_ response_struct;
  
    // do the call
    int soap_err = SOAP_OK;
    if((soap_err=soap_call_SRMv2__srmPrepareToGet(&soapobj, csoap->SOAP_URL(), "srmPrepareToGet", request, response_struct)) != SOAP_OK){
      logger.msg(Arc::INFO, "SOAP request failed (%s)", "srmPrepareToGet");
      soap_print_fault(&soapobj, stderr);
      csoap->disconnect();
      delete[] req_array;
      return SRM_ERROR_SOAP;
    };
  
    delete[] req_array;
    SRMv2__srmPrepareToGetResponse * response_inst = response_struct.srmPrepareToGetResponse;
    SRMv2__TStatusCode return_status = response_inst->returnStatus->statusCode;
    SRMv2__ArrayOfTGetRequestFileStatus * file_statuses= response_inst->arrayOfFileStatuses;
  
    // store the request token in the request object
    if (response_inst->requestToken) req.request_token(response_inst->requestToken);

    // deal with response code
    if (return_status == SRMv2__TStatusCode__SRM_USCOREREQUEST_USCOREQUEUED ||
        return_status == SRMv2__TStatusCode__SRM_USCOREREQUEST_USCOREINPROGRESS) {
      // file is queued - need to wait and query with returned request token
      char * request_token = response_inst->requestToken;
  
      int sleeptime = 1;
      if (response_inst->arrayOfFileStatuses->statusArray[0]->estimatedWaitTime)
        sleeptime = *(response_inst->arrayOfFileStatuses->statusArray[0]->estimatedWaitTime);
      int request_time = 0;
  
      while (return_status != SRMv2__TStatusCode__SRM_USCORESUCCESS) {
  
        // sleep for recommended time (within limits)
        sleeptime = sleeptime<1?1:sleeptime;
        sleeptime = sleeptime>request_timeout ? request_timeout-request_time : sleeptime;
        logger.msg(Arc::VERBOSE, "%s: File request %s in SRM queue. Sleeping for %i seconds", req.surls().front(), request_token, sleeptime);
        sleep(sleeptime);
        request_time += sleeptime;
  
        SRMv2__srmStatusOfGetRequestRequest * sog_request = new SRMv2__srmStatusOfGetRequestRequest;
        sog_request->requestToken=request_token;
  
        struct SRMv2__srmStatusOfGetRequestResponse_ sog_response_struct;
  
        // call getRequestStatus
        if ((soap_err=soap_call_SRMv2__srmStatusOfGetRequest(&soapobj, csoap->SOAP_URL(), "srmStatusOfGetRequest", sog_request, sog_response_struct)) != SOAP_OK) {
          logger.msg(Arc::INFO, "SOAP request failed (%s)", "srmStatusOfGetRequest");
          soap_print_fault(&soapobj, stderr);
          csoap->disconnect();
          req.finished_abort();  
          return SRM_ERROR_SOAP;
        };
  
        // check return codes - loop will exit on success or return false on error
  
        return_status = sog_response_struct.srmStatusOfGetRequestResponse->returnStatus->statusCode;
        file_statuses = sog_response_struct.srmStatusOfGetRequestResponse->arrayOfFileStatuses;
        
        if (return_status == SRMv2__TStatusCode__SRM_USCOREREQUEST_USCOREQUEUED ||
            return_status == SRMv2__TStatusCode__SRM_USCOREREQUEST_USCOREINPROGRESS) {
          // still queued - keep waiting
          // check for timeout
          if (request_time >= request_timeout) {
            logger.msg(Arc::ERROR, "Error: PrepareToGet request timed out after %i seconds", request_timeout);
            req.finished_abort();
            return SRM_ERROR_TEMPORARY;
          }
          if(file_statuses->statusArray[0]->estimatedWaitTime)
            sleeptime = *(file_statuses->statusArray[0]->estimatedWaitTime);
        }
        else if (return_status != SRMv2__TStatusCode__SRM_USCORESUCCESS) {
          // error
          char * msg = sog_response_struct.srmStatusOfGetRequestResponse->returnStatus->explanation;
          logger.msg(Arc::ERROR, "Error: %s", msg);
          if (return_status == SRMv2__TStatusCode__SRM_USCOREINTERNAL_USCOREERROR)
            return SRM_ERROR_TEMPORARY;
          return SRM_ERROR_PERMANENT;
        };
      }; // while
  
    } // if file queued
  
    else if (return_status != SRMv2__TStatusCode__SRM_USCORESUCCESS) {
      // any other return code is a failure
      char * msg = response_inst->returnStatus->explanation;
      logger.msg(Arc::ERROR, "Error: %s", msg);
      if (return_status == SRMv2__TStatusCode__SRM_USCOREINTERNAL_USCOREERROR)
        return SRM_ERROR_TEMPORARY;
      return SRM_ERROR_PERMANENT;
    };
  
    // the file is ready and pinned - we can get the TURL
    char * turl = file_statuses->statusArray[0]->transferURL;
  
    logger.msg(Arc::VERBOSE, "File is ready! TURL is %s", turl);
    urls.push_back(std::string(turl));

    req.finished_success();
    return SRM_OK;
  }
  
  SRMReturnCode SRM22Client::requestBringOnline(SRMClientRequest& req) {
  
    SRMReturnCode rc = connect();
    if (rc != SRM_OK) return rc;
  
    // construct bring online request
    std::list<std::string> surls = req.surls();
    SRMv2__TGetFileRequest ** req_array = new SRMv2__TGetFileRequest*[surls.size()];
    
    // add each file to the request array
    int j = 0;
    for (std::list<std::string>::iterator i = surls.begin(); i != surls.end(); ++i) {
      SRMv2__TGetFileRequest * r = new SRMv2__TGetFileRequest;
      r->sourceSURL=(char*)(*i).c_str();
      req_array[j] = r;
      j++;
    };
  
    SRMv2__ArrayOfTGetFileRequest * file_requests = new SRMv2__ArrayOfTGetFileRequest;
    file_requests->__sizerequestArray=surls.size();
    file_requests->requestArray=req_array;
  
    // transfer parameters with protocols
    // should not be needed but dcache returns NullPointerException if
    // it is not given
    SRMv2__TTransferParameters * transfer_params = new SRMv2__TTransferParameters;
    SRMv2__ArrayOfString * prot_array = new SRMv2__ArrayOfString;
    prot_array->__sizestringArray=size_of_supported_protocols;
    prot_array->stringArray=(char**)Supported_Protocols;
    transfer_params->arrayOfTransferProtocols=prot_array;
    
    SRMv2__srmBringOnlineRequest * request = new SRMv2__srmBringOnlineRequest;
    request->arrayOfFileRequests=file_requests;
    request->transferParameters=transfer_params;
  
    // store the user id as part of the request, so they can find it later
    char * user = const_cast<char*>(g_get_user_name());
    if (user) {
      logger.msg(Arc::VERBOSE, "Setting userRequestDescription to %s", user);
      request->userRequestDescription = user;
    };
  
    struct SRMv2__srmBringOnlineResponse_ response_struct;
    
    // do the call
    int soap_err = SOAP_OK;
    if((soap_err=soap_call_SRMv2__srmBringOnline(&soapobj, csoap->SOAP_URL(), "srmBringOnline", request, response_struct)) != SOAP_OK){
      logger.msg(Arc::INFO, "SOAP request failed (%s)", "srmBringOnline");
      soap_print_fault(&soapobj, stderr);
      csoap->disconnect();
      return SRM_ERROR_SOAP;
    };
  
    SRMv2__srmBringOnlineResponse * response_inst = response_struct.srmBringOnlineResponse;
    SRMv2__TStatusCode return_status = response_inst->returnStatus->statusCode;
    SRMv2__ArrayOfTBringOnlineRequestFileStatus * file_statuses= response_inst->arrayOfFileStatuses;
  
    // store the request token in the request object
    if (response_inst->requestToken) req.request_token(response_inst->requestToken);
  
    // deal with response code - successful ones first
    if (return_status == SRMv2__TStatusCode__SRM_USCORESUCCESS) {
      // this means files are all already online
      for (std::list<std::string>::iterator i = surls.begin();
           i != surls.end();
           ++i) {
        req.surl_statuses(*i, SRM_ONLINE);
        req.finished_success();
      }
      return SRM_OK;
    };
  
    if (return_status == SRMv2__TStatusCode__SRM_USCOREREQUEST_USCOREQUEUED) {
      // all files have been queued - leave statuses as unknown
      return SRM_OK;
    };
  
    if (return_status == SRMv2__TStatusCode__SRM_USCOREREQUEST_USCOREINPROGRESS) {
      // some files have been queued and some are online. check each file
      fileStatus(req, file_statuses);
      return SRM_OK;
    };
  
    if (return_status == SRMv2__TStatusCode__SRM_USCOREPARTIAL_USCORESUCCESS) {
      // some files are already online, some failed. check each file
      fileStatus(req, file_statuses);
      return SRM_OK;
    };
  
    // here means an error code was returned and all files failed
    char * msg = response_inst->returnStatus->explanation;
    logger.msg(Arc::ERROR, "Error: %s", msg);
    req.finished_error();
    if (return_status == SRMv2__TStatusCode__SRM_USCOREINTERNAL_USCOREERROR)
      return SRM_ERROR_TEMPORARY;
    return SRM_ERROR_PERMANENT;  
  };
  
  
  SRMReturnCode SRM22Client::requestBringOnlineStatus(SRMClientRequest& req) {
  
    SRMReturnCode rc = connect();
    if (rc != SRM_OK) return rc;
  
    SRMv2__srmStatusOfBringOnlineRequestRequest * sobo_request = new SRMv2__srmStatusOfBringOnlineRequestRequest;
    if(req.request_token().empty()) {
      logger.msg(Arc::ERROR, "No request token specified!");
      return SRM_ERROR_OTHER;
    };
    sobo_request->requestToken=(char*)req.request_token().c_str();
  
    struct SRMv2__srmStatusOfBringOnlineRequestResponse_ sobo_response_struct;
  
    // do the call
    int soap_err = SOAP_OK;
    if ((soap_err=soap_call_SRMv2__srmStatusOfBringOnlineRequest(&soapobj, csoap->SOAP_URL(), "srmStatusOfBringOnlineRequest", sobo_request, sobo_response_struct)) != SOAP_OK) {
      logger.msg(Arc::INFO, "SOAP request failed (%s)", "srmStatusOfBringOnlineRequest");
      soap_print_fault(&soapobj, stderr);
      csoap->disconnect();
      return SRM_ERROR_SOAP;
    };
  
    SRMv2__TStatusCode return_status = sobo_response_struct.srmStatusOfBringOnlineRequestResponse->returnStatus->statusCode;
    SRMv2__ArrayOfTBringOnlineRequestFileStatus * file_statuses = sobo_response_struct.srmStatusOfBringOnlineRequestResponse->arrayOfFileStatuses;
  
    // deal with response code - successful ones first
    if (return_status == SRMv2__TStatusCode__SRM_USCORESUCCESS) {
      // this means files are all online
      fileStatus(req, file_statuses);    
      req.finished_success();
      return SRM_OK;
    };
  
    if (return_status == SRMv2__TStatusCode__SRM_USCOREREQUEST_USCOREQUEUED) {
      // all files are in the queue - leave statuses as they are
      return SRM_OK;
    };
  
    if (return_status == SRMv2__TStatusCode__SRM_USCOREREQUEST_USCOREINPROGRESS) {
      // some files have been queued and some are online. check each file
      fileStatus(req, file_statuses);    
      return SRM_OK;
    };
  
    if (return_status == SRMv2__TStatusCode__SRM_USCOREPARTIAL_USCORESUCCESS) {
      // some files are online, some failed. check each file
      fileStatus(req, file_statuses);
      req.finished_partial_success();
      return SRM_OK;
    };
  
    if (return_status == SRMv2__TStatusCode__SRM_USCOREABORTED) {
      // The request was aborted or finished successfully. dCache reports
      // SRM_ABORTED after the first time a successful request is queried
      // so we have to look at the explanation string for the real reason.
      std::string explanation(sobo_response_struct.srmStatusOfBringOnlineRequestResponse->returnStatus->explanation);
      if(explanation.find("All files are done") != std::string::npos) {
        logger.msg(Arc::VERBOSE, "Request is reported as ABORTED, but all files are done");
        req.finished_success();
        return SRM_OK;
      }
      else if(explanation.find("Canceled") != std::string::npos) {
        logger.msg(Arc::VERBOSE, "Request is reported as ABORTED, since it was cancelled");
        req.cancelled();
        return SRM_OK;
      }
      else if(explanation.length() != 0){
        logger.msg(Arc::VERBOSE, "Request is reported as ABORTED. Reason: %s", explanation);
        req.finished_error();
        return SRM_ERROR_PERMANENT;
      }
      else {
        logger.msg(Arc::VERBOSE, "Request is reported as ABORTED");
        req.finished_error();
        return SRM_ERROR_PERMANENT;
      }
      
    };
  
    // here means an error code was returned and all files failed
    // return error, but may be retryable by client
    char * msg = sobo_response_struct.srmStatusOfBringOnlineRequestResponse->returnStatus->explanation;
    logger.msg(Arc::ERROR, "Error: %s", msg);
    if(file_statuses) fileStatus(req, file_statuses);
    req.finished_error();
    if (return_status == SRMv2__TStatusCode__SRM_USCOREINTERNAL_USCOREERROR)
      return SRM_ERROR_TEMPORARY;
    return SRM_ERROR_PERMANENT;
  };
  
  void SRM22Client::fileStatus(SRMClientRequest& req,
                               SRMv2__ArrayOfTBringOnlineRequestFileStatus * file_statuses) {
  
    int wait_time = 0;
  
    for (int i=0; i<file_statuses->__sizestatusArray; i++) {
        
      SRMv2__TReturnStatus * file_status = file_statuses->statusArray[i]->status;
      char * surl = file_statuses->statusArray[i]->sourceSURL;
      
      // store the largest estimated waiting time
      if (file_statuses->statusArray[i]->estimatedWaitTime &&
          *(file_statuses->statusArray[i]->estimatedWaitTime) > wait_time)
        wait_time = *(file_statuses->statusArray[i]->estimatedWaitTime);
      
      if (file_status->statusCode == SRMv2__TStatusCode__SRM_USCORESUCCESS ||
          file_status->statusCode == SRMv2__TStatusCode__SRM_USCOREFILE_USCOREIN_USCORECACHE) {
        // file is online
        req.surl_statuses(surl, SRM_ONLINE);
      }
        
      else if (file_status->statusCode == SRMv2__TStatusCode__SRM_USCOREREQUEST_USCOREQUEUED ||
               file_status->statusCode == SRMv2__TStatusCode__SRM_USCOREREQUEST_USCOREINPROGRESS) {
        // in queue to be staged
        req.surl_statuses(surl, SRM_NEARLINE);
      }
      else {
        // error 
        req.surl_statuses(surl, SRM_STAGE_ERROR);
        if (file_status->explanation) req.surl_failures(surl, file_status->explanation);
        else req.surl_failures(surl, "No reason available");
      };
    };
    req.waiting_time(wait_time);
  };
  
  
  SRMReturnCode SRM22Client::putTURLs(SRMClientRequest& req,
                                      std::list<std::string>& urls,
                                      unsigned long long size) {
    SRMReturnCode rc = connect();
    if (rc != SRM_OK) return rc;
  
    // call put
    
    // construct put request - only one file requested at a time
    SRMv2__TPutFileRequest * req_array = new SRMv2__TPutFileRequest[1];
  
    SRMv2__TPutFileRequest * r = new SRMv2__TPutFileRequest;
    // need to create new object here or doesn't work
    //std::string * surl = new std::string(srm_url.FullURL());
    r->targetSURL=(char*)req.surls().front().c_str();
    ULONG64 fsize = size;
    r->expectedFileSize=&fsize;
  
    req_array[0] = *r;
  
    SRMv2__ArrayOfTPutFileRequest * file_requests = new SRMv2__ArrayOfTPutFileRequest;
    file_requests->__sizerequestArray=1;
    file_requests->requestArray=&req_array;
  
    // transfer parameters with protocols
    SRMv2__TTransferParameters * transfer_params = new SRMv2__TTransferParameters;
    SRMv2__ArrayOfString * prot_array = new SRMv2__ArrayOfString;
    prot_array->__sizestringArray=size_of_supported_protocols;
    prot_array->stringArray=(char**)Supported_Protocols;
    transfer_params->arrayOfTransferProtocols=prot_array;
    
    SRMv2__srmPrepareToPutRequest * request = new SRMv2__srmPrepareToPutRequest;
    request->transferParameters=transfer_params;
    request->arrayOfFileRequests=file_requests;
  
    // set space token if supplied
    if(req.space_token() != "") request->targetSpaceToken = (char*)req.space_token().c_str();
  
    // dcache does not handle this correctly yet
    //request->desiredTotalRequestTime=(int*)&request_timeout;
  
    struct SRMv2__srmPrepareToPutResponse_ response_struct;
  
    // do the call
    int soap_err = SOAP_OK;
    if((soap_err=soap_call_SRMv2__srmPrepareToPut(&soapobj, csoap->SOAP_URL(), "srmPrepareToPut", request, response_struct)) != SOAP_OK){
      logger.msg(Arc::INFO, "SOAP request failed (%s)", "srmPrepareToPut");
      soap_print_fault(&soapobj, stderr);
      csoap->disconnect();
      delete[] req_array;
      return SRM_ERROR_SOAP;
    };
  
    delete[] req_array;
    SRMv2__srmPrepareToPutResponse * response_inst = response_struct.srmPrepareToPutResponse;
    SRMv2__TStatusCode return_status = response_inst->returnStatus->statusCode;
    SRMv2__ArrayOfTPutRequestFileStatus * file_statuses= response_inst->arrayOfFileStatuses;
  
    // store the request token in the request object
    if (response_inst->requestToken) req.request_token(response_inst->requestToken);
  
    // deal with response code
    if (return_status == SRMv2__TStatusCode__SRM_USCOREREQUEST_USCOREQUEUED ||
        return_status == SRMv2__TStatusCode__SRM_USCOREREQUEST_USCOREINPROGRESS) {
      // file is queued - need to wait and query with returned request token
  
      char * request_token = response_inst->requestToken;
      int sleeptime = 1;
      if (response_inst->arrayOfFileStatuses->statusArray[0]->estimatedWaitTime)
        sleeptime = *(response_inst->arrayOfFileStatuses->statusArray[0]->estimatedWaitTime);
      int request_time = 0;
  
      while(return_status != SRMv2__TStatusCode__SRM_USCORESUCCESS) {
  
        // sleep for recommended time (within limits)
        sleeptime = sleeptime<1?1:sleeptime;
        sleeptime = sleeptime>request_timeout ? request_timeout-request_time : sleeptime;
        logger.msg(Arc::VERBOSE, "%s: File request %s in SRM queue. Sleeping for %i seconds", req.surls().front(), request_token, sleeptime);
        sleep(sleeptime);
        request_time += sleeptime;
  
        SRMv2__srmStatusOfPutRequestRequest * sog_request = new SRMv2__srmStatusOfPutRequestRequest;
        sog_request->requestToken=request_token;
  
        struct SRMv2__srmStatusOfPutRequestResponse_ sog_response_struct;
  
        // call putRequestStatus
        if ((soap_err=soap_call_SRMv2__srmStatusOfPutRequest(&soapobj, csoap->SOAP_URL(), "srmStatusOfPutRequest", sog_request, sog_response_struct)) != SOAP_OK) {
          logger.msg(Arc::INFO, "SOAP request failed (%s)", "srmStatusOfPutRequest");
          soap_print_fault(&soapobj, stderr);
          csoap->disconnect();
          req.finished_abort();
          return SRM_ERROR_SOAP;
        };
  
        // check return codes - loop will exit on success or return false on error
  
        return_status = sog_response_struct.srmStatusOfPutRequestResponse->returnStatus->statusCode;
        file_statuses = sog_response_struct.srmStatusOfPutRequestResponse->arrayOfFileStatuses;
        
        if (return_status == SRMv2__TStatusCode__SRM_USCOREREQUEST_USCOREQUEUED ||
            return_status == SRMv2__TStatusCode__SRM_USCOREREQUEST_USCOREINPROGRESS) {
          // still queued - keep waiting
          // check for timeout
          if (request_time >= request_timeout) {
            logger.msg(Arc::ERROR, "Error: PrepareToPut request timed out after %i seconds", request_timeout);
            req.finished_abort();
            return SRM_ERROR_TEMPORARY;
          }
          if (file_statuses &&
              file_statuses->statusArray &&
              file_statuses->statusArray[0] &&
              file_statuses->statusArray[0]->estimatedWaitTime) {
            sleeptime = *(file_statuses->statusArray[0]->estimatedWaitTime);
          }
        }
        else if (return_status != SRMv2__TStatusCode__SRM_USCORESUCCESS) {
          // error
          // check individual file statuses
          if (file_statuses &&
              file_statuses->statusArray &&
              file_statuses->statusArray[0] &&
              file_statuses->statusArray[0]->status) {
            if (file_statuses->statusArray[0]->status->statusCode &&
                file_statuses->statusArray[0]->status->statusCode == SRMv2__TStatusCode__SRM_USCOREINVALID_USCOREPATH) {
              // make directories
              logger.msg(Arc::VERBOSE, "Path %s is invalid, creating required directories", req.surls().front());
              SRMReturnCode mkdirres = mkDir(req);
              if (mkdirres == SRM_OK)
                return putTURLs(req, urls, size);
              logger.msg(Arc::ERROR, "Error creating required directories for %s", req.surls().front());
              return mkdirres;
            }
            // log file-level error message
            if (file_statuses->statusArray[0]->status->explanation)
              logger.msg(Arc::ERROR, "Error: %s", file_statuses->statusArray[0]->status->explanation);
          }
          char * msg = sog_response_struct.srmStatusOfPutRequestResponse->returnStatus->explanation;
          logger.msg(Arc::ERROR, "Error: %s", msg);
          if (return_status == SRMv2__TStatusCode__SRM_USCOREINTERNAL_USCOREERROR)
            return SRM_ERROR_TEMPORARY;
          return SRM_ERROR_PERMANENT;
        };
      }; // while
  
    } // if file queued
  
    else if (return_status != SRMv2__TStatusCode__SRM_USCORESUCCESS) {
      // any other return code is a failure
      // check individual file statuses
      if (file_statuses &&
          file_statuses->statusArray &&
          file_statuses->statusArray[0] &&
          file_statuses->statusArray[0]->status) {
        if (file_statuses->statusArray[0]->status->statusCode &&
            file_statuses->statusArray[0]->status->statusCode == SRMv2__TStatusCode__SRM_USCOREINVALID_USCOREPATH) {
          // make directories
          logger.msg(Arc::VERBOSE, "Path %s is invalid, creating required directories", req.surls().front());
          SRMReturnCode mkdirres = mkDir(req);
          if (mkdirres == SRM_OK)
            return putTURLs(req, urls, size);
          logger.msg(Arc::ERROR, "Error creating required directories for %s", req.surls().front());
          return mkdirres;
        }
        // log file-level error message
        if (file_statuses->statusArray[0]->status->explanation)
          logger.msg(Arc::ERROR, "Error: %s", file_statuses->statusArray[0]->status->explanation);
      }  
      char * msg = response_inst->returnStatus->explanation;
      logger.msg(Arc::ERROR, "Error: %s", msg);
      if (return_status == SRMv2__TStatusCode__SRM_USCOREINTERNAL_USCOREERROR)
        return SRM_ERROR_TEMPORARY;
      return SRM_ERROR_PERMANENT;
    };
  
    // the file is ready and pinned - we can get the TURL
    char * turl = file_statuses->statusArray[0]->transferURL;
  
    logger.msg(Arc::VERBOSE, "File is ready! TURL is %s", turl);
    urls.push_back(std::string(turl));

    req.finished_success();
    return SRM_OK;
  }
  
  SRMReturnCode SRM22Client::info(SRMClientRequest& req,
                                  std::list<struct SRMFileMetaData>& metadata,
                                  const int recursive,
                                  bool report_error) {
    return info(req, metadata, recursive, report_error, 0, 0);
  }
  
  SRMReturnCode SRM22Client::info(SRMClientRequest& req,
                                  std::list<struct SRMFileMetaData>& metadata,
                                  const int recursive,
                                  bool report_error,
                                  const int offset,
                                  const int count) {
  
    SRMReturnCode rc = connect();
    if (rc != SRM_OK) return rc;
  
    // call ls
  
    // construct ls request - only one SURL requested at a time
    xsd__anyURI * req_array = new xsd__anyURI[1];
    req_array[0] = (char*)req.surls().front().c_str();
  
    SRMv2__ArrayOfAnyURI * surls_array = new SRMv2__ArrayOfAnyURI;
    surls_array->__sizeurlArray=1;
    surls_array->urlArray=req_array;
  
    SRMv2__srmLsRequest * request = new SRMv2__srmLsRequest;
    request->arrayOfSURLs=surls_array;
    // 0 corresponds to list the directory entry not the files in it
    // 1 corresponds to list the files in a directory - this is the desired
    // behaviour of ngls with no recursion, so we add 1 to the -r value
    request->numOfLevels=new int(recursive+1);
    
    // add count and offset options, if set
    if (offset != 0) request->offset = new int(offset);
    if (count != 0) request->count = new int(count);
    
    if (req.long_list()) request->fullDetailedList = new bool(true);
  
    struct SRMv2__srmLsResponse_ response_struct;
    
    int soap_err = SOAP_OK;
   
    // do the srmLs call
    if((soap_err=soap_call_SRMv2__srmLs(&soapobj, csoap->SOAP_URL(), "srmLs", request, response_struct)) != SOAP_OK){
      logger.msg(Arc::INFO, "SOAP request failed (%s)", "srmLs");
      soap_print_fault(&soapobj, stderr);
      csoap->disconnect();
      return SRM_ERROR_SOAP;
    };
  
    SRMv2__srmLsResponse * response_inst = response_struct.srmLsResponse;
    SRMv2__TStatusCode return_status = response_inst->returnStatus->statusCode;
    SRMv2__ArrayOfTMetaDataPathDetail * file_details= response_inst->details;
  
    if (response_inst->requestToken) req.request_token(response_inst->requestToken);
  
    // deal with response code - successful ones first
    if (return_status == SRMv2__TStatusCode__SRM_USCORESUCCESS) {
      // request is finished - we can get all the details
    }
    else if (return_status == SRMv2__TStatusCode__SRM_USCOREREQUEST_USCOREQUEUED ||
        return_status == SRMv2__TStatusCode__SRM_USCOREREQUEST_USCOREINPROGRESS) {
      // file is queued - need to wait and query with returned request token
  
      char * request_token = response_inst->requestToken;
      int sleeptime = 1;
      int request_time = 0;
  
      while(return_status != SRMv2__TStatusCode__SRM_USCORESUCCESS &&
            request_time < request_timeout) {
  
        // sleep for some time (no estimated time is given by the server)
        logger.msg(Arc::VERBOSE, "%s: File request %s in SRM queue. Sleeping for %i seconds", req.surls().front(), request_token, sleeptime);
        sleep(sleeptime);
        request_time += sleeptime;
  
        SRMv2__srmStatusOfLsRequestRequest * sols_request = new SRMv2__srmStatusOfLsRequestRequest;
        sols_request->requestToken=request_token;
  
        struct SRMv2__srmStatusOfLsRequestResponse_ sols_response_struct;
  
        // call statusOfLsResponse
        if ((soap_err=soap_call_SRMv2__srmStatusOfLsRequest(&soapobj, csoap->SOAP_URL(), "srmStatusOfLsRequest", sols_request, sols_response_struct)) != SOAP_OK) {
          logger.msg(Arc::INFO, "SOAP request failed (%s)", "srmStatusOfLsRequest");
          soap_print_fault(&soapobj, stderr);
          csoap->disconnect();
          return SRM_ERROR_SOAP;
        };
  
        // check return codes - loop will exit on success or return false on error
  
        return_status = sols_response_struct.srmStatusOfLsRequestResponse->returnStatus->statusCode;
        file_details = sols_response_struct.srmStatusOfLsRequestResponse->details;
        
        if (return_status != SRMv2__TStatusCode__SRM_USCORESUCCESS &&
            return_status != SRMv2__TStatusCode__SRM_USCOREREQUEST_USCOREQUEUED &&
            return_status != SRMv2__TStatusCode__SRM_USCOREREQUEST_USCOREINPROGRESS) {
          // error
          const char * msg = "Error in srmLs";
          if (sols_response_struct.srmStatusOfLsRequestResponse->returnStatus->explanation)
            msg = sols_response_struct.srmStatusOfLsRequestResponse->returnStatus->explanation;
          if (report_error) {
            logger.msg(Arc::ERROR, "Error: %s", msg);
          } else {
            logger.msg(Arc::VERBOSE, "Error: %s", msg);
          }
          // check if individual file status gives more info
          if (sols_response_struct.srmStatusOfLsRequestResponse->details &&
              sols_response_struct.srmStatusOfLsRequestResponse->details->pathDetailArray &&
              sols_response_struct.srmStatusOfLsRequestResponse->details->__sizepathDetailArray > 0 &&
              sols_response_struct.srmStatusOfLsRequestResponse->details->pathDetailArray[0]->status &&
              sols_response_struct.srmStatusOfLsRequestResponse->details->pathDetailArray[0]->status->explanation) {
            if (report_error) {
              logger.msg(Arc::ERROR, "Error: %s", sols_response_struct.srmStatusOfLsRequestResponse->details->pathDetailArray[0]->status->explanation);
            } else {
              logger.msg(Arc::VERBOSE, "Error: %s", sols_response_struct.srmStatusOfLsRequestResponse->details->pathDetailArray[0]->status->explanation);
            }
          }
          if (return_status == SRMv2__TStatusCode__SRM_USCOREINTERNAL_USCOREERROR)
            return SRM_ERROR_TEMPORARY;
          return SRM_ERROR_PERMANENT;
        };
      }; // while
  
      // check for timeout
      if (request_time >= request_timeout) {
        logger.msg(Arc::ERROR, "Error: Ls request timed out after %i seconds", request_timeout);
        abort(req);
        return SRM_ERROR_TEMPORARY;
      }
  
    } // else if request queued
  
    else {
      // any other return code is a failure
      const char * msg = "Error in srmLs";
      if (response_inst->returnStatus->explanation)
        msg = response_inst->returnStatus->explanation;
      if (report_error) {
        logger.msg(Arc::ERROR, "Error: %s", msg);
      } else {
        logger.msg(Arc::VERBOSE, "Error: %s", msg);
      }
      // check if individual file status gives more info
      if (response_inst->details &&
          response_inst->details->pathDetailArray &&
          response_inst->details->__sizepathDetailArray > 0 &&
          response_inst->details->pathDetailArray[0]->status &&
          response_inst->details->pathDetailArray[0]->status->explanation) {
        if (report_error) {
          logger.msg(Arc::ERROR, "Error: %s", response_inst->details->pathDetailArray[0]->status->explanation);
        } else {
          logger.msg(Arc::VERBOSE, "Error: %s", response_inst->details->pathDetailArray[0]->status->explanation);
        }
      }
      if (return_status == SRMv2__TStatusCode__SRM_USCOREINTERNAL_USCOREERROR)
        return SRM_ERROR_TEMPORARY;
      return SRM_ERROR_PERMANENT;
    }
  
    // the request is ready - collect the details
    if (!file_details || !file_details->pathDetailArray || file_details->__sizepathDetailArray == 0 || !file_details->pathDetailArray[0]) return SRM_OK; // see bug 1364
    // first the file or directory corresponding to the surl
    SRMv2__TMetaDataPathDetail * details = file_details->pathDetailArray[0];
    if (!details->type || *(details->type) != SRMv2__TFileType__DIRECTORY || recursive < 0) {
      // it can happen that with multiple calls to info() for large dirs the last
      // call returns one directory. In this case we want to list it without the
      // directory structure.
      if (count == 0) metadata.push_back(fillDetails(details, false));
      else metadata.push_back(fillDetails(details, true));
    };
  
    // look for sub paths (files in a directory)
    SRMv2__ArrayOfTMetaDataPathDetail * subpaths;
    
    // schema differs by implementation...
    // dcache and dpm
    if (details->arrayOfSubPaths) subpaths = details->arrayOfSubPaths;
    // castor
    else if (file_details->__sizepathDetailArray > 1) subpaths = file_details;
    // no subpaths
    else return SRM_OK;
 
    // sometimes we don't know if we have a file or dir so take out the
    // entry added above if there are subpaths and offset is 0
    if (offset == 0 && subpaths->__sizepathDetailArray > 0)
      metadata.clear();
  
    // if there are more entries than max_files_list, we have to call info()
    // multiple times, setting offset and count
    for (int i = 0; i<subpaths->__sizepathDetailArray; i++) {
      if (i == max_files_list) {
        // call multiple times
        logger.msg(Arc::INFO, "Directory size is larger than %i files, will have to call multiple times", max_files_list);
        std::list<SRMFileMetaData> list_metadata;
        int list_no = 1;
        int list_offset = 0;
        int list_count = 0;
        do {
          list_metadata.clear();
          SRMClientRequest list_req(req.surls().front());
          list_offset = max_files_list * list_no;
          list_count = max_files_list;
          SRMReturnCode res = info(list_req, list_metadata, 0, true, list_offset, list_count);
          if (res != SRM_OK) return res;
          list_no++;
          // append to metadata
          for (std::list<SRMFileMetaData>::iterator it = list_metadata.begin();
               it != list_metadata.end();
               ++it) {
            metadata.push_back(*it);
          }
        } while (list_metadata.size() == max_files_list);
        break;
      }
      SRMv2__TMetaDataPathDetail * sub_details = subpaths->pathDetailArray[i];
      if (sub_details) metadata.push_back(fillDetails(sub_details, true));
  
    };
  
    // if castor take out the first two entries which are the directory
    if (file_details->__sizepathDetailArray > 1) {
      metadata.pop_front();
      // only take off the second one if no offset
      if (offset == 0) metadata.pop_front();
    };
  
    // sort list by filename
    //metadata.sort(compare_srm_file_meta_data);
  
    return SRM_OK;
  }
  
  SRMFileMetaData SRM22Client::fillDetails(SRMv2__TMetaDataPathDetail * details,
                                           bool directory) {
  
    SRMFileMetaData metadata;
  
    if(details->path){
      char * path = details->path;
      metadata.path = path;
      std::string::size_type i = metadata.path.find("//", 0);
      while (i != std::string::npos) {
        metadata.path.erase(i, 1);
        i = metadata.path.find("//", 0);
      };
      if (metadata.path.find("/") != 0) metadata.path = "/" + metadata.path;
      if (directory) {
      	// only use the basename of the path
      	metadata.path = metadata.path.substr(metadata.path.rfind("/", metadata.path.length())+1);
      };
    };
  
    if(details->size){
      ULONG64 * fsize = details->size;
      metadata.size = *fsize;
    }
    else {metadata.size = -1;};
  
    if(details->checkSumType){
      char * checksum_type = details->checkSumType;
      metadata.checkSumType = checksum_type;
    }
    else {metadata.checkSumType = "";};
  
    if(details->checkSumValue){
      char * checksum_value = details->checkSumValue;
      metadata.checkSumValue = checksum_value;
    }
    else {metadata.checkSumValue = "";};
  
    if(details->createdAtTime){
      time_t * creation_time = details->createdAtTime;
      metadata.createdAtTime = *creation_time;
    } 
    else {metadata.createdAtTime = 0;};
  
    if(details->type){
      SRMv2__TFileType * file_type = details->type;
      if (*file_type == SRMv2__TFileType__FILE_) metadata.fileType = SRM_FILE;
      else if (*file_type == SRMv2__TFileType__DIRECTORY) metadata.fileType = SRM_DIRECTORY;
      else if (*file_type == SRMv2__TFileType__LINK) metadata.fileType = SRM_LINK;
    }
    else {metadata.fileType = SRM_FILE_TYPE_UNKNOWN;};
  
    if(details->fileLocality){
      SRMv2__TFileLocality * file_locality = details->fileLocality;
      if (*file_locality == SRMv2__TFileLocality__ONLINE ||
          *file_locality == SRMv2__TFileLocality__ONLINE_USCOREAND_USCORENEARLINE) {
        metadata.fileLocality = SRM_ONLINE;
      }
      else if (*file_locality == SRMv2__TFileLocality__NEARLINE) {
        metadata.fileLocality = SRM_NEARLINE;
      };
    }
    else { metadata.fileLocality = SRM_UNKNOWN; };

    if(details->arrayOfSpaceTokens && details->arrayOfSpaceTokens->__sizestringArray > 0) {
      std::string tokens;
      for(int i = 0; i < details->arrayOfSpaceTokens->__sizestringArray; i++) {
        if (i == details->arrayOfSpaceTokens->__sizestringArray - 1) tokens += details->arrayOfSpaceTokens->stringArray[i];
        else tokens += std::string(details->arrayOfSpaceTokens->stringArray[i]) + ",";
      }
      metadata.arrayOfSpaceTokens = tokens;
    }
    
    if(details->ownerPermission && details->groupPermission && details->otherPermission) {
      std::string perm;
      if(details->ownerPermission->userID) metadata.owner = details->ownerPermission->userID;
      if(details->groupPermission->groupID) metadata.group = details->groupPermission->groupID;
      if(details->ownerPermission->mode &&
         details->groupPermission->mode &&
         details->otherPermission) {
        std::string perms;
        if (details->ownerPermission->mode & 4) perms += 'r'; else perms += '-';
        if (details->ownerPermission->mode & 2) perms += 'w'; else perms += '-';
        if (details->ownerPermission->mode & 1) perms += 'x'; else perms += '-';
        if (details->groupPermission->mode & 4) perms += 'r'; else perms += '-';
        if (details->groupPermission->mode & 2) perms += 'w'; else perms += '-';
        if (details->groupPermission->mode & 1) perms += 'x'; else perms += '-';
        if (*(details->otherPermission) & 4) perms += 'r'; else perms += '-';
        if (*(details->otherPermission) & 2) perms += 'w'; else perms += '-';
        if (*(details->otherPermission) & 1) perms += 'x'; else perms += '-';
        metadata.permission = perms;
       }
    }
    
    if(details->lastModificationTime) {
      time_t * mod_time = details->lastModificationTime;
      metadata.lastModificationTime = *mod_time;
    }
    else {metadata.lastModificationTime = 0;}
    
    if(details->lifetimeAssigned) metadata.lifetimeAssigned = *(details->lifetimeAssigned);
    else metadata.lifetimeAssigned = 0;
    if(details->lifetimeLeft) metadata.lifetimeLeft = *(details->lifetimeLeft);  
    else metadata.lifetimeLeft = 0;
    
    if(details->retentionPolicyInfo) {
      if (details->retentionPolicyInfo->retentionPolicy == SRMv2__TRetentionPolicy__REPLICA) metadata.retentionPolicy = SRM_REPLICA;
      else if (details->retentionPolicyInfo->retentionPolicy == SRMv2__TRetentionPolicy__OUTPUT) metadata.retentionPolicy = SRM_OUTPUT;
      else if (details->retentionPolicyInfo->retentionPolicy == SRMv2__TRetentionPolicy__CUSTODIAL) metadata.retentionPolicy = SRM_CUSTODIAL;
      else metadata.retentionPolicy = SRM_RETENTION_UNKNOWN;
    }
    else {metadata.retentionPolicy = SRM_RETENTION_UNKNOWN;}
    
    if(details->fileStorageType) {
      SRMv2__TFileStorageType * file_storage_type = details->fileStorageType;
      if (*file_storage_type == SRMv2__TFileStorageType__VOLATILE) metadata.fileStorageType = SRM_VOLATILE;
      else if (*file_storage_type == SRMv2__TFileStorageType__DURABLE) metadata.fileStorageType = SRM_DURABLE;
      else if (*file_storage_type == SRMv2__TFileStorageType__PERMANENT) metadata.fileStorageType = SRM_PERMANENT;
      else metadata.fileStorageType = SRM_FILE_STORAGE_UNKNOWN;
    }
    else {metadata.fileStorageType = SRM_FILE_STORAGE_UNKNOWN;}
    
    // if any other value, leave undefined
  
    return metadata;
  
  }
  
  SRMReturnCode SRM22Client::releaseGet(SRMClientRequest& req) {
  
    // Release all the pins referred to by the request token in the request object
    SRMv2__srmReleaseFilesRequest * request = new SRMv2__srmReleaseFilesRequest;
    if(req.request_token().empty()) {
      logger.msg(Arc::ERROR, "No request token specified!");
      return SRM_ERROR_OTHER;
    };
    request->requestToken=(char*)req.request_token().c_str();
  
    struct SRMv2__srmReleaseFilesResponse_ response_struct;
  
    int soap_err = SOAP_OK;
   
    // do the srmReleaseFiles call
    if((soap_err=soap_call_SRMv2__srmReleaseFiles(&soapobj, csoap->SOAP_URL(), "srmReleaseFiles", request, response_struct)) != SOAP_OK){
      logger.msg(Arc::INFO, "SOAP request failed (%s)", "srmReleaseFiles");
      soap_print_fault(&soapobj, stderr);
      csoap->disconnect();
      return SRM_ERROR_SOAP;
    };
  
    if (response_struct.srmReleaseFilesResponse->returnStatus->statusCode != SRMv2__TStatusCode__SRM_USCORESUCCESS) {
      char * msg = response_struct.srmReleaseFilesResponse->returnStatus->explanation;
      logger.msg(Arc::ERROR, "Error: %s", msg);
      csoap->disconnect();
      if (response_struct.srmReleaseFilesResponse->returnStatus->statusCode == SRMv2__TStatusCode__SRM_USCOREINTERNAL_USCOREERROR)
        return SRM_ERROR_TEMPORARY;
      return SRM_ERROR_PERMANENT;
    };
  
    // release went ok
    logger.msg(Arc::VERBOSE, "Files associated with request token %s released successfully", req.request_token());
    return SRM_OK;
  
  };
  
  
  SRMReturnCode SRM22Client::releasePut(SRMClientRequest& req) {
  
    // Set the files referred to by the request token in the request object
    // which were prepared to put to done
    SRMv2__srmPutDoneRequest * request = new SRMv2__srmPutDoneRequest;
    if(req.request_token().empty()) {
      logger.msg(Arc::ERROR, "No request token specified!");
      return SRM_ERROR_OTHER;
    };
    request->requestToken=(char*)req.request_token().c_str();

    // add the SURLs to the request
    xsd__anyURI * req_array = new xsd__anyURI[1];
    req_array[0] = (char*)req.surls().front().c_str();
  
    SRMv2__ArrayOfAnyURI * surls_array = new SRMv2__ArrayOfAnyURI;
    surls_array->__sizeurlArray=1;
    surls_array->urlArray=req_array;
  
    request->arrayOfSURLs=surls_array;
    
    struct SRMv2__srmPutDoneResponse_ response_struct;
  
    int soap_err = SOAP_OK;
   
    // do the srmPutDone call
    if((soap_err=soap_call_SRMv2__srmPutDone(&soapobj, csoap->SOAP_URL(), "srmPutDone", request, response_struct)) != SOAP_OK){
      logger.msg(Arc::INFO, "SOAP request failed (%s)", "srmPutDone");
      soap_print_fault(&soapobj, stderr);
      csoap->disconnect();
      return SRM_ERROR_SOAP;
    };
  
    if (response_struct.srmPutDoneResponse->returnStatus->statusCode != SRMv2__TStatusCode__SRM_USCORESUCCESS) {
      char * msg = response_struct.srmPutDoneResponse->returnStatus->explanation;
      logger.msg(Arc::ERROR, "Error: %s", msg);
      csoap->disconnect();
      if (response_struct.srmPutDoneResponse->returnStatus->statusCode == SRMv2__TStatusCode__SRM_USCOREINTERNAL_USCOREERROR)
        return SRM_ERROR_TEMPORARY;
      return SRM_ERROR_PERMANENT;
    };
  
    // release went ok
    logger.msg(Arc::VERBOSE, "Files associated with request token %s put done successfully", req.request_token());
    return SRM_OK;
  
  };
  
  SRMReturnCode SRM22Client::abort(SRMClientRequest& req) {
  
    // Call srmAbortRequest on the files in the request token
    SRMv2__srmAbortRequestRequest * request = new SRMv2__srmAbortRequestRequest;
    if(req.request_token().empty()) {
      logger.msg(Arc::ERROR, "No request token specified!");
      return SRM_ERROR_OTHER;
    };
    request->requestToken=(char*)req.request_token().c_str();
  
    struct SRMv2__srmAbortRequestResponse_ response_struct;
  
    int soap_err = SOAP_OK;
   
    // do the srmAbortRequest call
    if((soap_err=soap_call_SRMv2__srmAbortRequest(&soapobj, csoap->SOAP_URL(), "srmAbortRequest", request, response_struct)) != SOAP_OK){
      logger.msg(Arc::INFO, "SOAP request failed (%s)", "srmAbortRequest");
      soap_print_fault(&soapobj, stderr);
      csoap->disconnect();
      return SRM_ERROR_SOAP;
    };
  
    if (response_struct.srmAbortRequestResponse->returnStatus->statusCode != SRMv2__TStatusCode__SRM_USCORESUCCESS) {
      char * msg = response_struct.srmAbortRequestResponse->returnStatus->explanation;
      logger.msg(Arc::ERROR, "Error: %s", msg);
      csoap->disconnect();
      if (response_struct.srmAbortRequestResponse->returnStatus->statusCode == SRMv2__TStatusCode__SRM_USCOREINTERNAL_USCOREERROR)
        return SRM_ERROR_TEMPORARY;
      return SRM_ERROR_PERMANENT;
    };
  
    // release went ok
    logger.msg(Arc::VERBOSE, "Files associated with request token %s aborted successfully", req.request_token());
    return SRM_OK;
  
  };
  
  SRMReturnCode SRM22Client::remove(SRMClientRequest& req) {
  
    // TODO: bulk remove
  
    // call info() to find out if we are dealing with a file or directory
    SRMClientRequest inforeq(req.surls());
    std::list<struct SRMFileMetaData> metadata;
  
    // set recursion to -1, meaning don't list entries in a dir
    SRMReturnCode res = info(inforeq, metadata, -1);
    if(res != SRM_OK) {
      logger.msg(Arc::ERROR, "Failed to find metadata info on file %s", inforeq.surls().front());
      return res;
    };
  
    if(metadata.front().fileType == SRM_FILE) {
      logger.msg(Arc::VERBOSE, "Type is file, calling srmRm");
      return removeFile(req);
    };
    if(metadata.front().fileType == SRM_DIRECTORY) {
      logger.msg(Arc::VERBOSE, "Type is dir, calling srmRmDir");
      return removeDir(req);
    };
  
    logger.msg(Arc::WARNING, "File type is not available, attempting file delete");
    if (removeFile(req) == SRM_OK)
      return SRM_OK;
    logger.msg(Arc::WARNING, "File delete failed, attempting directory delete");
    return removeDir(req);
  };
  
  SRMReturnCode SRM22Client::removeFile(SRMClientRequest& req) {
  
    SRMReturnCode rc = connect();
    if (rc != SRM_OK) return rc;

    // construct rm request - only one file requested at a time
    xsd__anyURI * req_array = new xsd__anyURI[1];
    req_array[0] = (char*)req.surls().front().c_str();
  
    SRMv2__ArrayOfAnyURI * surls_array = new SRMv2__ArrayOfAnyURI;
    surls_array->__sizeurlArray=1;
    surls_array->urlArray=req_array;
  
    // Call srmRm on the files in the request token
    SRMv2__srmRmRequest * request = new SRMv2__srmRmRequest;
    request->arrayOfSURLs=surls_array;
  
    struct SRMv2__srmRmResponse_ response_struct;
  
    int soap_err = SOAP_OK;
   
    // do the srmRm call
    if((soap_err=soap_call_SRMv2__srmRm(&soapobj, csoap->SOAP_URL(), "srmRm", request, response_struct)) != SOAP_OK){
      logger.msg(Arc::INFO, "SOAP request failed (%s)", "srmRm");
      soap_print_fault(&soapobj, stderr);
      csoap->disconnect();
      return SRM_ERROR_SOAP;
    };
  
    if (response_struct.srmRmResponse->returnStatus->statusCode != SRMv2__TStatusCode__SRM_USCORESUCCESS) {
      char * msg = response_struct.srmRmResponse->returnStatus->explanation;
      logger.msg(Arc::ERROR, "Error: %s", msg);
      csoap->disconnect();
      if (response_struct.srmRmResponse->returnStatus->statusCode == SRMv2__TStatusCode__SRM_USCOREINTERNAL_USCOREERROR)
        return SRM_ERROR_TEMPORARY;
      return SRM_ERROR_PERMANENT;
    };
  
    // remove went ok
    logger.msg(Arc::VERBOSE, "File %s removed successfully", req.surls().front());
    return SRM_OK;
  
  };
  
  
  SRMReturnCode SRM22Client::removeDir(SRMClientRequest& req) {
  
    SRMReturnCode rc = connect();
    if (rc != SRM_OK) return rc;

    // construct rmdir request - only one file requested at a time
    xsd__anyURI surl = (char*)req.surls().front().c_str();
  
    // Call srmRmdir on the files in the request token
    SRMv2__srmRmdirRequest * request = new SRMv2__srmRmdirRequest;
    request->SURL = surl;
  
    struct SRMv2__srmRmdirResponse_ response_struct;
  
    int soap_err = SOAP_OK;
   
    // do the srmRm call
    if((soap_err=soap_call_SRMv2__srmRmdir(&soapobj, csoap->SOAP_URL(), "srmRmdir", request, response_struct)) != SOAP_OK){
      logger.msg(Arc::INFO, "SOAP request failed (%s)", "srmRmdir");
      soap_print_fault(&soapobj, stderr);
      csoap->disconnect();
      return SRM_ERROR_SOAP;
    };
  
    if (response_struct.srmRmdirResponse->returnStatus->statusCode != SRMv2__TStatusCode__SRM_USCORESUCCESS) {
      char * msg = response_struct.srmRmdirResponse->returnStatus->explanation;
      logger.msg(Arc::ERROR, "Error: %s", msg);
      csoap->disconnect();
      if (response_struct.srmRmdirResponse->returnStatus->statusCode == SRMv2__TStatusCode__SRM_USCOREINTERNAL_USCOREERROR)
        return SRM_ERROR_TEMPORARY;
      return SRM_ERROR_PERMANENT;
    };
  
    // remove went ok
    logger.msg(Arc::VERBOSE, "Directory %s removed successfully", req.surls().front());
    return SRM_OK;
  
  };
  
  SRMReturnCode SRM22Client::copy(SRMClientRequest& req, const std::string& source) {
    
    // construct copy request
    SRMv2__TCopyFileRequest * copyrequest = new SRMv2__TCopyFileRequest;
    copyrequest->sourceSURL = (char*)source.c_str();
    copyrequest->targetSURL = (char*)req.surls().front().c_str();
    
    SRMv2__TCopyFileRequest ** req_array = new SRMv2__TCopyFileRequest*[1];
    req_array[0] = copyrequest;
  
    SRMv2__ArrayOfTCopyFileRequest * file_requests = new SRMv2__ArrayOfTCopyFileRequest;
    file_requests->__sizerequestArray=1;
    file_requests->requestArray=req_array;
  
    SRMv2__srmCopyRequest * request = new SRMv2__srmCopyRequest;
    request->arrayOfFileRequests=file_requests;
  
    // set space token if supplied
    if(req.space_token() != "") request->targetSpaceToken = (char*)req.space_token().c_str();
  
    struct SRMv2__srmCopyResponse_ response_struct;
  
    // do the call
    int soap_err = SOAP_OK;
    if((soap_err=soap_call_SRMv2__srmCopy(&soapobj, csoap->SOAP_URL(), "srmCopy", request, response_struct)) != SOAP_OK){
      logger.msg(Arc::INFO, "SOAP request failed (%s)", "srmCopy");
      soap_print_fault(&soapobj, stderr);
      csoap->disconnect();
      return SRM_ERROR_SOAP;
    };
  
    SRMv2__srmCopyResponse * response_inst = response_struct.srmCopyResponse;
    SRMv2__TStatusCode return_status = response_inst->returnStatus->statusCode;
    SRMv2__ArrayOfTCopyRequestFileStatus * file_statuses= response_inst->arrayOfFileStatuses;
  
    // store the request token in the request object
    if (response_inst->requestToken) req.request_token(response_inst->requestToken);
  
    // set timeout for copying. Since we don't know the progress of the 
    // transfer we hard code a value 10 x the request timeout
    time_t copy_timeout = request_timeout * 10;
    
    // deal with response code
    if (return_status == SRMv2__TStatusCode__SRM_USCOREREQUEST_USCOREQUEUED ||
        return_status == SRMv2__TStatusCode__SRM_USCOREREQUEST_USCOREINPROGRESS) {
      // request is queued - need to wait and query with returned request token
      char * request_token = response_inst->requestToken;
  
      int sleeptime = 1;
      if (response_inst->arrayOfFileStatuses->statusArray[0]->estimatedWaitTime)
        sleeptime = *(response_inst->arrayOfFileStatuses->statusArray[0]->estimatedWaitTime);
      int request_time = 0;
  
      while(return_status != SRMv2__TStatusCode__SRM_USCORESUCCESS &&
            request_time < copy_timeout) {
  
        // sleep for recommended time (within limits)
        sleeptime = sleeptime<1?1:sleeptime;
        sleeptime = sleeptime>10?10:sleeptime;
        logger.msg(Arc::VERBOSE, "%s: File request %s in SRM queue. Sleeping for %i seconds", req.surls().front(), request_token, sleeptime);
        sleep(sleeptime);
        request_time += sleeptime;
  
        SRMv2__srmStatusOfCopyRequestRequest * soc_request = new SRMv2__srmStatusOfCopyRequestRequest;
        soc_request->requestToken=request_token;
  
        struct SRMv2__srmStatusOfCopyRequestResponse_ soc_response_struct;
  
        // call statusOfCopyRequest
        if ((soap_err=soap_call_SRMv2__srmStatusOfCopyRequest(&soapobj, csoap->SOAP_URL(), "srmStatusOfCopyRequest", soc_request, soc_response_struct)) != SOAP_OK) {
          logger.msg(Arc::INFO, "SOAP request failed (%s)", "srmStatusOfCopyRequest");
          soap_print_fault(&soapobj, stderr);
          csoap->disconnect();
          req.finished_abort();
          return SRM_ERROR_SOAP;
        };
  
        // check return codes - loop will exit on success or return false on error
  
        return_status = soc_response_struct.srmStatusOfCopyRequestResponse->returnStatus->statusCode;
        file_statuses = soc_response_struct.srmStatusOfCopyRequestResponse->arrayOfFileStatuses;
        
        if (return_status == SRMv2__TStatusCode__SRM_USCOREREQUEST_USCOREQUEUED ||
            return_status == SRMv2__TStatusCode__SRM_USCOREREQUEST_USCOREINPROGRESS) {
          // still queued - keep waiting
          if(file_statuses->statusArray[0]->estimatedWaitTime)
            sleeptime = *(file_statuses->statusArray[0]->estimatedWaitTime);
        }
        else if (return_status != SRMv2__TStatusCode__SRM_USCORESUCCESS) {
          // error
          char * msg = soc_response_struct.srmStatusOfCopyRequestResponse->returnStatus->explanation;
          logger.msg(Arc::ERROR, "Error: %s", msg);
          if (return_status == SRMv2__TStatusCode__SRM_USCOREINTERNAL_USCOREERROR)
            return SRM_ERROR_TEMPORARY;
          return SRM_ERROR_PERMANENT;
        };
      }; // while
  
      // check for timeout
      if (request_time >= copy_timeout) {
        logger.msg(Arc::ERROR, "Error: copy request timed out after %i seconds", copy_timeout);
        req.finished_abort();
        return SRM_ERROR_TEMPORARY;
      }
  
    } // if file queued
  
    else if (return_status != SRMv2__TStatusCode__SRM_USCORESUCCESS) {
      // any other return code is a failure
      char * msg = response_inst->returnStatus->explanation;
      logger.msg(Arc::ERROR, "Error: %s", msg);
      if (return_status == SRMv2__TStatusCode__SRM_USCOREINTERNAL_USCOREERROR)
        return SRM_ERROR_TEMPORARY;
      return SRM_ERROR_PERMANENT;
    };
    req.finished_success();
    return SRM_OK;
  };
  
  SRMReturnCode SRM22Client::mkDir(SRMClientRequest& req) {
    
    std::string surl = req.surls().front();
    std::string::size_type slashpos = surl.find('/', 6);
    slashpos = surl.find('/', slashpos+1); // don't create root dir
    bool keeplisting = true; // whether to keep checking dir exists
    while (slashpos != std::string::npos) {
      std::string dirname = surl.substr(0, slashpos);
      // list dir to see if it exists
      SRMClientRequest listreq(dirname);
      std::list<struct SRMFileMetaData> metadata;
      if (keeplisting) {
        logger.msg(Arc::VERBOSE, "Checking for existence of %s", dirname);
        if (info(listreq, metadata, -1, false) == SRM_OK) {
          if (metadata.front().fileType == SRM_FILE) {
            logger.msg(Arc::ERROR, "File already exists: %s", dirname);
            return SRM_ERROR_PERMANENT;
          }
          slashpos = surl.find("/", slashpos+1);
          continue;
        }; 
      };
      
      logger.msg(Arc::VERBOSE, "Creating directory %s", dirname);
      // construct mkdir request
      xsd__anyURI dir = (char*)dirname.c_str();
      SRMv2__srmMkdirRequest * request = new SRMv2__srmMkdirRequest;
      request->SURL = dir;
  
      struct SRMv2__srmMkdirResponse_ response_struct;
  
      int soap_err = SOAP_OK;
   
      // do the srmMkdir call
      if((soap_err=soap_call_SRMv2__srmMkdir(&soapobj, csoap->SOAP_URL(), "srmMkdir", request, response_struct)) != SOAP_OK){
        logger.msg(Arc::INFO, "SOAP request failed (srmMkdir)");
        soap_print_fault(&soapobj, stderr);
        csoap->disconnect();
        return SRM_ERROR_SOAP;
      };
  
      slashpos = surl.find("/", slashpos+1);
      
      // there can be undetectable errors in creating dirs that already exist
      // so only report error on creating the final dir
      if (response_struct.srmMkdirResponse->returnStatus->statusCode == SRMv2__TStatusCode__SRM_USCORESUCCESS ||
          response_struct.srmMkdirResponse->returnStatus->statusCode == SRMv2__TStatusCode__SRM_USCOREDUPLICATION_USCOREERROR) 
        keeplisting = false;
      else if (slashpos == std::string::npos) {
        char * msg = response_struct.srmMkdirResponse->returnStatus->explanation;
        logger.msg(Arc::ERROR, "Error creating directory %s: %s", dir, msg);
        csoap->disconnect();
        if (response_struct.srmMkdirResponse->returnStatus->statusCode == SRMv2__TStatusCode__SRM_USCOREINTERNAL_USCOREERROR)
          return SRM_ERROR_TEMPORARY;
        return SRM_ERROR_PERMANENT;
      };
    };
    return SRM_OK;
  };

//} // namespace Arc
