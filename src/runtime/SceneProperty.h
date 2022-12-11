#pragma once

#include "IG_Config.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace IG {
class SceneProperty {
public:
    using Number       = float;
    using Integer      = int32;
    using NumberArray  = std::vector<Number>;
    using IntegerArray = std::vector<Integer>;

    enum Type {
        PT_NONE = 0,
        PT_BOOL,
        PT_INTEGER,
        PT_NUMBER,
        PT_STRING,
        PT_TRANSFORM,
        PT_VECTOR2,
        PT_VECTOR3,
        PT_INTEGER_ARRAY,
        PT_NUMBER_ARRAY
    };

    inline SceneProperty()
        : mType(PT_NONE)
    {
    }

    SceneProperty(const SceneProperty& other) = default;
    SceneProperty(SceneProperty&& other)      = default;

    SceneProperty& operator=(const SceneProperty& other) = default;
    SceneProperty& operator=(SceneProperty&& other)      = default;

    inline Type type() const { return mType; }
    inline bool isValid() const { return mType != PT_NONE; }
    inline bool canBeNumber() const { return mType == PT_NUMBER || mType == PT_INTEGER; }

    inline Number getNumber(Number def = Number(0), bool* ok = nullptr, bool exact = false) const
    {
        if (mType == PT_NUMBER) {
            if (ok)
                *ok = true;
            return mNumber;
        } else if (!exact && mType == PT_INTEGER) {
            if (ok)
                *ok = true;
            return static_cast<Number>(mInteger);
        } else {
            if (ok)
                *ok = false;
            return def;
        }
    }

    static inline SceneProperty fromNumber(Number v)
    {
        SceneProperty p(PT_NUMBER);
        p.mNumber = v;
        return p;
    }

    inline Integer getInteger(Integer def = Integer(0), bool* ok = nullptr) const
    {
        if (mType == PT_INTEGER) {
            if (ok)
                *ok = true;
            return mInteger;
        } else {
            if (ok)
                *ok = false;
            return def;
        }
    }

    static inline SceneProperty fromInteger(Integer v)
    {
        SceneProperty p(PT_INTEGER);
        p.mInteger = v;
        return p;
    }

    inline bool getBool(bool def = false, bool* ok = nullptr) const
    {
        if (mType == PT_BOOL) {
            if (ok)
                *ok = true;
            return mBool;
        } else {
            if (ok)
                *ok = false;
            return def;
        }
    }

    static inline SceneProperty fromBool(bool b)
    {
        SceneProperty p(PT_BOOL);
        p.mBool = b;
        return p;
    }

    inline const Vector2f& getVector2(const Vector2f& def = Vector2f(0, 0), bool* ok = nullptr) const
    {
        if (mType == PT_VECTOR2) {
            if (ok)
                *ok = true;
            return mVector2;
        } else {
            if (ok)
                *ok = false;
            return def;
        }
    }

    static inline SceneProperty fromVector2(const Vector2f& v)
    {
        SceneProperty p(PT_VECTOR2);
        p.mVector2 = v;
        return p;
    }

    inline const Vector3f& getVector3(const Vector3f& def = Vector3f(0, 0, 0), bool* ok = nullptr) const
    {
        if (mType == PT_VECTOR3) {
            if (ok)
                *ok = true;
            return mVector3;
        } else {
            if (ok)
                *ok = false;
            return def;
        }
    }

    static inline SceneProperty fromVector3(const Vector3f& v)
    {
        SceneProperty p(PT_VECTOR3);
        p.mVector3 = v;
        return p;
    }

    inline const Transformf& getTransform(const Transformf& def = Transformf::Identity(), bool* ok = nullptr) const
    {
        if (mType == PT_TRANSFORM) {
            if (ok)
                *ok = true;
            return mTransform;
        } else {
            if (ok)
                *ok = false;
            return def;
        }
    }

    static inline SceneProperty fromTransform(const Transformf& v)
    {
        SceneProperty p(PT_TRANSFORM);
        p.mTransform = v;
        return p;
    }

    inline const std::string& getString(const std::string& def = "", bool* ok = nullptr) const
    {
        if (mType == PT_STRING) {
            if (ok)
                *ok = true;
            return mString;
        } else {
            if (ok)
                *ok = false;
            return def;
        }
    }

    static inline SceneProperty fromString(const std::string& v)
    {
        SceneProperty p(PT_STRING);
        p.mString = v;
        return p;
    }

    inline const IntegerArray& getIntegerArray(bool* ok = nullptr) const
    {
        if (mType == PT_INTEGER_ARRAY) {
            if (ok)
                *ok = true;
        } else {
            if (ok)
                *ok = false;
        }
        return mIntegerArray;
    }

    inline IntegerArray&& acquireIntegerArray(bool* ok = nullptr)
    {
        if (mType == PT_INTEGER_ARRAY) {
            if (ok)
                *ok = true;
        } else {
            if (ok)
                *ok = false;
        }
        return std::move(mIntegerArray);
    }

    static inline SceneProperty fromIntegerArray(const IntegerArray& v)
    {
        SceneProperty p(PT_INTEGER_ARRAY);
        p.mIntegerArray = v;
        return p;
    }

    static inline SceneProperty fromIntegerArray(IntegerArray&& v)
    {
        SceneProperty p(PT_INTEGER_ARRAY);
        p.mIntegerArray = std::move(v);
        return p;
    }

    inline const NumberArray& getNumberArray(bool* ok = nullptr) const
    {
        if (mType == PT_NUMBER_ARRAY) {
            if (ok)
                *ok = true;
        } else {
            if (ok)
                *ok = false;
        }
        return mNumberArray;
    }

    inline NumberArray&& acquireNumberArray(bool* ok = nullptr)
    {
        if (mType == PT_NUMBER_ARRAY) {
            if (ok)
                *ok = true;
        } else {
            if (ok)
                *ok = false;
        }
        return std::move(mNumberArray);
    }

    static inline SceneProperty fromNumberArray(const NumberArray& v)
    {
        SceneProperty p(PT_NUMBER_ARRAY);
        p.mNumberArray = v;
        return p;
    }

    static inline SceneProperty fromNumberArray(NumberArray&& v)
    {
        SceneProperty p(PT_NUMBER_ARRAY);
        p.mNumberArray = std::move(v);
        return p;
    }

private:
    inline explicit SceneProperty(Type type)
        : mType(type)
    {
    }

    Type mType;

    // Data Types
    union {
        Number mNumber;
        Integer mInteger;
        bool mBool;
    };
    std::string mString;
    Vector3f mVector3;
    Vector2f mVector2;
    Transformf mTransform;
    IntegerArray mIntegerArray;
    NumberArray mNumberArray;
};

} // namespace IG