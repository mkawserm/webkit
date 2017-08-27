
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "FetchHeaders.h"
#include "FetchOptions.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "SharedBuffer.h"

namespace WebCore {

struct CacheQueryOptions;

namespace DOMCache {

enum class Error {
    NotImplemented,
    Internal
};

Exception errorToException(Error);

WEBCORE_EXPORT bool queryCacheMatch(const ResourceRequest& request, const ResourceRequest& cachedRequest, const ResourceResponse&, const CacheQueryOptions&);

using ResponseBody = Variant<std::nullptr_t, Ref<FormData>, Ref<SharedBuffer>>;
ResponseBody isolatedResponseBody(const ResponseBody&);

struct Record {
    WEBCORE_EXPORT Record copy() const;

    uint64_t identifier;
    uint64_t updateResponseCounter;

    FetchHeaders::Guard requestHeadersGuard;
    ResourceRequest request;
    FetchOptions options;
    String referrer;

    FetchHeaders::Guard responseHeadersGuard;
    ResourceResponse response;
    ResponseBody responseBody;
};

struct CacheInfo {
    uint64_t identifier;
    String name;
};

using CacheIdentifierOrError = Expected<uint64_t, Error>;
using CacheIdentifierCallback = WTF::Function<void(const CacheIdentifierOrError&)>;

using RecordIdentifiersOrError = Expected<Vector<uint64_t>, Error>;
using RecordIdentifiersCallback = WTF::Function<void(RecordIdentifiersOrError&&)>;

using CacheInfosOrError = Expected<Vector<WebCore::DOMCache::CacheInfo>, Error>;
using CacheInfosCallback = WTF::Function<void(CacheInfosOrError&&)>;

using RecordsOrError = Expected<Vector<Record>, Error>;
using RecordsCallback = WTF::Function<void(RecordsOrError&&)>;

using CompletionCallback = WTF::Function<void(std::optional<Error>&&)>;

} // namespace DOMCache

} // namespace WebCore

namespace WTF {
template<> struct EnumTraits<WebCore::DOMCache::Error> {
    using values = EnumValues<
        WebCore::DOMCache::Error,
        WebCore::DOMCache::Error::NotImplemented,
        WebCore::DOMCache::Error::Internal
    >;
};
}
