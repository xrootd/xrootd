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

#include <Visus/StringTree.h>
#include <Visus/StringUtils.h>
#include <Visus/Path.h>
#include <Visus/Utils.h>
#include <Visus/File.h>
#include "osdep.hxx"

#include <tinyxml/tinyxml.h>

#include <algorithm>
#include <iomanip>

namespace Visus {

//////////////////////////////////////////////////////////////////////////////////////////////////
class PostProcessStringTree
{
private:

  //acceptAlias
  /* Example:
    <alias key='server' value='atlantis.sci.utah.edu'/>
  */
  static StringMap acceptAlias(StringTree& dst,StringTree& src,std::map<String,StringTree*> templates,StringMap aliases)
  {
    String key  =src.readString("key");
    String value=resolveAliases(src.readString("value"),aliases);
    VisusAssert(!key.empty() && !value.empty());
    aliases.setValue(key,value);

    //an alias can have childs too!
    for (auto src_child : src.getChilds())
    {
      auto dst_child = std::make_shared<StringTree>();
      dst.addChild(dst_child);
      accept(*dst_child,*src_child,templates,aliases);
    }

    return aliases;
  }

  //resolveAliases
  /* Example:
    <dataset url='$(server)' />
  */
  static String resolveAliases(String value,const StringMap& aliases)
  {
    if (!StringUtils::contains(value,"$"))
      return value;

    for (auto it=aliases.begin();it!=aliases.end();it++)
      value=StringUtils::replaceAll(value,"$(" + it->first + ")",it->second);

    //VisusAssert(!StringUtils::contains(value,"$")); 
    return value;
  }

  //acceptDefineTemplate
  /* Example:
  
      <template name='myname'>
        ...
      </template>
  */
  static std::map<String,StringTree*> acceptDefineTemplate(StringTree& dst,StringTree& src,std::map<String,StringTree*> templates,StringMap aliases)
  {
    String template_name=src.readString("name"); 
    VisusAssert(!template_name.empty());
    templates[template_name]=&src;
    return templates;
  }

  //acceptCallTemplate
  /* Example:
    <call template="myname" />
  */
  static void acceptCallTemplate(StringTree& dst,StringTree& src,std::map<String,StringTree*> templates,StringMap aliases)
  {
    VisusAssert(src.getNumberOfChilds()==0); //not supported child of calls
    String template_name=src.readString("template");
    VisusAssert(!template_name.empty());
    VisusAssert(templates.find(template_name)!=templates.end());

    //expand the template corrent into dst
    for (int I=0;I<(int)templates[template_name]->getNumberOfChilds();I++) 
    {
      auto child = std::make_shared<StringTree>();
      dst.addChild(child);
      accept(*child,*templates[template_name]->getChild(I),templates,aliases);
    }
  }

  //acceptInclude
  /* Example:
    <include url="..." />
  */
  static void acceptInclude(StringTree& dst,StringTree& src,std::map<String,StringTree*> templates,StringMap aliases)
  {
    VisusAssert(src.getNumberOfChilds()==0); //<include> should not have any childs!
    String url=src.readString("url");
    VisusAssert(!url.empty());

    StringTree inplace=StringTree::fromString(Utils::loadTextDocument(url));
    if (!inplace.valid())
    {
      PrintInfo("cannot load document",url);
      VisusAssert(false);
      return;
    }

    //'explode' all childs inside the current context
    if (inplace.name == "visus")
    {
      VisusAssert(inplace.attributes.empty());

      for (auto child : inplace.getChilds())
      {
        auto dst_child = std::make_shared<StringTree>();
        dst.addChild(dst_child);
        accept(*dst_child, *child, templates, aliases);
      }
    }
    else
    {
      accept(dst, inplace, templates, aliases);
    }
  }

  //acceptIf
  /* Example: 
    
  <if condition=''>
    <then>
      ...
    </then>
    ....
    <else>
      ...
    </else>
  </if>
  */
  static void acceptIf(StringTree& dst, StringTree& src, std::map<String, StringTree*> templates, StringMap aliases)
  {
    String condition_expr = src.readString("condition"); VisusAssert(!condition_expr.empty());
    
    //TODO: mini parser here?
    bool bCondition = false;

    auto platform_name = osdep::getPlatformName();

    if      (condition_expr ==  "win"  ) bCondition = platform_name == "win";
    else if (condition_expr == "!win"  ) bCondition = platform_name != "win";
    else if (condition_expr ==  "osx"  ) bCondition = platform_name == "osx";
    else if (condition_expr == "!osx"  ) bCondition = platform_name != "osx";
    else if (condition_expr ==  "ios"  ) bCondition = platform_name == "ios";
    else if (condition_expr == "!ios"  ) bCondition = platform_name != "ios";
    else if (condition_expr ==  "linux") bCondition = platform_name == "linux";
    else if (condition_expr == "!linux") bCondition = platform_name != "linux";
    else bCondition = cbool(condition_expr);

    for (auto child : src.getChilds())
    {
      if (child->name == "then")
      {
        if (bCondition)
        {
          for (auto then_child : child->getChilds())
          {
            StringTree tmp;
            accept(tmp, *then_child, templates, aliases);
            dst.addChild(tmp);
          }
        }
      }
      else if (child->name == "else")
      {
        if (!bCondition)
        {
          for (auto else_child : child->getChilds())
          {
            StringTree tmp;
            accept(tmp, *else_child, templates, aliases);
            dst.addChild(tmp);
          }
        }
        continue;
      }
      else
      {
        VisusAssert(false);
      }


    }
  }

  //accept
  static void accept(StringTree& dst,StringTree& src,std::map<String,StringTree*> templates,StringMap aliases)
  {
    if (src.name=="alias")
    {
      aliases=acceptAlias(dst,src,templates,aliases);
      return;
    }

    if (src.name=="template")
    {
      templates=acceptDefineTemplate(dst,src,templates,aliases);
      return;
    }

    if (src.name=="call")
    {
      acceptCallTemplate(dst,src,templates,aliases);
      return;
    }

    if (src.name=="include")
    {
      acceptInclude(dst,src,templates,aliases);
      return;
    }

    if (src.name == "if")
    {
      acceptIf(dst, src, templates, aliases);
      return;
    }

    //recursive
    {
      //name
      dst.name=src.name;

      //attributes
      for (auto it : src.attributes)
      {
        String key   = it.first;
        String value = resolveAliases(it.second,aliases);
        dst.write(key,value);
      }

      //childs
      for (auto child : src.getChilds())
      {
        if (child->name=="alias")
        {
          aliases=acceptAlias(dst,*child,templates,aliases);
          continue;
        }

        if (child->name=="template")
        {
          templates=acceptDefineTemplate(dst, *child,templates,aliases);
          continue;
        }

        if (child->name=="call")
        {
          acceptCallTemplate(dst, *child,templates,aliases);
          continue;
        }

        if (child->name=="include")
        {
          acceptInclude(dst, *child,templates,aliases);
          continue;
        }

        if (child->name == "if")
        {
          acceptIf(dst, *child, templates, aliases);
          continue;
        }

        dst.addChild(StringTree());
        accept(*dst.getLastChild(), *child,templates,aliases);
      }
    }

  }

public:

  //exec
  static StringTree exec(StringTree& src)
  {
    std::map<String,StringTree*> templates;
    StringMap      aliases;
    aliases.setValue("VisusHome"              , KnownPaths::VisusHome                );
    aliases.setValue("BinaryDirectory"        , KnownPaths::BinaryDirectory          );
    aliases.setValue("CurrentWorkingDirectory", KnownPaths::CurrentWorkingDirectory());

    StringTree dst(src.name);
    accept(dst,src,templates,aliases);
    return dst;
  }

};

/////////////////////////////////////////////////
StringTree StringTree::postProcess(const StringTree& src)
{
  auto dst=PostProcessStringTree::exec(const_cast<StringTree&>(src));
  //PrintInfo(dst);
  return dst;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
const StringTree* StringTree::NormalizeR(const StringTree* cursor, String& key)
{
  if (!StringUtils::contains(key, "/"))
    return cursor;

  auto v = StringUtils::split(key, "/");

  for (int I = 0; cursor && I < (int)v.size() - 1; I++)
  {
    bool bFound = false;
    for (auto child : cursor->getChilds())
    {
      if (child->name == v[I]) {
        cursor = child.get();
        bFound = true;
        break;
      }
    }

    if (!bFound)
      cursor = nullptr;
  }

  if (!cursor)
    return nullptr;

  key = v.back();
  return cursor;
}


//////////////////////////////////////////////////////////////////////////////////////////////////
StringTree* StringTree::NormalizeW(StringTree* cursor, String& key)
{
  if (!StringUtils::contains(key, "/"))
    return cursor;

  auto v = StringUtils::split(key, "/");

  for (int I = 0; cursor && I < (int)v.size() - 1; I++)
  {
    bool bFound = false;
    for (auto child : cursor->getChilds())
    {
      if (child->name == v[I]) {
        cursor = child.get();
        bFound = true;
        break;
      }
    }

    //automatically create a new one
    if (!bFound)
    {
      auto sub_child = std::make_shared<StringTree>(v[I]);
      cursor->addChild(sub_child);
      cursor = sub_child.get();
    }
  }

  VisusAssert(cursor);
  key = v.back();
  return cursor;
}


/////////////////////////////////////////////////
void StringTree::readText(String& value) const
{
  std::ostringstream out;
  for (auto child : childs)
  {
    if (child->name=="#text")
      out<< child->readString("value");
    
    if (child->name== "#cdata-section")
      out<< child->readString("value");
  }
  value=out.str();
}


/////////////////////////////////////////////////
StringTree& StringTree::write(String key,String value)
{
  VisusAssert(!key.empty());
  auto cursor = NormalizeW(this, key);
  cursor->setAttribute(key, value);
  return *this;
}

/////////////////////////////////////////////////
SharedPtr<StringTree> StringTree::getChild(String name) const
{
  auto cursor = NormalizeR(this, name);
  if (!cursor) return SharedPtr<StringTree>();
  for (auto child : cursor->getChilds())
  {
    if (child->name == name)
      return child;
  }
  return SharedPtr<StringTree>();
}

/////////////////////////////////////////////////
std::vector< SharedPtr<StringTree> > StringTree::getChilds(String name) const {
  std::vector< SharedPtr<StringTree> > ret;
  auto cursor = NormalizeR(this, name);
  if (!cursor) return ret;
  for (auto child : cursor->getChilds())
    if (child->name == name)
      ret.push_back(child);
  return ret;
}


/////////////////////////////////////////////////
std::vector<StringTree*> StringTree::getAllChilds(String name) const
{
  std::vector<StringTree*> ret;

  for (auto child:childs)
  {
    if (name.empty() || child->name==name) 
      ret.push_back(child.get());

    auto tmp=child->getAllChilds(name);
    ret.insert(ret.end(), tmp.begin(), tmp.end());
  }
  return ret;
}


/////////////////////////////////////////////////
static TiXmlElement* ToXmlElement(const StringTree& src)
{
  TiXmlElement* dst = new TiXmlElement(src.name.c_str());

  for (auto it = src.attributes.begin(); it != src.attributes.end(); it++)
  {
    String key = it->first;
    String value = it->second;
    dst->SetAttribute(key.c_str(), value.c_str());
  }

  for (auto child : src.getChilds())
  {
    if (child->name== "#cdata-section")
    {
      VisusAssert(child->attributes.size() == 1 && child->hasAttribute("value") && child->getNumberOfChilds() == 0);
      String text = child->readString("value");
      TiXmlText * ti_xml_text = new TiXmlText(text.c_str());
      ti_xml_text->SetCDATA(true);
      dst->LinkEndChild(ti_xml_text);
    }
    else if (child->name=="#text")
    {
      VisusAssert(child->attributes.size() == 1 && child->hasAttribute("value") && child->getNumberOfChilds() == 0);
      String text = child->readString("value");
      TiXmlText * ti_xml_text = new TiXmlText(text.c_str());
      ti_xml_text->SetCDATA(false);
      dst->LinkEndChild(ti_xml_text);
    }
    else
    {
      dst->LinkEndChild(ToXmlElement(*child));
    }
  }

  return dst;
}

///////////////////////////////////////////////////////////////////////////
String StringTree::toXmlString() const
{
  TiXmlDocument* xmldoc = new TiXmlDocument();
  xmldoc->LinkEndChild(new TiXmlDeclaration("1.0", "", ""));
  xmldoc->LinkEndChild(ToXmlElement(*this));

  TiXmlPrinter printer;
  printer.SetIndent("\t");
  printer.SetLineBreak("\n");
  xmldoc->Accept(&printer);
  String ret(printer.CStr());
  delete xmldoc;

  ret = StringUtils::replaceAll(ret, "<?xml version=\"1.0\" ?>", "");
  ret = StringUtils::trim(ret);
  return ret;
}

///////////////////////////////////////////////////////////////////////////
static String FormatJSON(String str)
{
  String nStr = str;
  for (size_t i = 0; i < nStr.size(); i++)
  {
    String sreplace = "";
    bool replace = true;

    switch (nStr[i])
    {
      case '\\': sreplace = "\\\\";    break;
      case '\n': sreplace = "\\n\\\n"; break;
      case '\r': sreplace = "\\r";     break;
      case '\a': sreplace = "\\a";     break;
      case '"':  sreplace = "\\\"";    break;
      default:
      {
        int nCh = ((int)nStr[i]) & 0xFF;
        if (nCh < 32 || nCh>127)
        {
          char buffer[5];
  #if VISUS_WIN
          sprintf_s(buffer, 5, "\\x%02X", nCh);
  #else
          snprintf(buffer, 5, "\\x%02X", nCh);
  #endif
          sreplace = buffer;
        }
        else
          replace = false;
      }
    }

    if (replace)
    {
      nStr = nStr.substr(0, i) + sreplace + nStr.substr(i + 1);
      i += sreplace.length() - 1;
    }
  }
  return "\"" + nStr + "\"";
};

/////////////////////////////////////////////////
String StringTree::toJSONString(const StringTree& src, int nrec) 
{
  std::ostringstream out;

  out << "{" << std::endl;
  {
    out << FormatJSON("name")      << " : " << FormatJSON(src.name) << "," << std::endl;
    out << FormatJSON("attributes") << " : { " << std::endl;
    {
      int I = 0, N = (int)src.attributes.size(); 
      for (const auto& it : src.attributes) {
        out << FormatJSON(it.first) << " : " << FormatJSON(it.second) << ((I != N - 1) ? "," : "") << std::endl;
        I++;
      }
    }
    out << "}," << std::endl;
    out << FormatJSON("childs") << " : [ " << std::endl;
    {
      int I = 0, N = (int)src.getChilds().size(); 
      for (auto child : src.getChilds())
      {
        out << toJSONString(*child, nrec + 1) << ((I != N - 1) ? "," : "") << std::endl;
        I++;
      }
    }
    out << "]" << std::endl;
  }
  out << "}" << std::endl;

  return out.str();
}

////////////////////////////////////////////////////////////////////////
static StringTree FromXmlElement(TiXmlElement* src)
{
  StringTree dst(src->Value());

  for (TiXmlAttribute* attr = src->FirstAttribute(); attr; attr = attr->Next())
    dst.write(attr->Name(), attr->Value());

  //xml_text                                              
  if (auto child = src->FirstChild())
  {
    if (auto child_text = child->ToText())
    {
      if (auto xml_text = child_text->Value())
        dst.writeText(xml_text, child_text->CDATA());
    }
    else if (auto child_comment = child->ToComment())
    {
      dst.addComment(child_comment->Value());
    }
  }

  for (auto child = src->FirstChildElement(); child; child = child->NextSiblingElement())
    dst.addChild(FromXmlElement(child));

  return dst;
}

////////////////////////////////////////////////////////////
StringTree StringTree::fromString(String content, bool bEnablePostProcessing)
{
  content = StringUtils::trim(content);

  if (content.empty())
    return StringTree();

  if (!StringUtils::startsWith(content, "<"))
    return StringTree();

  TiXmlDocument xmldoc;
  xmldoc.Parse(content.c_str());
  if (xmldoc.Error())
  {
    PrintWarning("Failed StringTree::fromString failed",
      "xmldoc.ErrorRow" ,xmldoc.ErrorRow(),
      "xmldoc.ErrorCol",xmldoc.ErrorCol(),
      "xmldoc.ErrorDesc",xmldoc.ErrorDesc());
    return StringTree();
  }

  auto ret = StringTree("TiXmlDocument");
  for (auto child = xmldoc.FirstChildElement(); child; child=child->NextSiblingElement())
    ret.addChild(FromXmlElement(child));

  if (ret.getNumberOfChilds() == 1)
    ret = *ret.getFirstChild();

  if (bEnablePostProcessing)
    ret = StringTree::postProcess(ret);

  return ret;
}


VISUS_IMPLEMENT_SINGLETON_CLASS(Private::VisusConfig)

//////////////////////////////////////////////////////////////
bool ConfigFile::load(String filename, bool bEnablePostProcessing)
{
  if (filename.empty()) {
    VisusAssert(false);
    return false;
  }

  if (!FileUtils::existsFile(filename))
    return false;

  String content = Utils::loadTextDocument(filename);
  StringTree temp=StringTree::fromString(content, bEnablePostProcessing);
  if (!temp.valid())
  {
    PrintWarning("visus config content is wrong");
    return false;
  }

  auto keep_name = this->name;
  this->filename = filename;
  this->StringTree::operator=(temp);
  this->name= keep_name;

  return true;
}

//////////////////////////////////////////////////////////////
void ConfigFile::save()
{
  Utils::saveTextDocument(filename, this->StringTree::toXmlString());
}

} //namespace Visus






