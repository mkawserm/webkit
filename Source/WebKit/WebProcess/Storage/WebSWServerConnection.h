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

#include "MessageReceiver.h"
#include "MessageSender.h"
#include <WebCore/SWServer.h>
#include <pal/SessionID.h>

namespace WebCore {
struct ExceptionData;
}

namespace WebKit {

class WebSWServerConnection : public WebCore::SWServer::Connection, public IPC::MessageSender, public IPC::MessageReceiver {
public:
    static Ref<WebSWServerConnection> create(const PAL::SessionID& sessionID)
    {
        return adoptRef(*new WebSWServerConnection(sessionID));
    }
    static Ref<WebSWServerConnection> create(IPC::Connection& connection, uint64_t connectionIdentifier, const PAL::SessionID& sessionID)
    {
        return adoptRef(*new WebSWServerConnection(connection, connectionIdentifier, sessionID));
    }

    ~WebSWServerConnection() final;

    uint64_t identifier() const { return m_identifier; }

    void scheduleJob(const WebCore::ServiceWorkerJobData&) final;

    void disconnectedFromWebProcess();
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

private:
    WebSWServerConnection(const PAL::SessionID&);
    WebSWServerConnection(IPC::Connection&, uint64_t connectionIdentifier, const PAL::SessionID&);

    void scheduleStorageJob(const WebCore::ServiceWorkerJobData&);

    IPC::Connection* messageSenderConnection() final { return m_connection.ptr(); }
    uint64_t messageSenderDestinationID() final { return m_identifier; }

    PAL::SessionID m_sessionID;
    uint64_t m_identifier;

    Ref<IPC::Connection> m_connection;
}; // class WebSWServerConnection

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)
