// -*- indent-tabs-mode: nil -*-

#ifndef __ARC_XMLNODE_H__
#define __ARC_XMLNODE_H__

#include <iostream>
#include <string>
#include <list>
#include <vector>
#include <map>

#include <libxml/xmlmemory.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/xmlschemas.h>

namespace Arc {

  /** \addtogroup common
   *  @{ */

  class XMLNode;

  /// Class to represent an XML namespace.
  /** \headerfile XMLNode.h arc/XMLNode.h */
  class NS
    : public std::map<std::string, std::string> {
  public:
    /// Constructor creates empty namespace
    NS(void) {}
    /// Constructor creates namespace with one entry
    NS(const char *prefix, const char *uri) {
      operator[](prefix) = uri;
    }
    /// Constructor creates namespace with multiple entries
    /** \param nslist Array made of prefix and URI pairs and must be NULL
        terminated */
    NS(const char *nslist[][2]) {
      for (int n = 0; nslist[n][0]; ++n)
        operator[](nslist[n][0]) = nslist[n][1];
    }
    /// Constructor creates namespace with multiple entries
    NS(const std::map<std::string, std::string>& nslist)
      : std::map<std::string, std::string>(nslist) {}
  };

  typedef std::list<XMLNode> XMLNodeList;

  /// Wrapper for LibXML library Tree interface.
  /** This class wraps XML Node, Document and Property/Attribute structures.
     Each instance serves as pointer to actual LibXML element and provides convenient
     (for chosen purpose) methods for manipulating it.
     This class has no special ties to LibXML library and may be easily rewritten for any XML
     parser which provides interface similar to LibXML Tree.
     It implements only small subset of XML capabilities, which is probably enough for performing
     most of useful actions. This class also filters out (usually) useless textual nodes which
     are often used to make XML documents human-readable.
     \headerfile XMLNode.h arc/XMLNode.h */
  class XMLNode {
    friend bool MatchXMLName(const XMLNode& node1, const XMLNode& node2);
    friend bool MatchXMLName(const XMLNode& node, const char *name);
    friend bool MatchXMLName(const XMLNode& node, const std::string& name);
    friend bool MatchXMLNamespace(const XMLNode& node1, const XMLNode& node2);
    friend bool MatchXMLNamespace(const XMLNode& node, const char *uri);
    friend bool MatchXMLNamespace(const XMLNode& node, const std::string& uri);
    friend class XMLNodeContainer;

  protected:
    xmlNodePtr node_;
    /// If true node is owned by this instance - hence released in destructor.
    /** Normally that may be true only for top level node of XML document. */
    bool is_owner_;
    /// This variable is reserved for future use.
    bool is_temporary_;
    /// Protected constructor for inherited classes.
    /** Creates instance and links to existing LibXML structure. Acquired structure is
       not owned by class instance. If there is need to completely pass control of
       LibXML document to then instance's is_owner_ variable has to be set to true.
     */
    XMLNode(xmlNodePtr node)
      : node_(node),
        is_owner_(false),
        is_temporary_(false) {}

    /** printf-like callback for libxml */
    static void LogError(void * ctx, const char * msg, ...);

    /** Convenience method for XML validation */
    bool Validate(xmlSchemaPtr schema, std::string &err_msg);

  public:
    /// Constructor of invalid node.
    /** Created instance does not point to XML element. All methods are still allowed
       for such instance but produce no results. */
    XMLNode(void)
      : node_(NULL),
        is_owner_(false),
        is_temporary_(false) {}
    /// Copies existing instance.
    /** Underlying XML element is NOT copied. Ownership is NOT inherited.
       Strictly speaking there should be no const here - but that conflicts
       with C++. */
    XMLNode(const XMLNode& node)
      : node_(node.node_),
        is_owner_(false),
        is_temporary_(false) {}
    /// Creates XML document structure from textual representation of XML document.
    /** Created structure is pointed and owned by constructed instance. */
    XMLNode(const std::string& xml);
    /// Creates XML document structure from textual representation of XML document.
    /** Created structure is pointed and owned by constructed instance. */
    XMLNode(const char *xml, int len = -1);
    /// Copy constructor. Used by language bindings
    XMLNode(long ptr_addr);
    /// Creates empty XML document structure with specified namespaces.
    /** Created XML contains only root element named 'name'.
       Created structure is pointed and owned by constructed instance. */
    XMLNode(const NS& ns, const char *name);
    /// Destructor
    /** Also destroys underlying XML document if owned by this instance */
    ~XMLNode(void);
    /// Creates a copy of XML (sub)tree.
    /** If object does not represent whole document - top level document
       is created. 'node' becomes a pointer owning new XML document. */
    void New(XMLNode& node) const;
    /// Exchanges XML (sub)trees.
    /** The following combinations are possible:
       - If both this and node are referring owned XML tree (top level
         node) then references are simply exchanged. This operation is fast.
       - If both this and node are referring to XML (sub)tree of different
         documents then (sub)trees are exchanged between documents.
       - If both this and node are referring to XML (sub)tree of same
         document then (sub)trees are moved inside document.

     The main reason for this method is to provide an effective way to insert
     one XML document inside another. One should take into account that if any
     of the exchanged nodes is top level it must be also the owner of the
     document. Otherwise this method will fail. If both nodes are top level
     owners and/or invalid nodes then this method is identical to Swap(). */
    void Exchange(XMLNode& node);
    /// Moves content of this XML (sub)tree to node.
    /** This operation is similar to New() except that XML (sub)tree to
      referred by this is destroyed. This method is more effective than
      combination of New() and Destroy() because internally it is optimized
      not to copy data if not needed. The main purpose of this is to
      effectively extract part of XML document. */
    void Move(XMLNode& node);
    /// Swaps XML (sub)trees to which this and node refer.
    /** For XML subtrees this method is not anyhow different then using the
      combination
      \code
      XMLNode tmp=*this; *this=node; node=tmp;
      \endcode
      But in case of either this or node owning XML document ownership
      is swapped too. And this is the main purpose of this method. */
    void Swap(XMLNode& node);
    /// Returns true if instance points to XML element - valid instance.
    operator bool(void) const {
      return ((node_ != NULL) && (!is_temporary_));
    }
    /// Returns true if instance does not point to XML element - invalid instance.
    bool operator!(void) const {
      return ((node_ == NULL) || is_temporary_);
    }
    /// Returns true if 'node' represents same XML element.
    bool operator==(const XMLNode& node) {
      return ((node_ == node.node_) && (node_ != NULL));
    }
    /// Returns false if 'node' represents same XML element.
    bool operator!=(const XMLNode& node) {
      return ((node_ != node.node_) || (node_ == NULL));
    }
    /// Returns true if 'node' represents same XML element - for bindings.
    bool Same(const XMLNode& node) {
      return operator==(node);
    }
    /// This operator is needed to avoid ambiguity.
    bool operator==(bool val) {
      return ((bool)(*this) == val);
    }
    /// This operator is needed to avoid ambiguity.
    bool operator!=(bool val) {
      return ((bool)(*this) != val);
    }
    /// This operator is needed to avoid ambiguity.
    bool operator==(const std::string& str) {
      return ((std::string)(*this) == str);
    }
    /// This operator is needed to avoid ambiguity.
    bool operator!=(const std::string& str) {
      return ((std::string)(*this) != str);
    }
    /// This operator is needed to avoid ambiguity.
    bool operator==(const char *str) {
      return ((std::string)(*this) == str);
    }
    /// This operator is needed to avoid ambiguity.
    bool operator!=(const char *str) {
      return ((std::string)(*this) != str);
    }
    /// Returns XMLNode instance representing n-th child of XML element.
    /** If such does not exist invalid XMLNode instance is returned */
    XMLNode Child(int n = 0);
    /// Returns XMLNode instance representing first child element with specified name.
    /** Name may be "namespace_prefix:name", "namespace_uri:name" or
       simply "name". In last case namespace is ignored. If such node 
       does not exist invalid XMLNode instance is returned.
       This method should not be marked const because obtaining 
       unrestricted XMLNode of child element allows modification
       of underlying XML tree. But in order to keep const in other
       places non-const-handling is passed to programmer. Otherwise
       C++ compiler goes nuts. */
    XMLNode operator[](const char *name) const;
    /// Returns XMLNode instance representing first child element with specified name.
    /** Similar to operator[](const char *name) const. */
    XMLNode operator[](const std::string& name) const {
      return operator[](name.c_str());
    }
    /// Returns XMLNode instance representing n-th node in sequence of siblings of same name.
    /** Its main purpose is to be used to retrieve an element in an array of
       children of the same name like node["name"][5].

       This method should not be marked const because obtaining
       unrestricted XMLNode of child element allows modification
       of underlying XML tree. But in order to keep const in other
       places non-const-handling is passed to programmer. Otherwise
       C++ compiler goes nuts. */
    XMLNode operator[](int n) const;
    /// Convenience operator to switch to next element of same name.
    /** If there is no such node this object becomes invalid. */
    void operator++(void);
    /// Convenience operator to switch to previous element of same name.
    /** If there is no such node this object becomes invalid. */
    void operator--(void);
    /// Returns number of children nodes.
    int Size(void) const;
    /// Same as operator[]().
    XMLNode Get(const std::string& name) const {
      return operator[](name.c_str());
    }
    /// Returns name of XML node.
    std::string Name(void) const;
    /// Returns namespace prefix of XML node.
    std::string Prefix(void) const;
    /// Returns prefix:name of XML node.
    std::string FullName(void) const {
      return Prefix() + ":" + Name();
    }
    /// Returns namespace URI of XML node.
    std::string Namespace(void) const;
    /// Assigns namespace prefix to XML node(s).
    /** The 'recursion' allows to assign prefixes recursively.
       Setting it to -1 allows for unlimited recursion. And 0
       limits it to this node. */
    void Prefix(const std::string& prefix, int recursion = 0);
    /// Removes namespace prefix from XML node(s).
    void StripNamespace(int recursion = 0);
    /// Assigns new name to XML node.
    void Name(const char *name);
    /// Assigns new name to XML node.
    void Name(const std::string& name) {
      Name(name.c_str());
    }
    /// Fills argument with this instance XML subtree textual representation.
    void GetXML(std::string& out_xml_str, bool user_friendly = false) const;
    /// Get string representation of XML subtree.
    /** Fills out_xml_str with this instance XML subtree textual representation
       if the XML subtree corresponds to the encoding format specified in the
       argument, e.g. utf-8. */
    void GetXML(std::string& out_xml_str, const std::string& encoding, bool user_friendly = false) const;
    /// Fills out_xml_str with whole XML document textual representation.
    void GetDoc(std::string& out_xml_str, bool user_friendly = false) const;
    /// Returns textual content of node excluding content of children nodes.
    operator std::string(void) const;
    /// Sets textual content of node. All existing children nodes are discarded.
    XMLNode& operator=(const char *content);
    /// Sets textual content of node. All existing children nodes are discarded.
    XMLNode& operator=(const std::string& content) {
      return operator=(content.c_str());
    }
    /// Same as operator=. Used for bindings.
    void Set(const std::string& content) {
      operator=(content.c_str());
    }
    /// Make instance refer to another XML node. Ownership is not inherited.
    /** Due to nature of XMLNode there should be no const here, but that
        does not fit into C++. */
    XMLNode& operator=(const XMLNode& node);
    // Returns list of all attributes of node
    // std::list<XMLNode> Attributes(void);
    /// Returns XMLNode instance reresenting n-th attribute of node.
    XMLNode Attribute(int n = 0);
    /// Returns XMLNode instance representing first attribute of node with specified by name.
    XMLNode Attribute(const char *name);
    /// Returns XMLNode instance representing first attribute of node with specified by name.
    XMLNode Attribute(const std::string& name) {
      return Attribute(name.c_str());
    }
    /// Creates new attribute with specified name.
    XMLNode NewAttribute(const char *name);
    /// Creates new attribute with specified name.
    XMLNode NewAttribute(const std::string& name) {
      return NewAttribute(name.c_str());
    }
    /// Returns number of attributes of node.
    int AttributesSize(void) const;
    /// Assigns namespaces of XML document at point specified by this instance.
    /** If namespace already exists it gets new prefix. New namespaces are added.
       It is useful to apply this method to XML being processed in order to
       refer to it's elements by known prefix.
       If keep is set to false existing namespace definition residing at
       this instance and below are removed (default behavior).
       If recursion is set to positive number then depth of prefix replacement
       is limited by this number (0 limits it to this node only). For unlimited
       recursion use -1. If recursion is limited then value of keep is ignored
       and existing namespaces are always kept.  */
    void Namespaces(const NS& namespaces, bool keep = false, int recursion = -1);
    /// Returns namespaces known at this node.
    NS Namespaces(void);
    /// Returns prefix of specified namespace or empty string if no such namespace.
    std::string NamespacePrefix(const char *urn);
    /// Creates new child XML element at specified position with specified name.
    /** Default is to put it at end of list. If global_order is true position
       applies to whole set of children, otherwise only to children of same name.
       Returns created node. */
    XMLNode NewChild(const char *name, int n = -1, bool global_order = false);
    /// Same as NewChild(const char*,int,bool).
    XMLNode NewChild(const std::string& name, int n = -1, bool global_order = false) {
      return NewChild(name.c_str(), n, global_order);
    }
    /// Creates new child XML element at specified position with specified name and namespaces.
    /** For more information look at NewChild(const char*,int,bool) */
    XMLNode NewChild(const char *name, const NS& namespaces, int n = -1, bool global_order = false);
    /// Same as NewChild(const char*,const NS&,int,bool).
    XMLNode NewChild(const std::string& name, const NS& namespaces, int n = -1, bool global_order = false) {
      return NewChild(name.c_str(), namespaces, n, global_order);
    }
    /// Link a copy of supplied XML node as child.
    /** Returns instance referring to new child. XML element is a copy of supplied
       one but not owned by returned instance */
    XMLNode NewChild(const XMLNode& node, int n = -1, bool global_order = false);
    /// Makes a copy of supplied XML node and makes this instance refer to it.
    void Replace(const XMLNode& node);
    /// Destroys underlying XML element.
    /** XML element is unlinked from XML tree and destroyed.
       After this operation XMLNode instance becomes invalid */
    void Destroy(void);
    /// Collects nodes corresponding to specified path.
    /** This is a convenience function to cover common use of XPath but
       without performance hit. Path is made of node_name[/node_name[...]]
       and is relative to current node. node_names are treated in same way
       as in operator[].
       \return all nodes which are represented by path. */
    XMLNodeList Path(const std::string& path);
    /// Uses XPath to look up XML tree.
    /** Returns a list of XMLNode points. The xpathExpr should be like
       "//xx:child1/" which indicates the namespace and node that you
       would like to find. The nsList contains namespaces used by the 
       xpathExpr. 
       Query is run on whole XML document but only the elements belonging
       to this XML subtree are returned.
       Please note, that default namespaces - without prefix - are not 
       fully supported. So xpathExpr without properly defined namespace
       prefixes will only work for XML documents without namespaces. */
    XMLNodeList XPathLookup(const std::string& xpathExpr, const NS& nsList);
    /// Get the root node from any child node of the tree.
    XMLNode GetRoot(void);
    /// Get the parent node from any child node of the tree.
    XMLNode Parent(void);
    /// Save string representation of node to file.
    bool SaveToFile(const std::string& file_name) const;
    /// Save string representation of node to stream.
    bool SaveToStream(std::ostream& out) const;
    /// Read XML document from file and associate it with this node.
    bool ReadFromFile(const std::string& file_name);
    /// Read XML document from stream and associate it with this node.
    bool ReadFromStream(std::istream& in);
    // Remove all eye-candy information leaving only informational parts
    //   void Purify(void);.
    /// XML schema validation against the schema file defined as argument.
    bool Validate(const std::string &schema_file, std::string &err_msg);
    /** XML schema validation against the schema XML document defined as argument */
    bool Validate(XMLNode schema_doc, std::string &err_msg);
  };

  /// Write XMLNode to output stream.
  std::ostream& operator<<(std::ostream& out, const XMLNode& node);
  /// Read into XMLNode from input stream.
  std::istream& operator>>(std::istream& in, XMLNode& node);

  /// Container for multiple XMLNode elements.
  /** \headerfile XMLNode.h arc/XMLNode.h */
  class XMLNodeContainer {
  private:
    std::vector<XMLNode*> nodes_;
  public:
    /// Default constructor.
    XMLNodeContainer(void);
    /// Copy constructor.
    /** Add nodes from argument. Nodes owning XML document are
       copied using AddNew(). Not owning nodes are linked using
       Add() method. */
    XMLNodeContainer(const XMLNodeContainer&);
    ~XMLNodeContainer(void);
    /// Same as copy constructor with current nodes being deleted first.
    XMLNodeContainer& operator=(const XMLNodeContainer&);
    /// Link XML subtree refered by node to container.
    /** XML tree must be available as long as this object is used. */
    void Add(const XMLNode&);
    /// Link multiple XML subtrees to container.
    void Add(const std::list<XMLNode>&);
    /// Copy XML subtree referenced by node to container.
    /** After this operation container refers to independent XML
       document. This document is deleted when container is destroyed. */
    void AddNew(const XMLNode&);
    /// Copy multiple XML subtrees to container.
    void AddNew(const std::list<XMLNode>&);
    /// Return number of refered/stored nodes.
    int Size(void) const;
    /// Returns n-th node in a store.
    XMLNode operator[](int);
    /// Returns all stored nodes.
    std::list<XMLNode> Nodes(void);
  };

  /// Returns true if underlying XML elements have same names.
  bool MatchXMLName(const XMLNode& node1, const XMLNode& node2);

  /// Returns true if 'name' matches name of 'node'. If name contains prefix it's checked too.
  bool MatchXMLName(const XMLNode& node, const char *name);

  /// Returns true if 'name' matches name of 'node'. If name contains prefix it's checked too.
  bool MatchXMLName(const XMLNode& node, const std::string& name);

  /// Returns true if underlying XML elements belong to same namespaces.
  bool MatchXMLNamespace(const XMLNode& node1, const XMLNode& node2);

  /// Returns true if 'namespace' matches 'node's namespace..
  bool MatchXMLNamespace(const XMLNode& node, const char *uri);

  /// Returns true if 'namespace' matches 'node's namespace..
  bool MatchXMLNamespace(const XMLNode& node, const std::string& uri);

  /** @} */

} // namespace Arc

#endif /* __ARC_XMLNODE_H__ */
