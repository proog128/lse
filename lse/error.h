#ifndef ERROR_H_
#define ERROR_H_

struct Error
{
    Error(const char* format, ...)
    {
        va_list args;
        va_start(args, format);
        vsnprintf(message, 256, format, args);
        va_end(args);
    }

    char message[256];
};

#endif
