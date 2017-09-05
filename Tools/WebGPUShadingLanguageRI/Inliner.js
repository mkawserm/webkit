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

class Inliner extends Rewriter {
    constructor(program, func, visiting)
    {
        super();
        this._program = program;
        this._visiting = visiting;
    }
    
    visitDotExpression(node)
    {
        let result = super.visitDotExpression(node);
        result.field = result.structType.unifyNode.fieldByName(result.fieldName);
        if (result.field.offset == null)
            throw new Error("Un-laid-out field: " + result.field + " (in " + result.structType + ")");
        return result;
    }
    
    visitCallExpression(node)
    {
        return this._visiting.doVisit(node.func, () => {
            let func = this._program.funcInstantiator.getUnique(node.func, node.actualTypeArguments);
            if (func.isNative) {
                let result = super.visitCallExpression(node);
                result.nativeFuncInstance = func;
                return result;
            }
            _inlineFunction(this._program, func, this._visiting);
            return new FunctionLikeBlock(
                node.origin,
                func.returnType,
                node.argumentList.map(argument => argument.visit(this)),
                func.parameters, func.body);
        });
    }
}

