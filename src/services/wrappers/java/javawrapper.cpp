#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>
#include <arc/message/SOAPMessage.h>
#include <arc/message/PayloadSOAP.h>
#include "javawrapper.h"

static Arc::Plugin* get_service(Arc::PluginArgument* arg) {
    Arc::ServicePluginArgument* srvarg =
            arg?dynamic_cast<Arc::ServicePluginArgument*>(arg):NULL;
    if(!srvarg) return NULL;
    return new Arc::Service_JavaWrapper((Arc::Config*)(*srvarg));
}

Arc::PluginDescriptor PLUGINS_TABLE_NAME[] = {
    { "arcservice_javawrapper", "HED:SERVICE", 0, &get_service },
    { NULL, NULL, 0, NULL }
};

namespace Arc {

Arc::Logger Service_JavaWrapper::logger(Service::logger, "JavaWrapper");

Service_JavaWrapper::Service_JavaWrapper(Arc::Config *cfg)
  : Service(cfg),
    libjvm(NULL),
    jvm(NULL),
    classPath(NULL) {
    std::string path = "-Djava.class.path=" + (std::string)((*cfg)["ClassPath"]);
    std::string class_name = (std::string)(*cfg)["ClassName"];
    logger.msg(Arc::VERBOSE, "config: %s, class name: %s", path, class_name);
    JNIEnv *jenv = NULL;
    JavaVMInitArgs jvm_args;
    JavaVMOption options[1];
    /* Initiliaze java engine */
    Glib::ModuleFlags flags = Glib::ModuleFlags(0);
    libjvm = new Glib::Module("libjvm.so", flags);
    if (!*libjvm) {
        logger.msg(Arc::ERROR,
		   "libjvm.so not loadable - check your LD_LIBRARY_PATH");
	return;
    }
    void* myJNI_GetDefaultJavaVMInitArgs;
    libjvm->get_symbol("JNI_GetDefaultJavaVMInitArgs",
		       myJNI_GetDefaultJavaVMInitArgs);
    void* myJNI_CreateJavaVM;
    libjvm->get_symbol("JNI_CreateJavaVM", myJNI_CreateJavaVM);
    if (myJNI_GetDefaultJavaVMInitArgs == NULL || myJNI_CreateJavaVM == NULL) {
        logger.msg(Arc::ERROR,
		   "libjvm.so does not contain the expected symbols");
	return;
    }
    ((jint(*)(void*))myJNI_GetDefaultJavaVMInitArgs)(&jvm_args);
    jvm_args.version = JNI_VERSION_1_2;
    jvm_args.nOptions = 1;
    classPath = strdup(path.c_str());
    options[0].optionString = classPath;
    options[0].extraInfo = NULL;
    // "-Djava.class.path=.:/home/szferi/arc1/src/services/echo_java/:/home/szferi/arc1/java/arc.jar";
    jvm_args.options = options;
    jvm_args.ignoreUnrecognized = JNI_FALSE;
    ((jint(*)(JavaVM**, void**, void*))myJNI_CreateJavaVM)(&jvm, (void **)&jenv,
							   &jvm_args);
    logger.msg(Arc::VERBOSE, "JVM started");
    
    /* Find and construct class */
    serviceClass = jenv->FindClass(class_name.c_str());
    if (serviceClass == NULL) {
        logger.msg(Arc::ERROR, "There is no service: %s in your java class search path", class_name);
        if (jenv->ExceptionOccurred()) {
            jenv->ExceptionDescribe();
        }
        return;
    }
    jmethodID constructorID = jenv->GetMethodID(serviceClass, "<init>", "()V");
    if (constructorID == NULL) {
        logger.msg(Arc::ERROR, "There is no constructor function");
        if (jenv->ExceptionOccurred()) {
            jenv->ExceptionDescribe();
        }
        return;
    }
    serviceObj = jenv->NewObject(serviceClass, constructorID);
    logger.msg(Arc::VERBOSE, "%s constructed", class_name);
}

Service_JavaWrapper::~Service_JavaWrapper(void) {
    logger.msg(Arc::VERBOSE, "Destroy jvm"); 
    if (jvm)
        jvm->DestroyJavaVM();
    if (libjvm)
        delete libjvm;
    if (classPath)
        free(classPath);
}

Arc::MCC_Status Service_JavaWrapper::make_fault(Arc::Message& outmsg) 
{
    Arc::PayloadSOAP* outpayload = new Arc::PayloadSOAP(Arc::NS(),true);
    Arc::SOAPFault* fault = outpayload->Fault();
    if(fault) {
        fault->Code(Arc::SOAPFault::Sender);
        fault->Reason("Failed processing request");
    };
    outmsg.Payload(outpayload);
    return Arc::MCC_Status();
}

Arc::MCC_Status Service_JavaWrapper::java_error(JNIEnv *jenv, const char *str) {
    std::cerr << str << std::endl;
    if (jenv->ExceptionOccurred()) {
        jenv->ExceptionDescribe();
    }
    /* Cleanup */
    jvm->DetachCurrentThread();
    return Arc::MCC_Status(Arc::GENERIC_ERROR);
}

Arc::MCC_Status Service_JavaWrapper::process(Arc::Message& inmsg, Arc::Message& outmsg) 
{
    JNIEnv *jenv = NULL;
    
    /* Attach to the current java engine thread */
    jvm->AttachCurrentThread((void **)&jenv, NULL);
    /* Get the process function of service */
    jmethodID processID = jenv->GetMethodID(serviceClass, "process", "(Lnordugrid/arc/SOAPMessage;Lnordugrid/arc/SOAPMessage;)Lnordugrid/arc/MCC_Status;");
    if (processID == NULL) {
        return java_error(jenv, "Cannot find process method of java class");
    }
    /* convert inmsg and outmsg to java objects */
    Arc::SOAPMessage *inmsg_ptr = NULL;
    Arc::SOAPMessage *outmsg_ptr = NULL;
    try {
        inmsg_ptr = new Arc::SOAPMessage(inmsg);
        outmsg_ptr = new Arc::SOAPMessage(outmsg);
    } catch(std::exception& e) { };
    if(!inmsg_ptr) {
        logger.msg(Arc::ERROR, "input is not SOAP");
        return make_fault(outmsg);
    };
    if(!outmsg_ptr) {
        logger.msg(Arc::ERROR, "output is not SOAP");
        return make_fault(outmsg);
    };

    jclass JSOAPMessageClass = jenv->FindClass("nordugrid/arc/SOAPMessage");
    if (JSOAPMessageClass == NULL) {
        return java_error(jenv, "Cannot find SOAPMessage object");
    }
    /* Get the constructor of java object */
    jmethodID constructorID = jenv->GetMethodID(JSOAPMessageClass, "<init>", "(I)V");
    if (constructorID == NULL) {
        return java_error(jenv, "Cannot find constructor function of message");
    }
    /* Convert C++ object to Java objects */
    jobject jinmsg = jenv->NewObject(JSOAPMessageClass, constructorID, (jlong)((long int)inmsg_ptr));
    if (jinmsg == NULL) {
        return java_error(jenv, "Cannot convert input message to java object");
    }
    jobject joutmsg = jenv->NewObject(JSOAPMessageClass, constructorID, (jlong)((long int)outmsg_ptr));
    if (jinmsg == NULL) {
        return java_error(jenv, "Cannot convert output message to java object");
    }
    /* Create arguments for java process function */
    jvalue args[2];
    args[0].l = jinmsg;
    args[1].l = joutmsg;
    /* Call the process method of Java object */
    jobject jmcc_status = jenv->CallObjectMethodA(serviceObj, processID, args);
    if (jmcc_status == NULL) {
        return java_error(jenv, "Error in call process function of java object");
    }
    /* Get SWIG specific getCPtr function of Message class */
    jmethodID msg_getCPtrID = jenv->GetStaticMethodID(JSOAPMessageClass, "getCPtr", "(Lnordugrid/arc/SOAPMessage;)J");
    if (msg_getCPtrID == NULL) {
        return java_error(jenv, "Cannot find getCPtr method of java Message class");
    }
    /* Get Java MCC_Status class */
    jclass JMCC_StatusClass = jenv->FindClass("nordugrid/arc/MCC_Status");
    if (JMCC_StatusClass == NULL) {
        logger.msg(Arc::ERROR, "Cannot find MCC_Status object");
        /* Cleanup */
        jvm->DetachCurrentThread();
        return Arc::MCC_Status(Arc::GENERIC_ERROR);
    }
    /* Get SWIG specific getCPtr function of MCC_Status class */
    jmethodID mcc_status_getCPtrID = jenv->GetStaticMethodID(JMCC_StatusClass, "getCPtr", "(Lnordugrid/arc/MCC_Status;)J");
    if (mcc_status_getCPtrID == NULL) {
        return java_error(jenv, "Cannot find getCPtr method of java MCC_Status class");
    }
    
    /* Convert Java status object to C++ class */
    jlong mcc_status_addr = jenv->CallStaticLongMethod(JMCC_StatusClass, mcc_status_getCPtrID, jmcc_status);
    if (!mcc_status_addr) {
        logger.msg(ERROR, "Java object returned NULL status");
        return MCC_Status(GENERIC_ERROR);
    }
    Arc::MCC_Status status(*((Arc::MCC_Status *)(long)mcc_status_addr));
    /* Convert Java output message object to C++ class */
    jlong outmsg_addr = jenv->CallStaticLongMethod(JSOAPMessageClass, msg_getCPtrID, joutmsg);
     
    Arc::SOAPMessage *outmsg_ptr2 = (Arc::SOAPMessage *)(long)outmsg_addr;
    /* std::string xml;
    outmsg_ptr2->Payload()->GetXML(xml);   
    std::cout << xml << std::endl; */
    Arc::PayloadSOAP *pl = new Arc::PayloadSOAP(*(outmsg_ptr2->Payload()));
    outmsg.Payload((MessagePayload *)pl);
    // XXX: how to handle error?
    /* Detach from the Java engine */
    jvm->DetachCurrentThread();
    return status;
}

} // namespace Arc
