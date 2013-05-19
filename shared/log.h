
#ifndef LOG_H
#define	LOG_H

#define DO_LOG(lvl, msg, ...) logger::log(lvl, msg, ##__VA_ARGS__)

#ifdef LOG_ENABLE_TRACE
#define LOG_TRACE(msg, ...) DO_LOG(logger::LogLevel::TRACE, msg, ##__VA_ARGS__)
#else
#define LOG_TRACE(...)
#endif

#ifdef LOG_ENABLE_DEBUG
#define LOG_DEBUG(msg, ...) DO_LOG(logger::LogLevel::DEBUG, msg, ##__VA_ARGS__)
#else
#define LOG_DEBUG(...)
#endif

#define LOG_INFO(msg, ...)      DO_LOG(logger::LogLevel::INFO, msg, ##__VA_ARGS__)
#define LOG_WARNING(msg, ...)   DO_LOG(logger::LogLevel::WARNING, msg, ##__VA_ARGS__)
#define LOG_ERROR(msg, ...)     DO_LOG(logger::LogLevel::ERROR, msg, ##__VA_ARGS__)
#define LOG_FATAL(msg, ...)     DO_LOG(logger::LogLevel::FATAL, msg, ##__VA_ARGS__)

#ifdef ERROR
#undef ERROR
#endif

#ifndef LOGSTRLEN
#define LOGSTRLEN 512
#endif

namespace logger
{
    struct LogLevel
    {
        enum Level
        {
            FATAL,
            ERROR,
            WARNING,
            INFO,
            DEBUG,
            TRACE
        };

        int lvl;
    };

    void log(int lvl, const char *message, ...) PRINTFARGS(2, 3);
    void setLogFile(const char *file);
    void closeLog();

}

#endif	/* LOG_H */

