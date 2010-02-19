#ifndef __ARC_XML_CONTAINER_H__
#define __ARC_XML_CONTAINER_H__

#include <string>
#include <vector>
#include <db_cxx.h>
#include <arc/XMLNode.h>
#include <arc/Logger.h>

namespace Arc
{

class XmlContainer
{
    private:
        DbEnv *env_;
        Db *db_;
        DbTxn *update_tid_;
        Arc::Logger logger_;

    public:
        XmlContainer(const std::string &db_path, const std::string &db_name);
        ~XmlContainer();
        bool put(const std::string &name, const std::string &content);
        std::string get(const std::string &name);
        void del(const std::string &name);
        std::vector<std::string> get_doc_names();
        void start_update();
        void end_update();
        void checkpoint();
        operator bool(void) {return db_ != NULL;}
        bool operator!(void) {return db_ == NULL;}
};

}
#endif
