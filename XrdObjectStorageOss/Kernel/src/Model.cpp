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


#include <Visus/Model.h>
#include <Visus/Diff.h>

namespace Visus {

///////////////////////////////////////////////////////////////
Model::Model() {
  clearHistory();
}

///////////////////////////////////////////////////////////////
Model::~Model() {
  destroyed.emitSignal();
  VisusAssert(destroyed.empty());
}

///////////////////////////////////////////////////////////////
void Model::copy(Model& dst, const Model& src) 
{
  auto redo = EncodeObject("Decode", src);
  auto undo = EncodeObject("Decode", dst);
  dst.beginUpdate(redo, undo);
  dst.read(redo);
  dst.endUpdate();
}


///////////////////////////////////////////////////////////////
void Model::applyPatch(String patch)
{
  auto diff = Visus::Diff(StringUtils::getNonEmptyLines(patch));
  if (diff.empty())
    return;

  auto encoded = EncodeObject(this->getTypeName(), *this);

  std::vector<String> curr = StringUtils::getNonEmptyLines(encoded.toXmlString());
  std::vector<String> next = diff.applyDirect(curr);

  encoded = StringTree::fromString(StringUtils::join(next, "\r\n"));
  if (!encoded.valid())
  {
    String error_msg = cstring("Error ApplyPatch::applyPatch()\r\n"
      "diff:\r\n", "[[", diff.toString(), "]]\r\n"
      "curr:\r\n", "[[", StringUtils::join(curr, "\r\n"), "]]\r\n"
      "next:\r\n", "[[", StringUtils::join(next, "\r\n"), "]]\r\n\r\n");

    ThrowException(error_msg);
  }

  beginDiff();
  read(encoded);
  endDiff();
}

///////////////////////////////////////////////////////////////
void Model::execute(Archive& ar)
{
  //backward compatible
  if (ar.name == "Viewer")
    ar.name = "Transaction";

  if (ar.name == "Decode")
  {
    auto redo = ar;
    auto undo = EncodeObject("Decode", *this);
    beginUpdate(redo, undo);
    read(redo);
    endUpdate();
    return;
  }

  if (ar.name == "Redo")
  {
    redo();
    return;
  }

  if (ar.name == "Undo")
  {
    undo();
    return;
  }

  if (ar.name == "Transaction")
  {
    beginTransaction();
    {
      for (auto sub_action : ar.getChilds())
      {
        if (!sub_action->isHash())
          execute(*sub_action);
      }
    }
    endTransaction();
    return;
  }

  if (ar.name == "ApplyPatch")
  {
    String patch; 
    ar.readText("patch", patch);

    applyPatch(patch);
    return;
  }

  ThrowException("internal error, unknown action " + ar.name);
}


///////////////////////////////////////////////////////////////
void Model::enableLog(String filename)
{
  if (log.is_open())
    log.close();

  this->log_filename = filename;

  if (!filename.empty())
  {
    //important to set the buffer to zerro
    log.open(log_filename.c_str(), std::fstream::out);
    log.rdbuf()->pubsetbuf(0, 0);
  }
}

///////////////////////////////////////////////////////////////
void Model::clearHistory()
{
  VisusAssert(!isUpdating());
  VisusAssert(!bUndoingRedoing);
  this->history.clear();
  this->stack = std::stack<UndoRedo>();
  this->undo_redo.clear();
  this->cursor_undo_redo = 0;
  this->bUndoingRedoing = false;
  this->utc = Time::now().getUTCMilliseconds();
  enableLog(this->log_filename);
}

///////////////////////////////////////////////////////////////
StringTree Model::getHistory() const {

  StringTree ret("history");
  for (auto it : history)
    ret.addChild(it.redo);
  return ret;
}


///////////////////////////////////////////////////////////////
void Model::beginUpdate(StringTree redo, StringTree undo)
{
  UndoRedo undo_redo;
  undo_redo.redo = redo;
  undo_redo.undo = undo;

  this->stack.push(undo_redo);

  //emit signal
  if (stack.size()==1)
    begin_update.emitSignal();
}

///////////////////////////////////////////////////////////////
StringTree Model::simplifyAction(StringTree action)
{
  //so far only Transaction can be simplied
  if (action.name != "Transaction")
    return action;

  auto v = action.getChilds();

  // nothing to simplify 
  if (v.size() == 0)
    return action;

  //a transaction with only only one action is equivalent to that action
  if (v.size() == 1)
    return simplifyAction(*v[0]);

  //it is still a transaction (preserving attributes if present)
  auto ret=Transaction();
  ret.attributes = action.attributes;

  for (auto it : v)
  {
    auto sub= simplifyAction(*it);

    //simplify transaction of transactions
    if (sub.name == "Transaction")
    {
      for (auto jt : sub.getChilds())
        ret.addChild(*jt);
    }
    else if (bool bValid=!sub.name.empty())
    {
      ret.addChild(sub);
    }
  }

  return ret;
}

///////////////////////////////////////////////////////////////
void Model::endUpdate()
{
  auto redo = simplifyAction(this->stack.top().redo);
  auto undo = simplifyAction(this->stack.top().undo);

  this->stack.pop();

  if (stack.empty())
  {
    VisusReleaseAssert(!redo.name.empty());
    VisusReleaseAssert(!undo.name.empty());

    auto utc = Time::now().getUTCMilliseconds() - this->utc;
    redo.write("utc", utc);
    undo.write("utc", utc);

    UndoRedo undo_redo;
    undo_redo.redo = redo;
    undo_redo.undo = undo;

    this->history.push_back(undo_redo);

    if (this->log.is_open())
    {
      this->log << redo.toString() << std::endl;
      this->log.flush();

      //for debugging
#ifdef _DEBUG
      {
        this->log << "<!--UNDO" << std::endl;
        this->log << undo.toString() << std::endl;
        this->log << "-->" << std::endl;
        this->log.flush();
      }
#endif
    }

    //do not touch the undo/redo history if in the middle of an undo/redo
    if (!bUndoingRedoing)
    {
      this->undo_redo.resize(cursor_undo_redo);
      this->undo_redo.push_back(undo_redo);
      this->cursor_undo_redo = (int)this->undo_redo.size();
    }

    //emit signals 
    this->modelChanged();
    this->end_update.emitSignal();
  }

  //collect actions
  if (!stack.empty())
  {
    auto& REDO = stack.top().redo;
    auto& UNDO = stack.top().undo;

    if (REDO.name == "Transaction" && !redo.name.empty())
      REDO.addChild(std::make_shared<StringTree>(redo));

    //for undo they are executed in reverse order
    if (UNDO.name == "Transaction" && !undo.name.empty())
      UNDO.addChildAtBegin(std::make_shared<StringTree>(undo));
  }
}


///////////////////////////////////////////////////////////////
void Model::beginDiff()
{
  if (isUpdating())
    return;

  this->diff_begin = EncodeObject(this->getTypeName(), *this);

  beginUpdate(
    StringTree("applyPatch"),
    StringTree("applyPatch"));
}

///////////////////////////////////////////////////////////////
void Model::endDiff()
{
  StringTree A = diff_begin;
  StringTree B = EncodeObject(this->getTypeName(), *this);

  auto diff = Visus::Diff(
    StringUtils::getNonEmptyLines(A.toXmlString()),
    StringUtils::getNonEmptyLines(B.toXmlString()));

  stack.top().redo.writeText("patch", diff           .toString(), /*cdata*/true);
  stack.top().undo.writeText("patch", diff.inverted().toString(), /*cdata*/true);

  endUpdate();
}

///////////////////////////////////////////////////////////////
bool Model::redo() 
{
  VisusAssert(VisusHasMessageLock());
  VisusAssert(!isUpdating());
  VisusAssert(!bUndoingRedoing);

  if (!canRedo())  
    return false;

  auto action = undo_redo[cursor_undo_redo++].redo;

  bUndoingRedoing = true;
  beginUpdate(
    StringTree("Redo"),
    StringTree("Undo"));
  execute(action);
  endUpdate();
  bUndoingRedoing = false;

  return true;
}

///////////////////////////////////////////////////////////////
bool Model::undo() 
{
  VisusAssert(VisusHasMessageLock());
  VisusAssert(!isUpdating());
  VisusAssert(!bUndoingRedoing);

  if (!canUndo()) 
    return false;

  auto action = undo_redo[--cursor_undo_redo].undo;

  bUndoingRedoing = true;
  beginUpdate(
    StringTree("Undo"), 
    StringTree("Redo"));
  execute(action);
  endUpdate();
  bUndoingRedoing = false;

  return true;
}

} //namespace Visus

