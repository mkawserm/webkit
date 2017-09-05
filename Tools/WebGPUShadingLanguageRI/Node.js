/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */
"use strict";

class Node {
    visit(visitor)
    {
        // FIXME: We could stash a memo table in the visitor. So, before calling the visitor, we ask
        // it if they have seen this node before. If they have, then we return what they returned the
        // last time. This also trivially makes the rewriter do remapping correctly. The downside of
        // not doing it is mostly that various visitors might check the same types multiple times.
        // https://bugs.webkit.org/show_bug.cgi?id=176286
        
        let visitFunc = visitor["visit" + this.constructor.name];
        if (!visitFunc)
            throw new Error("No visit function for " + this.constructor.name + " in " + visitor.constructor.name);
        let returnValue = visitFunc.call(visitor, this);
        if ("returnValue" in visitor)
            return visitor.returnValue;
        return returnValue;
    }
    
    unify(unificationContext, other)
    {
        if (!other)
            throw new Error("Null other");
        let unifyThis = this.unifyNode;
        let unifyOther = other.unifyNode;
        if (unifyThis == unifyOther)
            return true;
        if (unifyOther.typeVariableUnify(unificationContext, unifyThis))
            return true;
        return unifyThis.unifyImpl(unificationContext, unifyOther);
    }
    
    unifyImpl(unificationContext, other)
    {
        if (other.typeVariableUnify(unificationContext, this))
            return true;
        return this == other;
    }
    
    typeVariableUnify(unificationContext, other)
    {
        return false;
    }
    
    _typeVariableUnifyImpl(unificationContext, other)
    {
        let realThis = unificationContext.find(this);
        if (realThis != this)
            return realThis.unify(unificationContext, other);
        
        unificationContext.union(this, other);
        return true;
    }
    
    // Most type variables don't care about this.
    commitUnification(unificatoinContext) { }
    
    get unifyNode() { return this; }
    get isUnifiable() { return false; }
    
    get isNative() { return false; }
    
    equals(other)
    {
        let unificationContext = new UnificationContext();
        if (this.unify(unificationContext, other) && unificationContext.verify())
            return unificationContext;
        return false;
    }
    
    equalsWithCommit(other)
    {
        let unificationContext = this.equals(other);
        if (!unificationContext)
            return false;
        unificationContext.commit();
        return unificationContext;
    }
    
    substitute(parameters, argumentList)
    {
        return this.visit(new Substitution(parameters, argumentList));
    }
    
    substituteToUnification(parameters, unificationContext)
    {
        return this.substitute(parameters, parameters.map(type => unificationContext.find(type)));
    }
}
