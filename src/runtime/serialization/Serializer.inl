#pragma once

namespace IG {
inline bool Serializer::isReadMode() const { return mReadMode; }

inline void Serializer::writeRawLooped(const uint8* data, size_t size)
{
    size_t total = 0;
    while (total < size) {
        size_t written = writeRaw(data + total, size - total);
        if (written == 0)
            return; // ERROR??
        total += written;
    }
}

inline void Serializer::write(bool v)
{
    uint8 tmp = v ? 1 : 0;
    write(tmp);
}

inline void Serializer::write(int8 v) { write((uint8)v); }
inline void Serializer::write(uint8 v)
{
    writeRaw(&v, sizeof(uint8));
}

inline void Serializer::write(int16 v) { write((uint16)v); }
inline void Serializer::write(uint16 v)
{
    writeRawLooped(reinterpret_cast<uint8*>(&v), sizeof(uint16));
}

inline void Serializer::write(int32 v) { write((uint32)v); }
inline void Serializer::write(uint32 v)
{
    writeRawLooped(reinterpret_cast<uint8*>(&v), sizeof(uint32));
}

inline void Serializer::write(int64 v) { write((uint64)v); }
inline void Serializer::write(uint64 v)
{
    writeRawLooped(reinterpret_cast<uint8*>(&v), sizeof(uint64));
}

inline void Serializer::write(float v)
{
    writeRawLooped(reinterpret_cast<uint8*>(&v), sizeof(float));
}

inline void Serializer::write(double v)
{
    writeRawLooped(reinterpret_cast<uint8*>(&v), sizeof(double));
}

inline void Serializer::write(const char* v)
{
    for (size_t i = 0; v[i]; ++i) {
        write((uint8)v[i]);
    }
    write((uint8)0);
}

inline void Serializer::write(const std::string& v)
{
    write(v.c_str());
}

inline void Serializer::write(const std::wstring& v)
{
    const wchar_t* data = v.c_str();
    for (size_t i = 0; data[i]; ++i) {
        write((wchar_t)data[i]);
    }
    write((wchar_t)0);
}

template <typename T, typename Alloc>
inline std::enable_if_t<is_trivial_serializable<T>::value, void>
Serializer::write(const std::vector<T, Alloc>& vec, bool naked)
{
    if (!naked)
        write((uint64)vec.size());
    writeRawLooped(reinterpret_cast<const uint8*>(vec.data()),
                   vec.size() * sizeof(T));
}

template <typename T, typename Alloc>
inline std::enable_if_t<!is_trivial_serializable<T>::value, void>
Serializer::write(const std::vector<T, Alloc>& vec, bool naked)
{
    if (!naked)
        write((uint64)vec.size());
    for (size_t i = 0; i < vec.size(); ++i)
        write(vec.at(i));
}

template <typename T1, typename T2>
inline void Serializer::write(const std::unordered_map<T1, T2>& map)
{
    write((uint64)map.size());
    for (auto it = map.begin(); it != map.end(); ++it) {
        write(it->first);
        write(it->second);
    }
}

template <typename Derived>
inline void Serializer::write(const Eigen::MatrixBase<Derived>& v, bool colMajor)
{
    if (!colMajor) {
        for (int r = 0; r < v.rows(); ++r)
            for (int c = 0; c < v.cols(); ++c)
                write(v(r, c));
    } else {
        for (int c = 0; c < v.cols(); ++c)
            for (int r = 0; r < v.rows(); ++r)
                write(v(r, c));
    }
}

inline void Serializer::write(const ISerializable& v)
{
    const_cast<ISerializable&>(v).serialize(*this);
}

template <typename T, typename Alloc>
void Serializer::writeAligned(const std::vector<T, Alloc>& vec, size_t alignment, bool naked)
{
    if (alignment == 0) {
        write(vec, naked);
        return;
    }

    // TODO: Faster approach?
    constexpr size_t TSize = sizeof(T);
    const size_t defect    = alignment - (TSize % alignment);

    if (defect == 0) {
        write(vec, naked);
        return;
    }

    uint8_t _tmp = 0;

    if (!naked)
        write((uint64)vec.size());
    for (size_t i = 0; i < vec.size(); ++i) {
        write(vec.at(i));

        for (size_t j = 0; j < defect; ++j)
            write(_tmp);
    }
}

inline void Serializer::writeAlignmentPad(size_t alignment)
{
    const size_t defect = alignment - (currentSize() % alignment);

    uint8_t _tmp = 0;
    for (size_t i = 0; i < defect; ++i)
        write(_tmp);
}

//////////////////////////////////////////////////////////

inline void Serializer::readRawLooped(uint8* data, size_t size)
{
    size_t total = 0;
    while (total < size) {
        size_t read = readRaw(data + total, size - total);
        if (read == 0)
            return; // ERROR??
        total += read;
    }
}

inline void Serializer::read(bool& v)
{
    uint8 tmp;
    read(tmp);
    v = (tmp != 0);
}

inline void Serializer::read(int8& v)
{
    uint8 tmp;
    read(tmp);
    v = static_cast<int8>(tmp);
}

inline void Serializer::read(uint8& v)
{
    readRaw(&v, sizeof(uint8));
}

inline void Serializer::read(int16& v)
{
    uint16 tmp;
    read(tmp);
    v = static_cast<int8>(tmp);
}

inline void Serializer::read(uint16& v)
{
    readRawLooped(reinterpret_cast<uint8*>(&v), sizeof(uint16));
}

inline void Serializer::read(int32& v)
{
    uint32 tmp;
    read(tmp);
    v = static_cast<int8>(tmp);
}

inline void Serializer::read(uint32& v)
{
    readRawLooped(reinterpret_cast<uint8*>(&v), sizeof(uint32));
}

inline void Serializer::read(int64& v)
{
    uint64 tmp;
    read(tmp);
    v = static_cast<int8>(tmp);
}

inline void Serializer::read(uint64& v)
{
    readRawLooped(reinterpret_cast<uint8*>(&v), sizeof(uint64));
}

inline void Serializer::read(float& v)
{
    readRawLooped(reinterpret_cast<uint8*>(&v), sizeof(float));
}

inline void Serializer::read(double& v)
{
    readRawLooped(reinterpret_cast<uint8*>(&v), sizeof(double));
}

inline void Serializer::read(std::string& v)
{
    v.clear();

    uint8 c;
    read(c);
    for (; c != 0; read(c)) {
        v += static_cast<uint8>(c);
    }
}

inline void Serializer::read(std::wstring& v)
{
    v.clear();

    uint32 c;
    read(c);
    for (; c != 0; read(c)) {
        v += static_cast<wchar_t>(c);
    }
}

inline void Serializer::read(ISerializable& v)
{
    v.serialize(*this);
}

template <typename T, typename Alloc>
inline std::enable_if_t<is_trivial_serializable<T>::value, void>
Serializer::read(std::vector<T, Alloc>& vec)
{
    uint64 size;
    read(size);
    vec.resize(size);
    readRawLooped(reinterpret_cast<uint8*>(vec.data()), vec.size() * sizeof(T));
}

template <typename T, typename Alloc>
inline std::enable_if_t<!is_trivial_serializable<T>::value, void>
Serializer::read(std::vector<T, Alloc>& vec)
{
    uint64 size;
    read(size);
    vec.resize(size);

    T tmp;
    for (size_t i = 0; i < size; ++i) {
        read(tmp);
        vec[i] = tmp;
    }
}

template <typename T1, typename T2>
inline void Serializer::read(std::unordered_map<T1, T2>& map)
{
    map.clear();

    uint64 size;
    read(size);

    T1 t1;
    T2 t2;
    for (size_t i = 0; i < size; ++i) {
        read(t1);
        read(t2);
        map[t1] = t2;
    }

    PR_ASSERT(map.size() == size, "Given size is not same as produced one!");
}

template <typename Derived>
inline void Serializer::read(Eigen::MatrixBase<Derived>& v, bool colMajor)
{
    if (!colMajor) {
        for (int r = 0; r < v.rows(); ++r)
            for (int c = 0; c < v.cols(); ++c)
                read(v(r, c));
    } else {
        for (int c = 0; c < v.cols(); ++c)
            for (int r = 0; r < v.rows(); ++r)
                read(v(r, c));
    }
}

////////////////////////////////////////////////////

#define IG_SER_OPERATOR(type)                              \
    inline Serializer& operator|(Serializer& ser, type& v) \
    {                                                      \
        if (ser.isReadMode())                              \
            ser.read(v);                                   \
        else                                               \
            ser.write(v);                                  \
        return ser;                                        \
    }

#define IG_SER_OPERATOR_T2(type, t1, t2)                           \
    inline Serializer& operator|(Serializer& ser, type<t1, t2>& v) \
    {                                                              \
        if (ser.isReadMode())                                      \
            ser.read(v);                                           \
        else                                                       \
            ser.write(v);                                          \
        return ser;                                                \
    }

#define IG_SER_OPERATOR_T3(type, t1, t2, t3)                           \
    inline Serializer& operator|(Serializer& ser, type<t1, t2, t3>& v) \
    {                                                                  \
        if (ser.isReadMode())                                          \
            ser.read(v);                                               \
        else                                                           \
            ser.write(v);                                              \
        return ser;                                                    \
    }

#define IG_SER_OPERATOR_T4(type, t1, t2, t3, t4)                           \
    inline Serializer& operator|(Serializer& ser, type<t1, t2, t3, t4>& v) \
    {                                                                      \
        if (ser.isReadMode())                                              \
            ser.read(v);                                                   \
        else                                                               \
            ser.write(v);                                                  \
        return ser;                                                        \
    }

IG_SER_OPERATOR(bool)
IG_SER_OPERATOR(int8)
IG_SER_OPERATOR(uint8)
IG_SER_OPERATOR(int16)
IG_SER_OPERATOR(uint16)
IG_SER_OPERATOR(int32)
IG_SER_OPERATOR(uint32)
IG_SER_OPERATOR(int64)
IG_SER_OPERATOR(uint64)
IG_SER_OPERATOR(float)
IG_SER_OPERATOR(double)
IG_SER_OPERATOR(std::string)
IG_SER_OPERATOR(ISerializable)

template <typename T, typename Alloc>
IG_SER_OPERATOR_T2(std::vector, T, Alloc)
template <typename T1, typename T2>
IG_SER_OPERATOR_T2(std::unordered_map, T1, T2)

template <typename Scalar, int Rows, int Cols, int Options>
IG_SER_OPERATOR_T4(Eigen::Matrix, Scalar, Rows, Cols, Options)

#undef IG_SER_OPERATOR
#undef IG_SER_OPERATOR_T2
#undef IG_SER_OPERATOR_T3
#undef IG_SER_OPERATOR_T4

} // namespace IG