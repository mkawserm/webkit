/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/PointerPreparations.h>

namespace JSC {

enum PtrTag : uintptr_t {
    NoPtrTag = 0,
    NearCallPtrTag,
    CFunctionPtrTag,

    BytecodePtrTag,
    BytecodeHelperPtrTag,
    CodeEntryPtrTag,
    CodeEntryWithArityCheckPtrTag,
    ExceptionHandlerPtrTag,
    InternalFunctionPtrTag,
    JITCodePtrTag,
    NativeCodePtrTag,
    SlowPathPtrTag,
};

#if !USE(POINTER_PROFILING)
inline uintptr_t uniquePtrTagID() { return 0; }

template<typename... Arguments>
inline constexpr PtrTag ptrTag(Arguments&&...) { return NoPtrTag; }

template<typename T, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value && !std::is_same<T, PtrType>::value>>
inline constexpr T tagCodePtr(PtrType ptr, PtrTag) { return bitwise_cast<T>(ptr); }

template<typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline constexpr PtrType tagCodePtr(PtrType ptr, PtrTag) { return ptr; }

template<typename T, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value && !std::is_same<T, PtrType>::value>>
inline constexpr T untagCodePtr(PtrType ptr, PtrTag) { return bitwise_cast<T>(ptr); }

template<typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline constexpr PtrType untagCodePtr(PtrType ptr, PtrTag) { return ptr; }

template<typename T, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value && !std::is_same<T, PtrType>::value>>
inline constexpr T retagCodePtr(PtrType ptr, PtrTag, PtrTag) { return bitwise_cast<T>(ptr); }

template<typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline constexpr PtrType retagCodePtr(PtrType ptr, PtrTag, PtrTag) { return ptr; }

template<typename T, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value && !std::is_same<T, PtrType>::value>>
inline constexpr T removeCodePtrTag(PtrType ptr) { return bitwise_cast<T>(ptr); }

template<typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline constexpr PtrType removeCodePtrTag(PtrType ptr) { return ptr; }

#endif // !USE(POINTER_PROFILING)

} // namespace JSC

#if USE(APPLE_INTERNAL_SDK) && __has_include(<WebKitAdditions/PtrTagSupport.h>)
#include <WebKitAdditions/PtrTagSupport.h>
#endif
