/*-----------------------------------------------------------------------------
Copyright(c) 2010 - 2018 ViSUS L.L.C.,
Scientific Computing and Imaging Institute of the University of Utah

ViSUS L.L.C., 50 W.Broadway, Ste. 300, 84101 - 2044 Salt Lake City, UT
University of Utah, 72 S Central Campus Dr, Room 3750, 84112 Salt Lake City, UT

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met :

* Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

For additional information about this project contact : pascucci@acm.org
For support : support@visus.net
-----------------------------------------------------------------------------*/

#ifndef VISUS_STRINGTREE_H__
#define VISUS_STRINGTREE_H__

#include <Visus/Kernel.h>
#include <Visus/StringMap.h>
#include <Visus/StringMap.h>
#include <Visus/StringUtils.h>

#include <stack>
#include <vector>
#include <iostream>
#include <vector>
#include <functional>

namespace Visus {

///////////////////////////////////////////////////////////////////////
class VISUS_KERNEL_API StringTree
{
public:

  VISUS_CLASS(StringTree)

  //name
  String name;

  //attributes
  std::vector< std::pair<String, String> > attributes;

  // constructor
  StringTree() {
  }

  // constructor
  explicit StringTree(String name_ ) : name(name_) {
  }

  //copy constructor
  StringTree(const StringTree& other) {
    operator=(other);
  }

  //recursive
#if !SWIG
  template <typename Value, typename... Args>
  explicit StringTree(String name, String key, Value value, Args&&... args) : StringTree(name) {
    write(key, value);
    for (auto it : StringTree(name, std::forward<Args>(args)...).attributes)
      write(it.first, it.second);
  }
#endif

  //destructor
  ~StringTree() {
  }

  //valid
  bool valid() const {
    return !name.empty();
  }

  //bool()
  operator bool() const {
    return valid();
  }

  //fromString
  static StringTree fromString(String content, bool bEnablePostProcessing = true);

  //operator=
  StringTree& operator=(const StringTree& other) {
    this->name = other.name;
    this->attributes = other.attributes;
    this->childs.clear();
    for (auto it : other.getChilds())
      this->childs.push_back(std::make_shared<StringTree>(*it));
    return *this;
  }

  //hasAttribute
  bool hasAttribute(String name) const
  {
    for (int I = 0; I < attributes.size(); I++) {
      if (attributes[I].first == name)
        return true;
    }
    return false;
  }

  //getAttribute
  String getAttribute(String name, String default_value = "") const
  {
    for (int I = 0; I < this->attributes.size(); I++) {
      if (attributes[I].first == name)
        return attributes[I].second;
    }
    return default_value;
  }

  //setAttribute
  void setAttribute(String name, String value)
  {
    for (int I = 0; I < this->attributes.size(); I++) {
      if (attributes[I].first == name) {
        attributes[I].second = value;
        return;
      }
    }
    attributes.push_back(std::make_pair(name, value));
  }

  //removeAttribute
  void removeAttribute(String name)
  {
    for (auto it = attributes.begin(); it != attributes.end(); it++) {
      if (it->first == name) {
        attributes.erase(it);
        return;
      }
    }
  }

public:

  //getNumberOfChilds
  int getNumberOfChilds() const {
    return (int)childs.size();
  }

  //getChilds
  const std::vector< SharedPtr<StringTree> >& getChilds() const {
    return childs;
  }

  //addChild
  StringTree& addChild(SharedPtr<StringTree> child) {
    childs.push_back(child);
    return *this;
  }

  //addChildAtBegin
  StringTree& addChildAtBegin(SharedPtr<StringTree> child) {
    childs.insert(childs.begin(), child);
    return *this;
  }

#if !SWIG
  //addChild
  StringTree& addChild(const StringTree& child) {
    return addChild(std::make_shared<StringTree>(child));
  }
#endif

  //addChild
  SharedPtr<StringTree> addChild(String name) {
    auto child = std::make_shared<StringTree>(name);
    NormalizeW(this, name)->addChild(child);
    return child;
  }

  //getChild
  SharedPtr<StringTree> getChild(int I) const {
    return childs[I];
  }

  //getFirstChild
  SharedPtr<StringTree> getFirstChild() const {
    return childs.front();
  }

  //getFirstChild
  SharedPtr<StringTree> getLastChild() const {
    return childs.back();
  }

  //getChild
  SharedPtr<StringTree> getChild(String name) const;

  //getChild
  std::vector< SharedPtr<StringTree> > getChilds(String name) const;

  //getAllChilds
  std::vector<StringTree*> getAllChilds(String name) const;

  //removeChild
  void removeChild(String name) {
    auto child = getChild(name);
    if (!child) return;
    auto it = std::find(this->childs.begin(), this->childs.end(), child);
    if (it == this->childs.end()) return;
    this->childs.erase(it);
  }

public:

  //isHash
  bool isHash() const {
    return !name.empty() && name[0] == '#';
  }

  //isComment
  bool isComment() const {
    return name == "#comment";
  }

  //isText
  bool isText() const {
    return name == "#text";
  }

  //isCData
  bool isCData() const {
    return name == "#cdata-section";
  }

  //addComment
  void addComment(String value) {
    childs.push_back(std::make_shared<StringTree>(StringTree("#comment").write("value", value)));
  }

  //addText
  void addText(const String& value) {
    childs.push_back(std::make_shared<StringTree>(StringTree("#text").write("value", value)));
  }

  //addCData
  void addCData(const String& value) {
    childs.push_back(std::make_shared<StringTree>(StringTree("#cdata-section").write("value", value)));
  }

public:

  //write
  StringTree& write(String key, String value);

  //write
  StringTree& write(String key, const char* value) {
    return write(key, String(value));
  }

  //write
  StringTree& write(String key, bool value) {
    return write(key, cstring(value));
  }

  //write
  StringTree& write(String key, int value) {
    return write(key, cstring(value));
  }

  //write
  StringTree& write(String key, Int64 value) {
    return write(key, cstring(value));
  }

  //write
  StringTree& write(String key, double value) {
    return write(key, cstring(value));
  }

  //write
#if !SWIG
  StringTree& write(String key, const std::vector<int>& values) {
    return writeText(key, StringUtils::join(values));
  }
#endif

  //write
  StringTree& write(String key, const std::vector<double>& values) {
    return writeText(key, StringUtils::join(values));
  }

  //write
  template <class Value>
  StringTree& write(String key, const Value& value) {
    return write(key, value.toString());
  }

  //writeIfNotDefault
  template <class Value>
  StringTree& writeIfNotDefault (String key, const Value& value, const Value& default_value)
  {
    if (value!= default_value) write(key, value);
    return *this;
  }

public:

  //read
  const StringTree& read(String key, String& value, String default_value="") const
  {
    VisusAssert(!key.empty());
    auto cursor = NormalizeR(this, key);
    value = cursor? cursor->getAttribute(key, default_value) : default_value;
    return *this;
  }

  //read
  const StringTree& read(const char* key, String& value, String default_value = "") const {
    read(String(key), value, default_value);
    return *this;
  }

  //read
  const StringTree& read(String key, bool& value, bool default_value=false) const {
    auto cursor = NormalizeR(this, key);
    value = cursor && cursor->hasAttribute(key) ? cbool(cursor->getAttribute(key)) : default_value;
    return *this;
  }

  //read
  const StringTree& read(String key, int& value, int default_value=0) const {
    auto cursor = NormalizeR(this, key);
    value = cursor && cursor->hasAttribute(key) ? cint(cursor->getAttribute(key)) : default_value;
    return *this;
  }

  //read(
  const StringTree& read(String key, Int64& value, Int64 default_value=0) const {
    auto cursor = NormalizeR(this, key);
    value = cursor && cursor->hasAttribute(key) ? cint64(cursor->getAttribute(key)) : default_value;
    return *this;
  }

  //read
  const StringTree& read(String key, double& value, double default_value=0.0) const {
    auto cursor = NormalizeR(this, key);
    value = cursor && cursor->hasAttribute(key) ? cdouble(cursor->getAttribute(key)) : default_value;
    return *this;
  }

  //read
  template <class Value>
  const StringTree& read(String key, Value& value, Value default_value=Value()) const {
    auto cursor = NormalizeR(this, key);
    value = cursor && cursor->hasAttribute(key) ? Value::fromString(cursor->getAttribute(key)) : default_value;
    return *this;
  }

  //read
  const StringTree& read(String key, std::vector<int>& values) const {
    values.clear();
    String text; 
    readText(key,text);
    values = StringUtils::parseInts(text);
    return *this;
  }

  //read
  const StringTree& read(String key, std::vector<double>& values) const {
    values.clear();
    String text; 
    readText(key, text);
    values=StringUtils::parseDoubles(text);
    return *this;
  }

public:

  //readString
  String readString(String key, String default_value = "") const {

    VisusAssert(!key.empty());
    auto cursor = NormalizeR(this, key);
    return cursor? cursor->getAttribute(key, default_value) : default_value;
  }

  //readInt
  bool readBool(String key, bool default_value = false) const {
    return cbool(readString(key, cstring(default_value)));
  }

  //readInt
  int readInt(String key, int default_value = 0) const {
    return cint(readString(key, cstring(default_value)));
  }

  //readInt64
  Int64 readInt64(String key, Int64 default_value = 0) const {
    return cint64(readString(key, cstring(default_value)));
  }

  //readDouble
  double readDouble(String key, double default_value = 0) const {
    return cdouble(readString(key, cstring(default_value)));
  }

public:

  //writeText
  StringTree& writeText(const String& text, bool bCData=false) {
    bCData ? addCData(text) : addText(text);
    return *this;
  }

  //writeText
  StringTree& writeText(String name, const String& value, bool bCData) {
    NormalizeW(this, name)->addChild(name)->writeText(value, bCData); return *this;
  }


  //writeText
  StringTree& writeText(String name, const String& value) {
    bool bCData = StringUtils::containsControl(value);
    NormalizeW(this, name)->addChild(name)->writeText(value, bCData); return *this;
  }

  //readText
  void readText(String& value) const;

  //readTextInline
  String readTextInline() const {
    String value;
    readText(value);
    return value;
  }


  //readText
  void readText(String name, String& value) const {
    auto child = getChild(name); 
    if (!child) return;
    child->readText(value);
  }


  //merge
  static void merge(StringTree& dst, StringTree& src) {
    for (auto it : src.attributes) 
      if (!dst.hasAttribute(it.first))
        dst.setAttribute(it.first, it.second);

    for (auto it : src.getChilds())
      dst.addChild(*it);
  }

public:

  //readObject
  template <class Object>
  bool readObject(String name, Object& obj)
  {
    auto child = getChild(name);
    if (!child) return false;
    obj.read(*child);
    return true;
  }

  //writeObject
  template <class Object>
  StringTree& writeObject(String name, Object& obj) {
    obj.write(*NormalizeW(this, name)->addChild(name)); return *this;
  }

public:

  //internal use only
  static StringTree postProcess(const StringTree& src);

  //toXmlString
  String toXmlString() const;

  //toJSONString
  String toJSONString() const {
    return toJSONString(*this, 0);
  }

  //toString
  String toString() const {
    return toXmlString();
  }

private:

  //childs
  std::vector< SharedPtr<StringTree> > childs;

  //toJSONString
  static String toJSONString(const StringTree& stree, int nrec);

  //NormalizeR (normlize for reading, return null if does not exist)
  static const StringTree* NormalizeR(const StringTree* cursor, String& key);

  //NormalizeW (normalize for writing, create the node if does not exist)
  static StringTree* NormalizeW(StringTree* cursor, String& key);

}; //end class


typedef StringTree Archive;
                  

//////////////////////////////////////////////////////////////////////
class VISUS_KERNEL_API ConfigFile : public StringTree
{
public:

  //constructor
  ConfigFile(String name = "ConfigFile") : StringTree(name) {
  }

  //constructor
  ConfigFile(const StringTree& tree) : StringTree(tree) {
  }

  //destructor
  ~ConfigFile() {
  }

  //fromString
  static ConfigFile fromString(String content, bool bEnablePostProcessing = true) {
    return ConfigFile(StringTree::fromString(content, bEnablePostProcessing));
  }
   
  //getFilename
  String getFilename() const {
    return filename;
  }

  //load
  bool load(String filename, bool bEnablePostProcessing = true);

  //reload
  bool reload(bool bEnablePostProcessing = true) {
    return load(filename, bEnablePostProcessing);
  }

  //save
  void save();


private:

  String filename;

};

//////////////////////////////////////////////////////////////////////
#if !SWIG
namespace Private {
class VISUS_KERNEL_API VisusConfig : public ConfigFile
{
public:

  VISUS_DECLARE_SINGLETON_CLASS(VisusConfig)

  //constructor
  VisusConfig() : ConfigFile("visus_config") {
  }
};
} //namespace Private
#endif


} //namespace Visus
 
#endif //VISUS_STRINGTREE_H__


