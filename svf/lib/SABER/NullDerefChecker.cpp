//===- NullDerefChecker.cpp -- Null pointer dereference detector------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.

// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===----------------------------------------------------------------------===//

/*
 * NullDerefChecker.cpp
 *
 *  Created on: Jun 11, 2026
 *      Author: Claude (SVFG-based NPD checker for ArkTS)
 */

#include "Util/Options.h"
#include "SABER/NullDerefChecker.h"

using namespace SVF;
using namespace SVFUtil;

/*!
 * Initialize sources (nullable return values)
 */
void NullDerefChecker::initSrcs()
{
    SVFIR* pag = getPAG();

    // Iterate over all callsite returns
    for(SVFIR::CSToRetMap::iterator it = pag->getCallSiteRets().begin(),
            eit = pag->getCallSiteRets().end(); it != eit; ++it)
    {
        const RetICFGNode* cs = it->first;
        const CallICFGNode* callNode = cs->getCallICFGNode();

        // Only consider pointer returns
        if (!cs->getType()->isPointerTy())
            continue;

        CallGraph::FunctionSet callees;
        getCallgraph()->getCallees(cs->getCallICFGNode(), callees);

        // Check direct callees
        for(CallGraph::FunctionSet::const_iterator cit = callees.begin(),
                ecit = callees.end(); cit != ecit; ++cit)
        {
            const FunObjVar* fun = *cit;
            if (isSourceLikeFun(fun))
            {
                const ValVar* svfVar = pag->getCallSiteRet(cs);
                const SVFGNode* node = getSVFG()->getDefSVFGNode(svfVar);
                addToSources(node);
                addSrcToCSID(node, callNode);
            }
        }

        // Check indirect calls via !ark.callee.name metadata (ArkTS pattern)
        // Similar to LeakChecker's handling
        if (callees.empty() && callNode->isIndirectCall())
        {
            const std::string& arkName = callNode->getArkCalleeName();
            if (isNullableIndirectFunction(arkName))
            {
                const ValVar* svfVar = pag->getCallSiteRet(cs);
                const SVFGNode* node = getSVFG()->getDefSVFGNode(svfVar);
                addToSources(node);
                addSrcToCSID(node, callNode);
            }
        }
    }
}

/*!
 * Initialize sinks (dereference operations)
 */
void NullDerefChecker::initSnks()
{
    SVFIR* pag = getPAG();

    // For NPD, sinks are loads and stores that dereference pointers
    // We'll collect LoadSVFGNode and StoreSVFGNode from the SVFG
    for (SVFG::const_iterator it = getSVFG()->begin(), eit = getSVFG()->end(); it != eit; ++it)
    {
        const SVFGNode* node = it->second;

        // Load nodes represent pointer dereferences
        if (SVFUtil::isa<LoadSVFGNode>(node))
        {
            addToSinks(node);
        }
        // Store nodes also dereference the pointer being stored to
        else if (SVFUtil::isa<StoreSVFGNode>(node))
        {
            addToSinks(node);
        }
        // GEP (GetElementPtr) nodes represent field/array accesses, which are dereferences
        else if (SVFUtil::isa<GepSVFGNode>(node))
        {
            addToSinks(node);
        }
    }

    // Also check for actual parameters that are dereferenced (passed to functions)
    for(SVFIR::CSToArgsListMap::iterator it = pag->getCallSiteArgsMap().begin(),
            eit = pag->getCallSiteArgsMap().end(); it!=eit; ++it)
    {
        const CallICFGNode* callNode = it->first;
        SVFIR::ValVarList &arglist = it->second;

        // For method calls, the first few arguments might be receivers that get dereferenced
        for (SVFIR::ValVarList::const_iterator ait = arglist.begin(),
                aeit = arglist.end(); ait != aeit; ++ait)
        {
            const SVFVar *svfVar = *ait;
            if (svfVar->isPointer())
            {
                const SVFGNode *snk = getSVFG()->getActualParmVFGNode(svfVar, callNode);
                addToSinks(snk);
            }
        }
    }
}

/*!
 * Check if a function may return null
 */
bool NullDerefChecker::isNullableReturnFunction(const FunObjVar* fun) const
{
    if (!fun)
        return false;

    const std::string& name = fun->getName();

    // ArkTS external APIs that may return null
    if (name.find("@ohos:multimedia.image.imagecreateImageSource") != std::string::npos ||
        name.find("@ohos:multimedia.image.imagecreateImagePacker") != std::string::npos ||
        name.find("@ohos:file.fs.fileIo.open") != std::string::npos ||
        name.find("@ohos:file.fs.fileIo.openSync") != std::string::npos ||
        name.find("@ohos:file.fs.fs.open") != std::string::npos ||
        name.find("@ohos:file.fs.fs.openSync") != std::string::npos ||
        name.find("@ohos:data.dataShare.dataSharecreateDataShareHelper") != std::string::npos)
    {
        return true;
    }

    // JSON.parse can return null
    if (name.find("JSON.parse") != std::string::npos ||
        name.find("json.parse") != std::string::npos)
    {
        return true;
    }

    return false;
}

/*!
 * Check if a function name (from !ark.callee.name) may return null
 */
bool NullDerefChecker::isNullableIndirectFunction(const std::string& funcName) const
{
    // ArkTS indirect method calls that may return null
    if (funcName == "getFirstObject" ||
        funcName == "getThumbnailData" ||
        funcName == "getAllObjects" ||
        funcName == "getAssets" ||
        funcName == "getDataByUri" ||
        funcName == "showAssetsCreationDialog" ||
        funcName == "createDataShareHelper")
    {
        return true;
    }

    return false;
}

/*!
 * Report null dereference bugs
 */
void NullDerefChecker::reportBug(ProgSlice* slice)
{
    const SVFGNode* source = slice->getSource();

    // Get the callsite for this source
    const CallICFGNode* cs = getSrcCSID(source);

    if(isAllPathReachable() == false && isSomePathReachable() == false)
    {
        // Full null dereference - null value always reaches a dereference
        GenericBug::EventStack eventStack =
        {
            SVFBugEvent(SVFBugEvent::SourceInst, cs)
        };
        report.addSaberBug(GenericBug::FULLNULLPTRDEREFERENCE, eventStack);
    }
    else if (isAllPathReachable() == false && isSomePathReachable() == true)
    {
        // Partial null dereference - null value reaches dereference on some paths
        GenericBug::EventStack eventStack;
        slice->evalFinalCond2Event(eventStack);
        eventStack.push_back(SVFBugEvent(SVFBugEvent::SourceInst, cs));
        report.addSaberBug(GenericBug::PARTIALNULLPTRDEREFERENCE, eventStack);
    }
}
