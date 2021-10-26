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

#ifndef VISUS_MODEL_H__
#define VISUS_MODEL_H__

#include <Visus/Kernel.h>
#include <Visus/Time.h>
#include <Visus/StringUtils.h>
#include <Visus/SignalSlot.h>

#include <stack>
#include <fstream>

namespace Visus {

//////////////////////////////////////////////////////////
class VISUS_KERNEL_API BaseView
{
public:

  VISUS_CLASS(BaseView)

  virtual ~BaseView() {
  }
};


//////////////////////////////////////////////////////////
class VISUS_KERNEL_API Model
{
public:

  VISUS_NON_COPYABLE_CLASS(Model)

  //begin_update
  Signal<void()>  begin_update;

  //end_update
  Signal<void()> end_update;

  //destroyed
  Signal<void()> destroyed;

  //views
  std::vector<BaseView*> views;

  //constructor
  Model();

  //destructor
  virtual ~Model();

  //getTypeName
  virtual String getTypeName() const = 0;

public:

  //enableLog
  void enableLog(String filename);

  //clearHistory
  void clearHistory();

  //getHistory
  StringTree getHistory() const;

public:

  //isUpdating
  inline bool isUpdating() const {
    return stack.size() > 0;
  }

  //lastRedo
  StringTree lastRedo() const {
    return history.back().redo;
  }

  //lastUndo
  StringTree lastUndo() const {
    return history.back().undo;;
  }

  //beginUpdate
  void beginUpdate(StringTree redo, StringTree undo);

  //endUpdate
  void endUpdate();

  //addUpdate
  void addUpdate(StringTree redo, StringTree undo) {
    beginUpdate(redo, undo);
    endUpdate();
  }

  //beginTransaction
  void beginTransaction() {
    beginUpdate(Transaction(),Transaction());
  }

  //endTransaction
  void endTransaction() {
    endUpdate();
  }

  //Transaction
  static StringTree Transaction() {
    return StringTree("Transaction");
  }

  //beginDiff
  void beginDiff();

  //endDiff
  void endDiff();

public:

  //setProperty
  template <typename Value>
  void setProperty(String action_name, Value& old_value, const Value& new_value, bool bForce=false)
  {
    if (!bForce && old_value == new_value) 
      return;
    
    beginUpdate(
      StringTree(action_name).write("value", new_value),
      StringTree(action_name).write("value", old_value));
    {
      old_value = new_value;
    }
    endUpdate();
  }

  //setEncodedProperty
  template <typename Value>
  void setEncodedProperty(String action_name, Value& old_value, const Value& new_value, bool bForce = false)
  {
    if (!bForce && old_value == new_value)
      return;

    beginUpdate(
      EncodeObject(action_name, new_value),
      EncodeObject(action_name, old_value));
    {
      old_value = new_value;
    }
    endUpdate();
  }

public:

  //copy
  static void copy(Model& dst, const Model& src);

public:

  //canRedo
  bool canRedo() const {
    return !undo_redo.empty() && cursor_undo_redo < undo_redo.size();
  }

  //canUndo
  bool canUndo() const {
    return !undo_redo.empty() && cursor_undo_redo > 0;
  }

  //redo
  bool redo();

  //undo
  bool undo();

  //applyPatch
  void applyPatch(String text);

public:

  //addView
  void addView(BaseView* value) {
    this->views.push_back(value);
  }

  //removeView
  void removeView(BaseView* value) {
    Utils::remove(this->views,value);
  }

public:

  //execute
  virtual void execute(Archive& ar);

  //write
  virtual void write(Archive& ar) const  = 0;

  //read
  virtual void read(Archive& ar) = 0;

protected:

  //modelChanged
  virtual void modelChanged() {
  }

private:

  typedef struct
  {
    StringTree redo;
    StringTree undo;
  }
  UndoRedo;

  Int64                    utc = 0;
  std::vector<UndoRedo>    history;
  String                   log_filename;
  std::ofstream            log;
  bool                     bUndoingRedoing = false;
  std::stack<UndoRedo>     stack;
  std::vector<UndoRedo>    undo_redo;
  int                      cursor_undo_redo = 0;
  StringTree               diff_begin;

  //simplifyAction
  StringTree simplifyAction(StringTree action);

};


inline StringTree CreatePassThroughAction(String left, const StringTree& action) {
  //i want the target_id at the beginning of attributes
  auto ret = action;
  ret.removeAttribute("target_id");
  ret.attributes.insert(ret.attributes.begin(), std::make_pair("target_id", action.readString("target_id").empty() ? left : left + "/" + action.readString("target_id")));
  return ret;
}

inline String PopTargetId(StringTree& action)
{
  auto v = StringUtils::split(action.readString("target_id"), "/");
  if (v.empty()) return "";
  auto left = v[0];
  auto right = StringUtils::join(std::vector<String>(v.begin() + 1, v.end()), "/");

  //i want the target_id at the beginning of attributes
  action.removeAttribute("target_id");
  action.attributes.insert(action.attributes.begin(), std::make_pair("target_id", right));
  return left;
}

inline bool GetPassThroughAction(String left, StringTree& action) {
  auto v = StringUtils::split(action.readString("target_id"), "/");
  if (v.empty() || v[0] != left) return false;
  PopTargetId(action);
  return true;
}

template <class Value>
inline StringTree EncodeObject(String name, const Value& value)
{
  StringTree ret(name);
  value.write(ret);
  return ret;
}

//////////////////////////////////////////////////////////
template <class ModelClassArg>
class View : public virtual BaseView
{
public:

  typedef ModelClassArg ModelClass;

  //constructor
  View() : model(nullptr)
  {}

  //destructor
  virtual ~View()
  {
    //did you forget to call bindModel(nullptr)?
    VisusAssert(model==nullptr);
    this->View::bindModel(nullptr);
  }

  //getModel
  inline ModelClass* getModel() const {
    return model;
  }

  //bindViewModel
  virtual void bindModel(ModelClass* value)
  {
    if (value==this->model) return;

    if (this->model) 
    {
      this->model->removeView(this);
      this->model->end_update.disconnect(changed_slot);
      this->model->Model::destroyed.disconnect(destroyed_slot);
    }

    this->model=value; 

    if (this->model) 
    {
      this->model->end_update.connect(changed_slot=Slot<void()>([this]() {
        modelChanged();
      }));

      this->model->Model::destroyed.connect(destroyed_slot=Slot<void()>([this] {
        bindModel(nullptr);
      }));

      this->model->addView(this);
    }
  }

  //rebindModel
  inline void rebindModel()
  {
    ModelClass* model=this->model;
    bindModel(nullptr);
    bindModel(model);
  }

  //modelChanged
  virtual void modelChanged(){
  }

protected:

  ModelClass* model;

private:

  VISUS_NON_COPYABLE_CLASS(View)

  Slot<void()> changed_slot;
  Slot<void()> destroyed_slot;

};

} //namespace Visus

#endif //VISUS_MODEL_H__


