//===- NullDerefChecker.h -- Null pointer dereference detector--------------//
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
 * NullDerefChecker.h
 *
 *  Created on: Jun 11, 2026
 *      Author: Claude (SVFG-based NPD checker for ArkTS)
 */

#ifndef NULLDEREFCHECKER_H_
#define NULLDEREFCHECKER_H_

#include "SABER/SrcSnkDDA.h"
#include "SABER/SaberCheckerAPI.h"

namespace SVF
{

/*!
 * Null Pointer Dereference Detector (SVFG-based)
 *
 * Sources: API calls that may return null (image.createImageSource, JSON.parse, etc.)
 * Sinks: Dereference operations (load, GEP, indirect calls)
 *
 * Uses the same SVFG traversal infrastructure as LeakChecker.
 */
class NullDerefChecker : public SrcSnkDDA
{

public:
    typedef Map<const SVFGNode*,const CallICFGNode*> SVFGNodeToCSIDMap;
    typedef FIFOWorkList<const CallICFGNode*> CSWorkList;
    typedef ProgSlice::VFWorkList WorkList;
    typedef NodeBS SVFGNodeBS;

    /// Constructor
    NullDerefChecker()
    {
    }
    /// Destructor
    virtual ~NullDerefChecker()
    {
    }

    /// We start from here
    virtual bool runOnModule(SVFIR* pag)
    {
        /// start analysis
        analyze();
        return false;
    }

    /// Initialize sources and sinks
    //@{
    /// Initialize sources (nullable return values)
    virtual void initSrcs() override;
    /// Initialize sinks (dereference sites)
    virtual void initSnks() override;

    /// Whether the function may return null
    virtual inline bool isSourceLikeFun(const FunObjVar* fun) override
    {
        return isNullableReturnFunction(fun);
    }

    /// Whether the node is a dereference operation
    virtual inline bool isSinkLikeFun(const FunObjVar* fun) override
    {
        // For NPD, sinks are not function calls but dereference operations
        // This will be handled differently in initSnks()
        return false;
    }
    //@}

protected:
    /// Report null dereference bugs
    //@{
    virtual void reportBug(ProgSlice* slice) override;
    //@}

    /// Check if a function may return null
    bool isNullableReturnFunction(const FunObjVar* fun) const;

    /// Check if a function name (from !ark.callee.name) may return null
    bool isNullableIndirectFunction(const std::string& funcName) const;

    /// Record a source to its callsite
    //@{
    inline void addSrcToCSID(const SVFGNode* src, const CallICFGNode* cs)
    {
        srcToCSIDMap[src] = cs;
    }
    inline const CallICFGNode* getSrcCSID(const SVFGNode* src)
    {
        SVFGNodeToCSIDMap::iterator it = srcToCSIDMap.find(src);
        assert(it != srcToCSIDMap.end() && "source node not at a callsite??");
        return it->second;
    }
    //@}

private:
    SVFGNodeToCSIDMap srcToCSIDMap;
};

} // End namespace SVF

#endif /* NULLDEREFCHECKER_H_ */
