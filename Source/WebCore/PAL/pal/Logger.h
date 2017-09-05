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

#pragma once

#include <wtf/Assertions.h>
#include <wtf/HexNumber.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace PAL {

template<typename T>
struct LogArgument {
    template<typename U = T> static typename std::enable_if<std::is_same<U, bool>::value, String>::type toString(bool argument) { return argument ? ASCIILiteral("true") : ASCIILiteral("false"); }
    template<typename U = T> static typename std::enable_if<std::is_same<U, int>::value, String>::type toString(int argument) { return String::number(argument); }
    template<typename U = T> static typename std::enable_if<std::is_same<U, float>::value, String>::type toString(float argument) { return String::number(argument); }
    template<typename U = T> static typename std::enable_if<std::is_same<U, double>::value, String>::type toString(double argument) { return String::number(argument); }
    template<typename U = T> static typename std::enable_if<std::is_same<typename std::remove_reference<U>::type, AtomicString>::value, String>::type toString(AtomicString argument) { return argument.string(); }
    template<typename U = T> static typename std::enable_if<std::is_same<typename std::remove_reference<U>::type, String>::value, String>::type toString(String argument) { return argument; }
    template<typename U = T> static typename std::enable_if<std::is_same<U, const char*>::value, String>::type toString(const char* argument) { return String(argument); }
    template<size_t length> static String toString(const char (&argument)[length]) { return String(argument); }
};

class Logger : public RefCounted<Logger> {
    WTF_MAKE_NONCOPYABLE(Logger);
public:

    class Observer {
    public:
        virtual ~Observer() { }
        virtual void didLogMessage(const WTFLogChannel&, const String&) = 0;
    };

    static Ref<Logger> create(const void* owner)
    {
        return adoptRef(*new Logger(owner));
    }

    template<typename... Arguments>
    inline void logAlways(WTFLogChannel& channel, const Arguments&... arguments) const
    {
#if RELEASE_LOG_DISABLED
        // "Standard" WebCore logging goes to stderr, which is captured in layout test output and can generally be a problem
        //  on some systems, so don't allow it.
        UNUSED_PARAM(channel);
#else
        if (!willLog(channel, WTFLogLevelAlways))
            return;

        log(channel, arguments...);
#endif
    }

    template<typename... Arguments>
    inline void error(WTFLogChannel& channel, const Arguments&... arguments) const
    {
        if (!willLog(channel, WTFLogLevelError))
            return;

        log(channel, arguments...);
    }

    template<typename... Arguments>
    inline void warning(WTFLogChannel& channel, const Arguments&... arguments) const
    {
        if (!willLog(channel, WTFLogLevelWarning))
            return;

        log(channel, arguments...);
    }

    template<typename... Arguments>
    inline void notice(WTFLogChannel& channel, const Arguments&... arguments) const
    {
        if (!willLog(channel, WTFLogLevelNotice))
            return;

        log(channel, arguments...);
    }

    template<typename... Arguments>
    inline void info(WTFLogChannel& channel, const Arguments&... arguments) const
    {
        if (!willLog(channel, WTFLogLevelInfo))
            return;

        log(channel, arguments...);
    }

    template<typename... Arguments>
    inline void debug(WTFLogChannel& channel, const Arguments&... arguments) const
    {
        if (!willLog(channel, WTFLogLevelDebug))
            return;

        log(channel, arguments...);
    }

    inline bool willLog(const WTFLogChannel& channel, WTFLogLevel level) const
    {
        if (level != WTFLogLevelAlways && level > channel.level)
            return false;

        if (channel.level != WTFLogLevelAlways && channel.state == WTFLogChannelOff)
            return false;

        return m_enabled;
    }

    bool enabled() const { return m_enabled; }
    void setEnabled(const void* owner, bool enabled)
    {
        ASSERT(owner == m_owner);
        if (owner == m_owner)
            m_enabled = enabled;
    }

    struct MethodAndPointer {
        MethodAndPointer(const char* methodName, const void* objectPtr)
            : methodName { methodName }
            , objectPtr { reinterpret_cast<uintptr_t>(objectPtr) }
        {
        }

        MethodAndPointer(const char* className, const char* methodName, const void* objectPtr)
            : className { className }
            , methodName { methodName }
            , objectPtr { reinterpret_cast<uintptr_t>(objectPtr) }
        {
        }

        const char* className { nullptr };
        const char* methodName { nullptr };
        const uintptr_t objectPtr { 0 };
    };

    static inline void addObserver(Observer& observer)
    {
        observers().append(observer);
    }
    static inline void removeObserver(Observer& observer)
    {
        observers().removeFirstMatching([&observer](auto anObserver) {
            return &anObserver.get() == &observer;
        });
    }

private:
    Logger(const void* owner)
        : m_owner { owner }
    {
    }

    template<typename... Argument>
    static inline void log(WTFLogChannel& channel, const Argument&... arguments)
    {
        String logMessage = makeString(LogArgument<Argument>::toString(arguments)...);

#if RELEASE_LOG_DISABLED
        WTFLog(&channel, "%s", logMessage.utf8().data());
#else
        os_log(channel.osLogChannel, "%{public}s", logMessage.utf8().data());
#endif

        for (Observer& observer : observers())
            observer.didLogMessage(channel, logMessage);
    }

    static Vector<std::reference_wrapper<Observer>>& observers()
    {
        static NeverDestroyed<Vector<std::reference_wrapper<Observer>>> observers;
        return observers;
    }

    const void* m_owner;
    bool m_enabled { true };
};

class LogHelper {
public:
    virtual ~LogHelper() = default;

    virtual const Logger& logger() const = 0;
    virtual const char* className() const = 0;
    virtual WTFLogChannel& logChannel() const = 0;

    inline bool willLog(WTFLogLevel level) const { return logger().willLog(logChannel(), level); }

#define ALWAYS_LOG(...)     logger().logAlways(logChannel(), Logger::MethodAndPointer(className(), __func__, this), ##__VA_ARGS__)
#define ERROR_LOG(...)      logger().error(logChannel(), Logger::MethodAndPointer(className(), __func__, this), ##__VA_ARGS__)
#define WARNING_LOG(...)    logger().warning(logChannel(), Logger::MethodAndPointer(className(), __func__, this), ##__VA_ARGS__)
#define NOTICE_LOG(...)     logger().notice(logChannel(), Logger::MethodAndPointer(className(), __func__, this), ##__VA_ARGS__)
#define INFO_LOG(...)       logger().info(logChannel(), Logger::MethodAndPointer(className(), __func__, this), ##__VA_ARGS__)
#define DEBUG_LOG(...)      logger().debug(logChannel(), Logger::MethodAndPointer(className(), __func__, this), ##__VA_ARGS__)

};

template <>
struct LogArgument<Logger::MethodAndPointer> {
    static String toString(const Logger::MethodAndPointer& value)
    {
        StringBuilder builder;

        if (value.className) {
            builder.append(value.className);
            builder.appendLiteral("::");
        }
        builder.append(value.methodName);
        builder.appendLiteral("(0x");
        appendUnsigned64AsHex(value.objectPtr, builder);
        builder.appendLiteral(") ");
        return builder.toString();
    }
};

} // namespace PAL
