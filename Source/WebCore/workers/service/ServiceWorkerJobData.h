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

#if ENABLE(SERVICE_WORKER)

#include "SecurityOriginData.h"
#include "ServiceWorkerJobType.h"
#include "ServiceWorkerRegistrationKey.h"
#include "ServiceWorkerRegistrationOptions.h"
#include "URL.h"

namespace WebCore {

struct ServiceWorkerJobData {
public:
    explicit ServiceWorkerJobData(uint64_t connectionIdentifier);
    ServiceWorkerJobData(const ServiceWorkerJobData&);
    ServiceWorkerJobData() = default;

    uint64_t jobIdentifier() const { return m_jobIdentifier; }
    uint64_t connectionIdentifier() const { return m_connectionIdentifier; }

    URL scriptURL;
    URL clientCreationURL;
    SecurityOriginData topOrigin;
    URL scopeURL;
    ServiceWorkerJobType type;

    std::unique_ptr<RegistrationOptions> registrationOptions;

    ServiceWorkerRegistrationKey registrationKey() const;
    ServiceWorkerJobData isolatedCopy() const;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static bool decode(Decoder&, ServiceWorkerJobData&);

private:
    uint64_t m_jobIdentifier { 0 };
    uint64_t m_connectionIdentifier { 0 };
};

template<class Encoder>
void ServiceWorkerJobData::encode(Encoder& encoder) const
{
    encoder << m_jobIdentifier << m_connectionIdentifier << scriptURL << clientCreationURL << topOrigin << scopeURL;
    encoder.encodeEnum(type);
    switch (type) {
    case ServiceWorkerJobType::Register:
        RELEASE_ASSERT(registrationOptions);
        encoder << *registrationOptions;
        break;
    }
}

template<class Decoder>
bool ServiceWorkerJobData::decode(Decoder& decoder, ServiceWorkerJobData& jobData)
{
    if (!decoder.decode(jobData.m_jobIdentifier))
        return false;
    if (!decoder.decode(jobData.m_connectionIdentifier))
        return false;
    if (!decoder.decode(jobData.scriptURL))
        return false;
    if (!decoder.decode(jobData.clientCreationURL))
        return false;
    if (!decoder.decode(jobData.topOrigin))
        return false;
    if (!decoder.decode(jobData.scopeURL))
        return false;
    if (!decoder.decodeEnum(jobData.type))
        return false;

    switch (jobData.type) {
    case ServiceWorkerJobType::Register:
        jobData.registrationOptions = std::make_unique<RegistrationOptions>();
        if (!decoder.decode(*jobData.registrationOptions))
            return false;
        break;
    }

    return true;
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
