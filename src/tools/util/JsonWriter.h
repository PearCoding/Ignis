#pragma once

#include "IG_Config.h"

namespace IG {
class JsonWriter {
public:
    inline JsonWriter(std::ostream& stream, bool whitespace)
        : mStream(stream)
        , mUseWhitespace(whitespace)
        , mCurrentDepth(0)
    {
    }

    inline ~JsonWriter()
    {
        if (mUseWhitespace)
            mStream << std::endl;
    }

    inline std::ostream& w() { return mStream; }
    inline void s()
    {
        if (mUseWhitespace)
            mStream << " ";
    }

    inline void endl()
    {
        if (mUseWhitespace) {
            mStream << std::endl;

            const int whitespaces = mCurrentDepth * 2;
            for (int i = 0; i < whitespaces; ++i)
                mStream << " ";
        }
    }

    inline void goIn()
    {
        ++mCurrentDepth;
        endl();
    }

    inline void goOut()
    {
        if (mCurrentDepth > 0) {
            --mCurrentDepth;
            endl();
        } else {
            std::cerr << "INVALID OUTPUT STREAM HANDLING!" << std::endl;
        }
    }

    inline void objBegin()
    {
        mStream << "{";
        goIn();
    }

    inline void objEnd()
    {
        goOut();
        mStream << "}";
    }

    inline void arrBegin()
    {
        mStream << "[";
        // goIn();
    }

    inline void arrEnd()
    {
        // goOut();
        mStream << "]";
    }

    inline void key(const std::string& name)
    {
        mStream << "\"" << name << "\":";
        if (mUseWhitespace)
            mStream << " ";
    }

private:
    std::ostream& mStream;
    const bool mUseWhitespace;
    int mCurrentDepth;
};
} // namespace IG