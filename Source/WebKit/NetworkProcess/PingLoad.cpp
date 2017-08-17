/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

#include "config.h"
#include "PingLoad.h"

#if USE(NETWORK_SESSION)

#include "AuthenticationManager.h"
#include "Logging.h"
#include "NetworkCORSPreflightChecker.h"
#include "SessionTracker.h"
#include <WebCore/ContentSecurityPolicy.h>
#include <WebCore/CrossOriginAccessControl.h>
#include <WebCore/CrossOriginPreflightResultCache.h>

#define RELEASE_LOG_IF_ALLOWED(fmt, ...) RELEASE_LOG_IF(m_parameters.sessionID.isAlwaysOnLoggingAllowed(), Network, "%p - PingLoad::" fmt, this, ##__VA_ARGS__)

namespace WebKit {

using namespace WebCore;

PingLoad::PingLoad(NetworkResourceLoadParameters&& parameters)
    : m_parameters(WTFMove(parameters))
    , m_timeoutTimer(*this, &PingLoad::timeoutTimerFired)
    , m_isSameOriginRequest(securityOrigin().canRequest(m_parameters.request.url()))
{
    ASSERT(m_parameters.sourceOrigin);
    ASSERT(m_parameters.originalRequestHeaders);

    // If the server never responds, this object will hang around forever.
    // Set a very generous timeout, just in case.
    m_timeoutTimer.startOneShot(60000_s);

    if (m_isSameOriginRequest || m_parameters.mode == FetchOptions::Mode::NoCors) {
        loadRequest(ResourceRequest { m_parameters.request });
        return;
    }

    makeCrossOriginAccessRequest(ResourceRequest { m_parameters.request });
}

PingLoad::~PingLoad()
{
    if (m_redirectHandler)
        m_redirectHandler({ });

    if (m_task) {
        ASSERT(m_task->client() == this);
        m_task->clearClient();
        m_task->cancel();
    }
}

void PingLoad::loadRequest(ResourceRequest&& request)
{
    RELEASE_LOG_IF_ALLOWED("startNetworkLoad");
    if (auto* networkSession = SessionTracker::networkSession(m_parameters.sessionID)) {
        auto loadParameters = m_parameters;
        loadParameters.request = WTFMove(request);
        m_task = NetworkDataTask::create(*networkSession, *this, WTFMove(loadParameters));
        m_task->resume();
    } else
        ASSERT_NOT_REACHED();
}

SecurityOrigin& PingLoad::securityOrigin() const
{
    return m_origin ? *m_origin : *m_parameters.sourceOrigin;
}

const HTTPHeaderMap& PingLoad::originalRequestHeaders() const
{
    return *m_parameters.originalRequestHeaders;
}

void PingLoad::willPerformHTTPRedirection(ResourceResponse&& redirectResponse, ResourceRequest&& request, RedirectCompletionHandler&& completionHandler)
{
    RELEASE_LOG_IF_ALLOWED("willPerformHTTPRedirection - shouldFollowRedirects? %d", m_parameters.shouldFollowRedirects);
    if (!m_parameters.shouldFollowRedirects) {
        completionHandler({ });
        return;
    }

    if (auto* contentSecurityPolicy = this->contentSecurityPolicy()) {
        if (!contentSecurityPolicy->allowConnectToSource(request.url(), redirectResponse.isNull() ? ContentSecurityPolicy::RedirectResponseReceived::No : ContentSecurityPolicy::RedirectResponseReceived::Yes)) {
            RELEASE_LOG_IF_ALLOWED("willPerformHTTPRedirection - Redirect was blocked by CSP");
            completionHandler({ });
            return;
        }
    }

    // FIXME: We should ensure the number of redirects does not exceed 20.

    if (isAllowedRedirect(request.url())) {
        completionHandler(WTFMove(request));
        return;
    }
    RELEASE_LOG_IF_ALLOWED("willPerformHTTPRedirection - Redirect requires a CORS preflight");

    // Force any subsequent request to use these checks.
    m_isSameOriginRequest = false;

    // Use a unique origin for subsequent loads if needed.
    // https://fetch.spec.whatwg.org/#concept-http-redirect-fetch (Step 10).
    ASSERT(m_parameters.mode == FetchOptions::Mode::Cors);
    if (!securityOrigin().canRequest(redirectResponse.url()) && !protocolHostAndPortAreEqual(redirectResponse.url(), request.url())) {
        if (!m_origin || !m_origin->isUnique())
            m_origin = SecurityOrigin::createUnique();
    }

    // Except in case where preflight is needed, loading should be able to continue on its own.
    if (m_isSimpleRequest && isSimpleCrossOriginAccessRequest(request.httpMethod(), originalRequestHeaders())) {
        completionHandler(WTFMove(request));
        return;
    }

    m_parameters.allowStoredCredentials = DoNotAllowStoredCredentials;
    m_redirectHandler = WTFMove(completionHandler);

    // Let's fetch the request with the original headers (equivalent to request cloning specified by fetch algorithm).
    request.setHTTPHeaderFields(m_parameters.request.httpHeaderFields());

    makeCrossOriginAccessRequest(WTFMove(request));
}

void PingLoad::didReceiveChallenge(const AuthenticationChallenge&, ChallengeCompletionHandler&& completionHandler)
{
    RELEASE_LOG_IF_ALLOWED("didReceiveChallenge");
    completionHandler(AuthenticationChallengeDisposition::Cancel, { });
    delete this;
}

void PingLoad::didReceiveResponseNetworkSession(ResourceResponse&& response, ResponseCompletionHandler&& completionHandler)
{
    RELEASE_LOG_IF_ALLOWED("didReceiveResponseNetworkSession - httpStatusCode: %d", response.httpStatusCode());
    completionHandler(PolicyAction::PolicyIgnore);
    delete this;
}

void PingLoad::didReceiveData(Ref<SharedBuffer>&&)
{
    RELEASE_LOG_IF_ALLOWED("didReceiveData");
    ASSERT_NOT_REACHED();
}

void PingLoad::didCompleteWithError(const ResourceError& error, const NetworkLoadMetrics&)
{
    if (error.isNull())
        RELEASE_LOG_IF_ALLOWED("didComplete");
    else
        RELEASE_LOG_IF_ALLOWED("didCompleteWithError, error_code: %d", error.errorCode());
    delete this;
}

void PingLoad::didSendData(uint64_t totalBytesSent, uint64_t totalBytesExpectedToSend)
{
}

void PingLoad::wasBlocked()
{
    RELEASE_LOG_IF_ALLOWED("wasBlocked");
    delete this;
}

void PingLoad::cannotShowURL()
{
    RELEASE_LOG_IF_ALLOWED("cannotShowURL");
    delete this;
}

void PingLoad::timeoutTimerFired()
{
    RELEASE_LOG_IF_ALLOWED("timeoutTimerFired");
    delete this;
}

bool PingLoad::isAllowedRedirect(const URL& url) const
{
    if (m_parameters.mode == FetchOptions::Mode::NoCors)
        return true;

    return m_isSameOriginRequest && securityOrigin().canRequest(url);
}

ContentSecurityPolicy* PingLoad::contentSecurityPolicy() const
{
    if (!m_contentSecurityPolicy && m_parameters.cspResponseHeaders) {
        m_contentSecurityPolicy = std::make_unique<ContentSecurityPolicy>(*m_parameters.sourceOrigin);
        m_contentSecurityPolicy->didReceiveHeaders(*m_parameters.cspResponseHeaders, ContentSecurityPolicy::ReportParsingErrors::No);
    }
    return m_contentSecurityPolicy.get();
}

void PingLoad::makeCrossOriginAccessRequest(ResourceRequest&& request)
{
    ASSERT(m_parameters.mode == FetchOptions::Mode::Cors);
    RELEASE_LOG_IF_ALLOWED("makeCrossOriginAccessRequest");

    if (isSimpleCrossOriginAccessRequest(request.httpMethod(), originalRequestHeaders())) {
        makeSimpleCrossOriginAccessRequest(WTFMove(request));
        return;
    }

    m_isSimpleRequest = false;
    if (CrossOriginPreflightResultCache::singleton().canSkipPreflight(securityOrigin().toString(), request.url(), m_parameters.allowStoredCredentials, request.httpMethod(), originalRequestHeaders())) {
        RELEASE_LOG_IF_ALLOWED("makeCrossOriginAccessRequest - preflight can be skipped thanks to cached result");
        preflightSuccess(WTFMove(request));
    } else
        makeCrossOriginAccessRequestWithPreflight(WTFMove(request));
}

void PingLoad::makeSimpleCrossOriginAccessRequest(ResourceRequest&& request)
{
    ASSERT(isSimpleCrossOriginAccessRequest(request.httpMethod(), originalRequestHeaders()));
    RELEASE_LOG_IF_ALLOWED("makeSimpleCrossOriginAccessRequest");

    if (!request.url().protocolIsInHTTPFamily()) {
        RELEASE_LOG_IF_ALLOWED("makeSimpleCrossOriginAccessRequest: Cross origin requests are only supported for HTTP.");
        return;
    }

    updateRequestForAccessControl(request, securityOrigin(), m_parameters.allowStoredCredentials);
    loadRequest(WTFMove(request));
}

void PingLoad::makeCrossOriginAccessRequestWithPreflight(ResourceRequest&& request)
{
    RELEASE_LOG_IF_ALLOWED("makeCrossOriginAccessRequestWithPreflight");
    ASSERT(!m_corsPreflightChecker);

    NetworkCORSPreflightChecker::Parameters parameters = {
        WTFMove(request),
        securityOrigin(),
        m_parameters.sessionID,
        m_parameters.allowStoredCredentials
    };
    m_corsPreflightChecker = std::make_unique<NetworkCORSPreflightChecker>(WTFMove(parameters), [this](NetworkCORSPreflightChecker::Result result) {
        RELEASE_LOG_IF_ALLOWED("makeCrossOriginAccessRequestWithPreflight preflight complete, success: %d forRedirect? %d", result == NetworkCORSPreflightChecker::Result::Success, !!m_redirectHandler);
        auto corsPreflightChecker = WTFMove(m_corsPreflightChecker);
        if (result == NetworkCORSPreflightChecker::Result::Success)
            preflightSuccess(ResourceRequest { corsPreflightChecker->originalRequest() });
        else
            delete this;
    });
    m_corsPreflightChecker->startPreflight();
}

void PingLoad::preflightSuccess(ResourceRequest&& request)
{
    RELEASE_LOG_IF_ALLOWED("preflightSuccess");

    ResourceRequest actualRequest = WTFMove(request);
    updateRequestForAccessControl(actualRequest, securityOrigin(), m_parameters.allowStoredCredentials);

    if (auto redirectHandler = std::exchange(m_redirectHandler, nullptr))
        redirectHandler(WTFMove(actualRequest));
    else
        loadRequest(WTFMove(actualRequest));
}

} // namespace WebKit

#endif // USE(NETWORK_SESSION)
