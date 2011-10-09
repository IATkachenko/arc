/**
 * Note that the order of the "%include" statements are important! If a
 * "%include" depends on other "%include"s, it should be placed after these
 * "%include" dependencies.
 */

%{
#include <arc/data/DataStatus.h>
#include <arc/data/FileInfo.h>
#include <arc/data/URLMap.h>
#include <arc/data/DataPoint.h>
#include <arc/data/DataHandle.h>
#include <arc/data/DataSpeed.h>
#include <arc/data/DataBuffer.h>
#include <arc/data/FileCache.h>
#include <arc/data/DataMover.h>

#ifdef WIN32
#define uid_t int
#define gid_t int
#endif

%}

#ifdef SWIGPYTHON
%{
namespace Arc {

typedef struct {
    bool result;
    int handle;
    unsigned int size;
    unsigned long long int offset;
    char* buffer;
} DataBufferForWriteResult;

typedef struct {
    bool result;
    int handle;
    unsigned int size;
} DataBufferForReadResult;

}
%}

namespace Arc {

/* this typemap tells SWIG that we don't want to use the 'std::list<FileInfo>& files' argument
from the target language, but we need a temporary variable for internal use,
and we want this argument to point to this temporary variable */
%typemap(in, numinputs=0) std::list<FileInfo> & files (std::list<FileInfo> temp) {
    $1 = &temp;
}

/* this typemap tells SWIG what we want to do with the 'std::list<FileInfo> & files'
argument after the method finished:
first we create a python list from the std::list (it should be a better way to do this...)
then we want to return a tuple with two members, the first member will be the newly created list,
and the second member is the original return value, the DataStatus. */
%typemap(argout) std::list<FileInfo> & files {
    PyObject *o, *tuple;
    o = PyList_New(0);
    std::list<Arc::FileInfo>::iterator it;
    for (it = (*$1).begin(); it != (*$1).end(); ++it) {
        PyList_Append(o, SWIG_NewPointerObj(new Arc::FileInfo(*it), SWIGTYPE_p_Arc__FileInfo, SWIG_POINTER_OWN | 0 ));
    }
    tuple = PyTuple_New(2);
    PyTuple_SetItem(tuple,0,o);
    PyTuple_SetItem(tuple,1,$result);
    $result = tuple;
}

%typemap(out) Arc::DataBufferForWriteResult {
    $result = PyTuple_New(5);
    PyTuple_SetItem($result,0,PyInt_FromLong($1.result));
    PyTuple_SetItem($result,1,PyInt_FromLong($1.handle));
    PyTuple_SetItem($result,2,PyInt_FromLong($1.size));
    PyTuple_SetItem($result,3,PyInt_FromLong($1.offset));
    PyTuple_SetItem($result,4,$1.buffer?PyString_FromStringAndSize($1.buffer,$1.size):Py_None);
}

%typemap(out) Arc::DataBufferForReadResult {
    $result = PyTuple_New(3);
    PyTuple_SetItem($result,0,PyInt_FromLong($1.result));
    PyTuple_SetItem($result,1,PyInt_FromLong($1.handle));
    PyTuple_SetItem($result,2,PyInt_FromLong($1.size));
}

%typemap(in) (char* DataBufferIsReadBuf,unsigned int DataBufferIsReadSize) {
    $1 = PyString_AsString($input);
    $2 = ($1)?PyString_Size($input):0;
}

%extend DataBuffer {

    Arc::DataBufferForWriteResult for_write(bool wait) {
        Arc::DataBufferForWriteResult r;
        r.result = $self->for_write(r.handle,r.size,r.offset,wait);
        r.buffer = r.result?($self->operator[](r.handle)):NULL;
        return r;
    }
    
    Arc::DataBufferForReadResult for_read(bool wait) {
        Arc::DataBufferForReadResult r;
        r.result = $self->for_read(r.handle,r.size,wait);
        return r;
    }

    bool is_read(int handle,char* DataBufferIsReadBuf,unsigned int DataBufferIsReadSize,unsigned long long int offset) {
        char* buf = $self->operator[](handle);
        if(!buf) return false;
        if(DataBufferIsReadSize > $self->buffer_size()) return false;
        memcpy(buf,DataBufferIsReadBuf,DataBufferIsReadSize);
        return $self->is_read(handle,DataBufferIsReadSize,offset);
    }

};

}

%ignore Arc::DataHandle::operator->;
%ignore Arc::DataBuffer::operator[];
%ignore Arc::DataBuffer::for_write(int&,unsigned int&,unsigned long long int&,bool);
%ignore Arc::DataBuffer::for_read(int&,unsigned int&,bool);
%ignore Arc::DataBuffer::is_read(int,unsigned int,unsigned long long int);
%ignore Arc::DataBuffer::is_read(char*,unsigned int,unsigned long long int);
%ignore Arc::DataBuffer::is_written(char*);
%ignore Arc::DataBuffer::is_notwritten(char*);

#endif

#ifdef SWIGJAVA
%ignore Arc::DataHandle::operator->;
%ignore Arc::DataHandle::operator*;
#endif


%include "../src/hed/libs/data/DataStatus.h"
%include "../src/hed/libs/data/FileInfo.h"
%include "../src/hed/libs/data/URLMap.h"
%include "../src/hed/libs/data/DataPoint.h"
%include "../src/hed/libs/data/DataHandle.h"
%include "../src/hed/libs/data/DataSpeed.h"
%include "../src/hed/libs/data/DataBuffer.h"
%include "../src/hed/libs/data/FileCache.h"
%include "../src/hed/libs/data/DataMover.h"
