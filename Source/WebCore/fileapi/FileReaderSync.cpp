/*
 * Copyright (C) 2010 Google Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "FileReaderSync.h"

#include "Blob.h"
#include "BlobURL.h"
#include "ExceptionCode.h"
#include "FileReaderLoader.h"
#include <runtime/ArrayBuffer.h>

namespace WebCore {

FileReaderSync::FileReaderSync()
{
}

ExceptionOr<RefPtr<ArrayBuffer>> FileReaderSync::readAsArrayBuffer(ScriptExecutionContext& scriptExecutionContext, Blob& blob)
{
    FileReaderLoader loader(FileReaderLoader::ReadAsArrayBuffer, 0);
    auto result = startLoading(scriptExecutionContext, loader, blob);
    if (result.hasException())
        return result.releaseException();
    return loader.arrayBufferResult();
}

ExceptionOr<String> FileReaderSync::readAsBinaryString(ScriptExecutionContext& scriptExecutionContext, Blob& blob)
{
    FileReaderLoader loader(FileReaderLoader::ReadAsBinaryString, 0);
    return startLoadingString(scriptExecutionContext, loader, blob);
}

ExceptionOr<String> FileReaderSync::readAsText(ScriptExecutionContext& scriptExecutionContext, Blob& blob, const String& encoding)
{
    FileReaderLoader loader(FileReaderLoader::ReadAsText, 0);
    loader.setEncoding(encoding);
    return startLoadingString(scriptExecutionContext, loader, blob);
}

ExceptionOr<String> FileReaderSync::readAsDataURL(ScriptExecutionContext& scriptExecutionContext, Blob& blob)
{
    FileReaderLoader loader(FileReaderLoader::ReadAsDataURL, 0);
    loader.setDataType(blob.type());
    return startLoadingString(scriptExecutionContext, loader, blob);
}

static ExceptionCode errorCodeToExceptionCode(FileError::ErrorCode errorCode)
{
    switch (errorCode) {
    case FileError::OK:
        return 0;
    case FileError::NOT_FOUND_ERR:
        return NOT_FOUND_ERR;
    case FileError::SECURITY_ERR:
        return SECURITY_ERR;
    case FileError::ABORT_ERR:
        return ABORT_ERR;
    case FileError::NOT_READABLE_ERR:
        return NotReadableError;
    case FileError::ENCODING_ERR:
        return EncodingError;
    case FileError::NO_MODIFICATION_ALLOWED_ERR:
        return NO_MODIFICATION_ALLOWED_ERR;
    case FileError::INVALID_STATE_ERR:
        return INVALID_STATE_ERR;
    case FileError::SYNTAX_ERR:
        return SYNTAX_ERR;
    case FileError::INVALID_MODIFICATION_ERR:
        return INVALID_MODIFICATION_ERR;
    case FileError::QUOTA_EXCEEDED_ERR:
        return QUOTA_EXCEEDED_ERR;
    case FileError::TYPE_MISMATCH_ERR:
        return TYPE_MISMATCH_ERR;
    case FileError::PATH_EXISTS_ERR:
        return NO_MODIFICATION_ALLOWED_ERR;
    }
    return UnknownError;
}

ExceptionOr<void> FileReaderSync::startLoading(ScriptExecutionContext& scriptExecutionContext, FileReaderLoader& loader, Blob& blob)
{
    loader.start(&scriptExecutionContext, blob);
    if (ExceptionCode code = errorCodeToExceptionCode(loader.errorCode()))
        return Exception { code };
    return { };
}

ExceptionOr<String> FileReaderSync::startLoadingString(ScriptExecutionContext& scriptExecutionContext, FileReaderLoader& loader, Blob& blob)
{
    auto result = startLoading(scriptExecutionContext, loader, blob);
    if (result.hasException())
        return result.releaseException();
    return loader.stringResult();
}

} // namespace WebCore
