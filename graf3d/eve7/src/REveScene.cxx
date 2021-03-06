// @(#)root/eve:$Id$
// Authors: Matevz Tadel & Alja Mrak-Tadel: 2006, 2007

/*************************************************************************
 * Copyright (C) 1995-2007, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#include <ROOT/REveScene.hxx>
#include <ROOT/REveViewer.hxx>
#include <ROOT/REveManager.hxx>
#include <ROOT/REveTrans.hxx>
#include <ROOT/REveRenderData.hxx>
#include <ROOT/REveClient.hxx>
#include <ROOT/RWebWindow.hxx>

#include "json.hpp"


using namespace ROOT::Experimental;
namespace REX = ROOT::Experimental;

/** \class REveScene
\ingroup REve
Eve representation of TGLScene.
The GLScene is owned by this class - it is created on construction
time and deleted at destruction.

Normally all objects are positioned directly in global scene-space.
By setting the fHierarchical flag, positions of children get
calculated by multiplying the transformation matrices of all parents.
*/

////////////////////////////////////////////////////////////////////////////////
/// Constructor.

REveScene::REveScene(const char* n, const char* t) :
   REveElementList(n, t)
{
   fScene = this;
}

////////////////////////////////////////////////////////////////////////////////
/// Destructor.

REveScene::~REveScene()
{
   fDestructing = kStandard;

   REX::gEve->GetViewers()->SceneDestructing(this);
   REX::gEve->GetScenes()->RemoveElement(this);
}

////////////////////////////////////////////////////////////////////////////////
/// Virtual from REveElement; here we simply append this scene to
/// the list.

void REveScene::CollectSceneParents(List_t& scenes)
{
   scenes.push_back(this);
}

//------------------------------------------------------------------------------

void REveScene::AddSubscriber(std::unique_ptr<REveClient> &&sub)
{
   assert(sub.get() != nullptr && fAcceptingChanges == kFALSE);

   fSubscribers.emplace_back(std::move(sub));

   // XXX Here should send out the package to the new subscriber,
   // In principle can expect a new one in short time?
   // Keep streamed data until next begin change, maybe.
}

void REveScene::RemoveSubscriber(unsigned id)
{
   assert(fAcceptingChanges == kFALSE);

   auto pred = [&](std::unique_ptr<REveClient> &client) {
      return client->fId == id;
   };

   fSubscribers.erase(std::remove_if(fSubscribers.begin(), fSubscribers.end(), pred), fSubscribers.end());
}

void REveScene::BeginAcceptingChanges()
{
   if (fAcceptingChanges) return;

   if (HasSubscribers()) fAcceptingChanges = kTRUE;
}

void REveScene::SceneElementChanged(REveElement* element)
{
   assert(fAcceptingChanges);

   fChangedElements.insert(element);
}

void REveScene::SceneElementAdded(REveElement* element)
{
   assert(fAcceptingChanges);
   // printf("REveScene::SceneElementAdded(\n");
   fAddedElements.insert(element);
}


void REveScene::SceneElementRemoved(ElementId_t id)
{
   assert(fAcceptingChanges);

   fRemovedElements.push_back(id);
}

void REveScene::EndAcceptingChanges()
{
   if ( ! fAcceptingChanges) return;

   fAcceptingChanges = kFALSE;
}

void REveScene::ProcessChanges()
{
   // should return net message or talk to gEve about it
}

void REveScene::StreamElements()
{
   fOutputJson.clear();
   fOutputBinary.clear();

   fElsWithBinaryData.clear();
   fTotalBinarySize = 0;

   nlohmann::json jarr = nlohmann::json::array();

   nlohmann::json jhdr = {};
   jhdr["content"]  = "REveScene::StreamElements";
   jhdr["fSceneId"] = fElementId;

   if (fCommands.size() > 0) {
      jhdr["commands"] = nlohmann::json::array();
      for (auto &&cmd : fCommands) {
         nlohmann::json jcmd = {};
         jcmd["name"]  = cmd.fName;
         jcmd["icon"] = cmd.fIcon;
         jcmd["elementid"] = cmd.fElementId;
         jcmd["elementclass"] = cmd.fElementClass;
         jcmd["func"] = cmd.fAction; // SL: may be not needed on client side, can use name
         jhdr["commands"].push_back(jcmd);
      }
   }

   jarr.push_back(jhdr);

   StreamJsonRecurse(this, jarr);
   // for (auto &c : fChildren)
   // {
   //    StreamJsonRecurse(c, jarr);
   // }

   fOutputBinary.resize(fTotalBinarySize);
   Int_t actual_binary_size = 0;

   for (auto &&e : fElsWithBinaryData) {
      Int_t rd_size = e->fRenderData->Write( & fOutputBinary[ actual_binary_size ] );
      actual_binary_size += rd_size;
   }
   assert(actual_binary_size == fTotalBinarySize);

   jarr.front()["fTotalBinarySize"] = fTotalBinarySize;

   fOutputJson = jarr.dump();
}

void REveScene::StreamJsonRecurse(REveElement *el, nlohmann::json &jarr)
{
   nlohmann::json jobj = {};
   Int_t rd_size = el->WriteCoreJson(jobj, fTotalBinarySize);
   jarr.push_back(jobj);

   // If this is another scene, do not stream additional details.
   // It should be requested / subscribed to independently.

   if (el->fScene == el && el != this)
   {
      return;
   }

   if (rd_size > 0)
   {
      assert (rd_size % 4 == 0);

      fTotalBinarySize += rd_size;
      fElsWithBinaryData.push_back(el);
   }

   for (auto &&c : el->fChildren)
   {
      StreamJsonRecurse(c, jarr);
   }
}

////////////////////////////////////////////////////////////////////////////////
//
/// Prepare data for sending element changes
//
////////////////////////////////////////////////////////////////////////////////
void REveScene::StreamRepresentationChanges()
{
   fOutputJson.clear();
   fOutputBinary.clear();

   fElsWithBinaryData.clear();
   fTotalBinarySize = 0;

   nlohmann::json jarr = nlohmann::json::array();

   nlohmann::json jhdr = {};
   jhdr["content"]  = "ElementsRepresentaionChanges";
   jhdr["fSceneId"] = fElementId;

   jhdr["removedElements"] = nlohmann::json::array();
   for (auto &re : fRemovedElements)
      jhdr["removedElements"].push_back(re);

   jhdr["numRepresentationChanged"] = fChangedElements.size();

   // jarr.push_back(jhdr);

   for (Set_i i = fChangedElements.begin(); i != fChangedElements.end(); ++i)
   {
      REveElement* el = *i;
      UChar_t bits = el->GetChangeBits();

      nlohmann::json jobj = {};
      jobj["fElementId"] = el->GetElementId();
      jobj["changeBit"] = bits;
      if (bits & kCBVisibility)
      {
         jobj["fRnrSelf"]     = el->GetRnrSelf();
         jobj["fRnrChildren"] = el->GetRnrChildren();
      }

      if (bits & kCBColorSelection)
      {
         el->WriteCoreJson(jobj, -1);
      }

      if (bits & kCBTransBBox)
      {
      }

      if (bits & kCBObjProps)
      {
         printf("total element change %s \n", el->GetElementName());

         Int_t rd_size = el->WriteCoreJson(jobj, fTotalBinarySize);
         if (rd_size) {
            assert (rd_size % 4 == 0);
            fTotalBinarySize += rd_size;
            fElsWithBinaryData.push_back(el);
         }
      }

      jarr.push_back(jobj);

      el->ClearStamps();
   }

   for (auto el : fAddedElements) {
      nlohmann::json jobj = {};
      printf("scene representation change new element change %s \n", el->GetElementName());
      Int_t rd_size = el->WriteCoreJson(jobj, fTotalBinarySize);
      jarr.push_back(jobj);
      if (rd_size) {
         assert (rd_size % 4 == 0);
         fTotalBinarySize += rd_size;
         fElsWithBinaryData.push_back(el);
      }
   }

   fChangedElements.clear();
   fAddedElements.clear();
   fRemovedElements.clear();


   // render data for total change
   fOutputBinary.resize(fTotalBinarySize);
   Int_t actual_binary_size = 0;

   for (auto &e : fElsWithBinaryData)
   {
      Int_t rd_size = e->fRenderData->Write( & fOutputBinary[ actual_binary_size ] );

      actual_binary_size += rd_size;
   }
   assert(actual_binary_size == fTotalBinarySize);

   jhdr["fTotalBinarySize"] = fTotalBinarySize;

   nlohmann::json msg = { {"header", jhdr}, {"arr", jarr}};
   fOutputJson = msg.dump();

   printf("[%s] Stream representation changes %s ...\n", GetElementName(), fOutputJson.substr(0,30).c_str() );
}

void
REveScene::SendChangesToSubscribers()
{
   for (auto && client : fSubscribers) {
      printf("   sending json, len = %d --> to conn_id = %d\n", (int) fOutputJson.size(), client->fId);
      client->fWebWindow->Send(client->fId, fOutputJson);
      if (fTotalBinarySize) {
         printf("   sending binary, len = %d --> to conn_id = %d\n", fTotalBinarySize, client->fId);
         client->fWebWindow->SendBinary(client->fId, &fOutputBinary[0], fTotalBinarySize);
      }
   }
}

Bool_t
REveScene::IsChanged() const
{
   printf("REveScene::IsChanged %s %d\n", GetElementName(), fAddedElements.empty());
   return ! (fChangedElements.empty() && fAddedElements.empty() && fRemovedElements.empty());
}


/*
////////////////////////////////////////////////////////////////////////////////
/// Repaint the scene.

void REveScene::Repaint(Bool_t dropLogicals)
{
   if (dropLogicals) fGLScene->SetSmartRefresh(kFALSE);
   fGLScene->PadPaint(fPad);
   if (dropLogicals) fGLScene->SetSmartRefresh(kTRUE);
   fChanged = kFALSE;

   // Hack to propagate selection state to physical shapes.
   //
   // Should actually be published in PadPaint() following a direct
   // AddObject() call, but would need some other stuff for that.
   // Optionally, this could be exported via the TAtt3D and everything
   // would be sweet.

   TGLScene::LogicalShapeMap_t& logs = fGLScene->RefLogicalShapes();
   REveElement* elm;
   for (TGLScene::LogicalShapeMapIt_t li = logs.begin(); li != logs.end(); ++li)
   {
      elm = dynamic_cast<REveElement*>(li->first);
      if (elm && li->second->Ref() == 1)
      {
         TGLPhysicalShape* pshp = const_cast<TGLPhysicalShape*>(li->second->GetFirstPhysical());
         pshp->Select(elm->GetSelectedLevel());
      }
   }

   // Fix positions for hierarchical scenes.
   if (fHierarchical)
   {
      RetransHierarchically();
   }
}

////////////////////////////////////////////////////////////////////////////////
/// Entry point for hierarchical transformation update.
/// Calls the recursive variant on all children.

void REveScene::RetransHierarchically()
{
   fGLScene->BeginUpdate();

   RetransHierarchicallyRecurse(this, RefMainTrans());

   fGLScene->EndUpdate();
}

////////////////////////////////////////////////////////////////////////////////
/// Set transformation matrix for physical shape of element el in
/// the GL-scene and recursively descend into children (if enabled).

void REveScene::RetransHierarchicallyRecurse(REveElement* el, const REveTrans& tp)
{
   static const REveException eh("REveScene::RetransHierarchicallyRecurse ");

   REveTrans t(tp);
   if (el->HasMainTrans())
      t *= el->RefMainTrans();

   if (el->GetRnrSelf() && el != this)
   {
      fGLScene->UpdatePhysioLogical(el->GetRenderObject(eh), t.Array(), 0);
   }

   if (el->GetRnrChildren())
   {
      for (List_i i = el->BeginChildren(); i != el->EndChildren(); ++i)
      {
         if ((*i)->GetRnrAnything())
            RetransHierarchicallyRecurse(*i, t);
      }
   }
}
*/


/*
////////////////////////////////////////////////////////////////////////////////
/// Paint the scene. Iterate over children and calls PadPaint().

void REveScene::Paint(Option_t* option)
{
   if (GetRnrState())
   {
      for(List_i i=fChildren.begin(); i!=fChildren.end(); ++i)
      {
         // (*i)->PadPaint(option);
      }
   }
}

////////////////////////////////////////////////////////////////////////////////
/// Remove element from the scene.
/// It is not an error if the element is not found in the scene.

void REveScene::DestroyElementRenderers(REveElement* element)
{
   static const REveException eh("REveScene::DestroyElementRenderers ");

   fGLScene->BeginUpdate();
   Bool_t changed = fGLScene->DestroyLogical(element->GetRenderObject(eh), kFALSE);
   fGLScene->EndUpdate(changed, changed);
}

////////////////////////////////////////////////////////////////////////////////
/// Remove element represented by object rnrObj from the scene.
/// It is not an error if the element is not found in the scene.

void REveScene::DestroyElementRenderers(TObject* rnrObj)
{
   fGLScene->BeginUpdate();
   Bool_t changed = fGLScene->DestroyLogical(rnrObj, kFALSE);
   fGLScene->EndUpdate(changed, changed);
}
*/


/** \class REveSceneList
\ingroup REve
List of Scenes providing common operations on REveScene collections.
*/

////////////////////////////////////////////////////////////////////////////////
/// Constructor.

REveSceneList::REveSceneList(const char* n, const char* t) :
   REveElementList(n, t)
{
   SetChildClass(REveScene::Class());
}

////////////////////////////////////////////////////////////////////////////////
/// Destroy all scenes and their contents.
/// Tho object with non-zero deny-destroy will still survive.

void REveSceneList::DestroyScenes()
{
   List_i i = fChildren.begin();
   while (i != fChildren.end())
   {
      REveScene* s = (REveScene*) *(i++);
      s->DestroyElements();
      s->DestroyOrWarn();
   }
}

////////////////////////////////////////////////////////////////////////////////
/// Set accept changes flag on all scenes.

void REveSceneList::AcceptChanges(bool on)
{
   for (List_i i=fChildren.begin(); i!=fChildren.end(); ++i)
   {
      REveScene* s = (REveScene*) *i;
      if (on)
         s->BeginAcceptingChanges();
      else
         s->EndAcceptingChanges();
   }
}
/*
////////////////////////////////////////////////////////////////////////////////
/// Repaint scenes that are tagged as changed.

void REveSceneList::RepaintChangedScenes(Bool_t dropLogicals)
{
   for (List_i i=fChildren.begin(); i!=fChildren.end(); ++i)
   {
      REveScene* s = (REveScene*) *i;
      if (s->IsChanged())
      {
         s->Repaint(dropLogicals);
      }
   }
}

////////////////////////////////////////////////////////////////////////////////
/// Repaint all scenes.

void REveSceneList::RepaintAllScenes(Bool_t dropLogicals)
{
   for (List_i i=fChildren.begin(); i!=fChildren.end(); ++i)
   {
      ((REveScene*) *i)->Repaint(dropLogicals);
   }
}

////////////////////////////////////////////////////////////////////////////////
/// Loop over all scenes and remove all instances of element from them.

void REveSceneList::DestroyElementRenderers(REveElement* element)
{
   static const REveException eh("REveSceneList::DestroyElementRenderers ");

   TObject* obj = element->GetRenderObject(eh);
   for (List_i i=fChildren.begin(); i!=fChildren.end(); ++i)
   {
      ((REveScene*)*i)->DestroyElementRenderers(obj);
   }
}

*/

////////////////////////////////////////////////////////////////////////////////
//
// Send an update of element representations
//
////////////////////////////////////////////////////////////////////////////////

void REveSceneList::ProcessSceneChanges()
{
   printf("ProcessSceneChanges\n");

   for (List_i sIt=fChildren.begin(); sIt!=fChildren.end(); ++sIt)
   {
      REveScene* s = (REveScene*) *sIt;
      if (s->IsChanged()) {
         s->StreamRepresentationChanges();
         s->SendChangesToSubscribers();
      }
   }
}
