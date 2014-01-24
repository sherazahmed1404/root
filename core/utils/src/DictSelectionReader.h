// @(#)root/utils/src:$Id$
// Author: Danilo Piparo January 2014

/*************************************************************************
 * Copyright (C) 1995-2011, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/rootcint.            *
 *************************************************************************/

#ifndef __DICTSELECTIONREADER__
#define __DICTSELECTIONREADER__

#include "clang/AST/RecursiveASTVisitor.h"

#include <set>
#include <map>
#include <string>

class SelectionRules;
class ClassSelectionRule;
namespace clang {
   class ASTContext;
//    class DeclContext;
   class NamespaceDecl;
   class CXXRecordDecl;
}

/**
 * @file  DictSelectionReader.h
 * @author Danilo Piparo
 * @date January 2014
 * @brief Select classes and assign properties using C++ syntax.
 *
 * @details When generating dictionary information for a class,
 * one sometimes wants to specify additional information
 * beyond the class definition itself, for example, to specify
 * that certain members are to be treated as transient by the persistency
 * system.  This can be done by associating a dictionary selection class
 * with the class for which dictionary information is being generated.
 * The contents of this selection class encode the additional information.
 * Below, we first discuss how to associate a selection class
 * with your class; then we list the current Set of information
 * which may appear inside the selection class.
 *
 * The simplest case is for the case of a non-template class @c C.
 * By default, the Name of the selection class is then
 * @c ROOT::Meta::Selection::C.  If you have such a class, it will be found
 * automatically.  If @c C is in a namespace, @c NS::C, then
 * the selection class should be in the same namespace: @c ROOT::Selection::NS::C.
 * Examples:
 *

**/

/**
 * The DictSelectionReader is used to create selection rules starting from
 * C++ the constructs of the @c ROOT::Meta::Selection namespace. All rules
 * are matching by name.
 * A brief description of the operations that lead to class selection:
 *    1. If a class declaration is present in the selection namespace, a class
 * with the same name is selected outside the selection namespace.
 *    2. If a template class declaration and a template instantiation is present
 * in the selection namespace, a template instance witgh the same name is
 * selected outside the namespace.
 * For example:
 * @code
 * [...]
 * class classVanilla{};                            
 * template <class A> class classTemplateVanilla {};
 * classTemplateVanilla<char> t0;
 * namespace ROOT{
 *    namespace Meta {
 *       namespace Selection{
 *          class classVanilla{};
 *          template <typename A> class classTemplateVanilla{};
 *          classTemplateVanilla<char> st0;
 *       }
 *    }
 * }
 * @endcode
 * would create two selection rules to select @c classVanilla and
 * @c classTemplateVanilla<char>.
 *
 * A brief description of the properties that can be assigned to classes
 * with the @c ROOT::Meta::Selectio::ClassAttributes class.
 *    1. @c kNonSplittable : Makes the class non splittable  
 * The class properties can be assigned via a traits mechanism. For example:
 * @code
 * [...]
 * class classWithAttributes{};
 * namespace ROOT{
 *    namespace Meta {
 *       namespace Selection{
 *          class classWithAttributes : ClassAttributes <kNonSplittable> {};
 *       }
 *    }
 * }
 * @endcode
 * would create a selection rule which selects class @c classWithAttributes and
 * assignes to it the property described by @c kNonSplittable. Multiple
 * properties can be assigned to a single class with this syntax:
 * @code
 * [...]
 * namespace ROOT{
 *    namespace Meta {
 *       namespace Selection{
 *          class classWithAttributes :
 *             ClassAttributes <kProperty1, kProperty2, kPropertyN> {};
 *       }
 *    }
 * }
 * @endcode
 * 
 * A brief description of the properties that can be assigned to data members
 * with the @c ROOT::Meta::Selection MemberAttributes class:
 *    1. @c kTransient : the data member is transient, not persistified by the
 * ROOT I/O.
 *    2. @c kAutoSelected : the type of the data member is selected without the
 * need of specifying its class explicitely.
 * For example:
 * @code
 * [...]
 * class classTransientMember{
 *  private:
 *    int transientMember;
 * };
 * class classAutoselected{};
 * class classTestAutoselect{
 *  private:
 *    classAutoselected autoselected;
 * };
 *
 * namespace ROOT{
 *    namespace Meta {
 *       namespace Selection{
 *          class classTestAutoselect{
 *             MemberAttributes<kAutoSelected> autoselected;
 *          };
    
    class classTransientMember{
       MemberAttributes<kTransient> transientMember;
       };
 * 
 * @endcode
 * would lead to the creation of selection rules for @c classTransientMember
 * specifying that @c transientMember is transient, @c classTestAutoselect and
 * @c classAutoselected.
 *
 * 
 **/
class DictSelectionReader: public clang::RecursiveASTVisitor<DictSelectionReader>
{
public:

   /// Take the selection rules as input (for consistency w/ other selector interfaces)
   DictSelectionReader(SelectionRules&, const clang::ASTContext &);

   /// Visit the entities that needs to be selected
   bool VisitRecordDecl(clang::RecordDecl*);   

   bool  shouldVisitTemplateInstantiations () const {return true;}
   
private:

   bool fInSelectionNamespace(const clang::RecordDecl&, const std::string& str=""); ///< Check if in the ROOT::Selection namespace
   bool fFirstPass(const clang::RecordDecl&); ///< First pass on the AST
   bool fSecondPass(const clang::RecordDecl&); ///< Second pass on the AST, using the information of the first one
   void fManageFields(const clang::RecordDecl&, const std::string&, ClassSelectionRule&); ///< Take care of the class fields
   void fManageBaseClasses(const clang::CXXRecordDecl&, const std::string&, ClassSelectionRule&); ///< Take care of the class bases
   template<class T>
   const clang::CXXRecordDecl* fGetCXXRcrdDecl(const T& ); ///< Extract the CXXRecordDecl when @c clang::QualType @c T::getType() exists
   const clang::TemplateArgumentList* fGetTmplArgList(const clang::CXXRecordDecl& ); ///< Get the template arguments list if any
   
   SelectionRules& fSelectionRules; ///< The selection rules to be filled
   std::set<const clang::RecordDecl*> fSelectedRecordDecls; ///< The pointers of the selected RecordDecls
   std::set<std::string> fSpecialNames; ///< The names of the classes used for the selction syntax
   std::map<std::string,std::set<std::string> > fAutoSelectedClassFieldNames; ///< Collect the autoselected classes
   std::list<std::pair<std::string, unsigned int> >fTemplateInstanceNamePatternsArgsToKeep; ///< List of pattern-# of args to hide pairs
   //std::set<const clang::DeclContext*> fSelectionNsDecls;///< Cache the pointer to the @c ROOT::Meta::Selection namespace
   bool fIsFirstPass; ///< Keep trance of the number of passes through the AST
};

#endif