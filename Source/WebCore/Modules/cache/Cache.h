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

#include "ActiveDOMObject.h"
#include "CacheStorageConnection.h"
#include "CacheStorageRecord.h"

namespace WebCore {

class ScriptExecutionContext;

class Cache final : public RefCounted<Cache>, public ActiveDOMObject {
public:
    static Ref<Cache> create(ScriptExecutionContext& context, String&& name, uint64_t identifier, Ref<CacheStorageConnection>&& connection) { return adoptRef(*new Cache(context, WTFMove(name), identifier, WTFMove(connection))); }
    ~Cache();

    using RequestInfo = FetchRequest::Info;

    using KeysPromise = DOMPromiseDeferred<IDLSequence<IDLInterface<FetchRequest>>>;

    void match(RequestInfo&&, CacheQueryOptions&&, Ref<DeferredPromise>&&);
    void matchAll(std::optional<RequestInfo>&&, CacheQueryOptions&&, Ref<DeferredPromise>&&);
    void add(RequestInfo&&, DOMPromiseDeferred<void>&&);

    void addAll(Vector<RequestInfo>&&, DOMPromiseDeferred<void>&&);
    void put(RequestInfo&&, Ref<FetchResponse>&&, DOMPromiseDeferred<void>&&);
    void remove(RequestInfo&&, CacheQueryOptions&&, DOMPromiseDeferred<IDLBoolean>&&);
    void keys(std::optional<RequestInfo>&&, CacheQueryOptions&&, KeysPromise&&);

    const String& name() const { return m_name; }
    uint64_t identifier() const { return m_identifier; }

private:
    Cache(ScriptExecutionContext&, String&& name, uint64_t identifier, Ref<CacheStorageConnection>&&);

    enum class MatchType { All, OnlyFirst };
    void doMatch(std::optional<RequestInfo>&&, CacheQueryOptions&&, Ref<DeferredPromise>&&, MatchType);

    // ActiveDOMObject
    void stop() final;
    const char* activeDOMObjectName() const final;
    bool canSuspendForDocumentSuspension() const final;

    void retrieveRecords(WTF::Function<void()>&&);
    Vector<CacheStorageRecord> queryCacheWithTargetStorage(const FetchRequest&, const CacheQueryOptions&, const Vector<CacheStorageRecord>&);
    void queryCache(Ref<FetchRequest>&&, CacheQueryOptions&&, WTF::Function<void(const Vector<CacheStorageRecord>&)>&&);
    void batchDeleteOperation(const FetchRequest&, CacheQueryOptions&&, WTF::Function<void(bool didRemoval, CacheStorageConnection::Error)>&&);
    void batchPutOperation(const FetchRequest&, FetchResponse&, WTF::Function<void(CacheStorageConnection::Error)>&&);

    void updateRecords(Vector<CacheStorageConnection::Record>&&);

    String m_name;
    uint64_t m_identifier;
    Ref<CacheStorageConnection> m_connection;

    Vector<CacheStorageRecord> m_records;
    bool m_isStopped { false };
};

} // namespace WebCore
