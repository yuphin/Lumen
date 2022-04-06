#pragma once

#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#ifdef TPM_EXPORT
#define TPM_LIB __declspec(dllexport)
#elif defined(TPM_IMPORT)
#define TPM_LIB __declspec(dllimport)
#else
#define TPM_LIB
#endif
#elif __GNUC__ >= 4
#define TPM_LIB __attribute__((visibility("default")))
#else
#define TPM_LIB
#endif

#define TPM_MAJOR_VERSION 0
#define TPM_MINOR_VERSION 1
#define TPM_PATCH_VERSION 0

#define TPM_NAMESPACE tinyparser_mitsuba

// Check for C++ features

#if __cplusplus >= 201703L // 2017
#define TPM_NODISCARD [[nodiscard]]
#endif

#ifndef TPM_NODISCARD
#define TPM_NODISCARD
#endif

#ifndef TPM_HAS_STRING_VIEW
#if __cpp_lib_string_view >= 201606L
#define TPM_HAS_STRING_VIEW
#endif
#endif

namespace TPM_NAMESPACE {
#ifdef TPM_NUMBER_AS_DOUBLE
using Number = double;
#else
using Number = float;
#endif

using Integer = int64_t;

constexpr Number TPM_PI = Number(3.14159265358979323846);
constexpr Number degToRad(Number v) { return v * TPM_PI / Number(180); }
constexpr Number radToDeg(Number v) { return v / TPM_PI * Number(180); }

// --------------- Enums
enum ObjectType {
	OT_SCENE = 0,
	OT_BSDF,
	OT_EMITTER,
	OT_FILM,
	OT_INTEGRATOR,
	OT_MEDIUM,
	OT_PHASE,
	OT_RFILTER,
	OT_SAMPLER,
	OT_SENSOR,
	OT_SHAPE,
	OT_SUBSURFACE,
	OT_TEXTURE,
	OT_VOLUME,
	_OT_COUNT
};

enum PropertyType {
	PT_NONE = 0,
	PT_ANIMATION,
	PT_BLACKBODY,
	PT_BOOL,
	PT_INTEGER,
	PT_NUMBER,
	PT_COLOR,
	PT_SPECTRUM,
	PT_STRING,
	PT_TRANSFORM,
	PT_VECTOR,
};

// --------------- Vector/Points
/// The Vector structure is only for storage. Use a fully featured math library for calculations
struct TPM_LIB Vector {
	Number x, y, z;
	Vector() = default;
	inline Vector(Number x, Number y, Number z)
		: x(x)
		, y(y)
		, z(z)
	{
	}
};
TPM_NODISCARD inline bool operator==(const Vector& a, const Vector& b)
{
	return a.x == b.x && a.y == b.y && a.z == b.z;
}
TPM_NODISCARD inline bool operator!=(const Vector& a, const Vector& b)
{
	return !(a == b);
}
using Point = Vector;

// --------------- Transform
/// The Transform structure is only for storage and internal calculations. Use a fully featured math library for your own calculations
struct TPM_LIB Transform {
	using Array = std::array<Number, 4 * 4>;
	Array matrix; // Row major

	Transform()					= default;
	Transform(const Transform&) = default;
	Transform& operator=(const Transform&) = default;
	Transform(Transform&&)				   = default;
	Transform& operator=(Transform&&) = default;

	inline explicit Transform(const Array& arr)
		: matrix(arr)
	{
	}

	inline Number& operator()(int i, int j) { return matrix[i * 4 + j]; }
	inline Number operator()(int i, int j) const { return matrix[i * 4 + j]; }

	inline Transform operator*(const Transform& other) const
	{
		return multiplyFromRight(other);
	}

	inline Transform& operator*=(const Transform& other)
	{
		*this = multiplyFromRight(other);
		return *this;
	}

	static Transform fromIdentity();
	static Transform fromTranslation(const Vector& delta);
	static Transform fromScale(const Vector& scale);
	static Transform fromRotation(const Vector& axis, Number angle_degree);
	static Transform fromLookAt(const Vector& origin, const Vector& target, const Vector& up);

private:
	Transform multiplyFromRight(const Transform& other) const;
};

// --------------- Animation
/// A list of time and transform pairs. It is not sorted in any way
struct TPM_LIB Animation {
	Animation() = default;

	inline void addKeyFrame(Number time, const Transform& t)
	{
		mTimes.push_back(time);
		mTransforms.push_back(t);
	}

	inline size_t keyFrameCount() const { return mTimes.size(); }

	inline const std::vector<Number>& keyFrameTimes() const { return mTimes; }
	inline const std::vector<Transform>& keyFrameTransforms() const { return mTransforms; }

private:
	std::vector<Number> mTimes;
	std::vector<Transform> mTransforms;
};

// --------------- Color
struct TPM_LIB Color {
	Number r, g, b;
	inline Color(Number r, Number g, Number b)
		: r(r)
		, g(g)
		, b(b)
	{
	}
};
TPM_NODISCARD inline bool operator==(const Color& a, const Color& b)
{
	return a.r == b.r && a.g == b.g && a.b == b.b;
}
TPM_NODISCARD inline bool operator!=(const Color& a, const Color& b)
{
	return !(a == b);
}

// --------------- Spectrum
class TPM_LIB Spectrum {
public:
	Spectrum() = default;
	inline explicit Spectrum(Number uniform)
	{
		mWeights.push_back(uniform);
	}

	inline Spectrum(const std::vector<int>& wavelengths, const std::vector<Number>& weights)
		: mWavelengths(wavelengths)
		, mWeights(weights)
	{
	}

	TPM_NODISCARD inline bool isUniform() const { return mWavelengths.size() == 0 && mWeights.size() == 1; }
	TPM_NODISCARD inline Number uniformValue() const { return mWeights.front(); }

	TPM_NODISCARD inline const std::vector<int>& wavelengths() const { return mWavelengths; }
	TPM_NODISCARD inline const std::vector<Number>& weights() const { return mWeights; }

private:
	std::vector<int> mWavelengths;
	std::vector<Number> mWeights;
};

// --------------- Blackbody
struct TPM_LIB Blackbody {
	Number temperature, scale;
	inline Blackbody(Number temperature, Number scale)
		: temperature(temperature)
		, scale(scale)
	{
	}
};
TPM_NODISCARD inline bool operator==(const Blackbody& a, const Blackbody& b)
{
	return a.temperature == b.temperature && a.scale == b.scale;
}
TPM_NODISCARD inline bool operator!=(const Blackbody& a, const Blackbody& b)
{
	return !(a == b);
}

// --------------- Property
class TPM_LIB Property {
public:
	inline Property()
		: mType(PT_NONE)
	{
	}

	Property(const Property& other) = default;
	Property(Property&& other)		= default;

	Property& operator=(const Property& other) = default;
	Property& operator=(Property&& other) = default;

	TPM_NODISCARD inline PropertyType type() const { return mType; }
	TPM_NODISCARD inline bool isValid() const { return mType != PT_NONE; }

	inline Number getNumber(Number def = Number(0), bool* ok = nullptr) const
	{
		if (mType == PT_NUMBER) {
			if (ok)
				*ok = true;
			return mNumber;
		} else {
			if (ok)
				*ok = false;
			return def;
		}
	}
	TPM_NODISCARD static inline Property fromNumber(Number v)
	{
		Property p(PT_NUMBER);
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
	TPM_NODISCARD static inline Property fromInteger(Integer v)
	{
		Property p(PT_INTEGER);
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
	TPM_NODISCARD static inline Property fromBool(bool b)
	{
		Property p(PT_BOOL);
		p.mBool = b;
		return p;
	}

	inline const Vector& getVector(const Vector& def = Vector(0, 0, 0), bool* ok = nullptr) const
	{
		if (mType == PT_VECTOR) {
			if (ok)
				*ok = true;
			return mVector;
		} else {
			if (ok)
				*ok = false;
			return def;
		}
	}
	TPM_NODISCARD static inline Property fromVector(const Vector& v)
	{
		Property p(PT_VECTOR);
		p.mVector = v;
		return p;
	}

	inline const Transform& getTransform(const Transform& def = Transform::fromIdentity(), bool* ok = nullptr) const
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
	TPM_NODISCARD static inline Property fromTransform(const Transform& v)
	{
		Property p(PT_TRANSFORM);
		p.mTransform = v;
		return p;
	}

	inline const Color& getColor(const Color& def = Color(0, 0, 0), bool* ok = nullptr) const
	{
		if (mType == PT_COLOR) {
			if (ok)
				*ok = true;
			return mRGB;
		} else {
			if (ok)
				*ok = false;
			return def;
		}
	}
	TPM_NODISCARD static inline Property fromColor(const Color& rgb)
	{
		Property p(PT_COLOR);
		p.mRGB = rgb;
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
	TPM_NODISCARD static inline Property fromString(const std::string& v)
	{
		Property p(PT_STRING);
		p.mString = v;
		return p;
	}

	inline const Spectrum& getSpectrum(const Spectrum& def = Spectrum(), bool* ok = nullptr) const
	{
		if (mType == PT_SPECTRUM) {
			if (ok)
				*ok = true;
			return mSpectrum;
		} else {
			if (ok)
				*ok = false;
			return def;
		}
	}
	TPM_NODISCARD static inline Property fromSpectrum(const Spectrum& spec)
	{
		Property p(PT_SPECTRUM);
		p.mSpectrum = spec;
		return p;
	}

	inline Blackbody getBlackbody(const Blackbody& def = Blackbody(6504, 1), bool* ok = nullptr) const
	{
		if (mType == PT_BLACKBODY) {
			if (ok)
				*ok = true;
			return mBlackbody;
		} else {
			if (ok)
				*ok = false;
			return def;
		}
	}
	TPM_NODISCARD static inline Property fromBlackbody(const Blackbody& v)
	{
		Property p(PT_BLACKBODY);
		p.mBlackbody = v;
		return p;
	}

	inline const Animation& getAnimation(const Animation& def = Animation(), bool* ok = nullptr) const
	{
		if (mType == PT_ANIMATION) {
			if (ok)
				*ok = true;
			return mAnimation;
		} else {
			if (ok)
				*ok = false;
			return def;
		}
	}
	TPM_NODISCARD static inline Property fromAnimation(const Animation& v)
	{
		Property p(PT_ANIMATION);
		p.mAnimation = v;
		return p;
	}

private:
	inline explicit Property(PropertyType type)
		: mType(type)
	{
	}

	PropertyType mType;

	// Data Types
	union {
		Number mNumber;
		Integer mInteger;
		bool mBool;

		Vector mVector;
		Transform mTransform;
		Color mRGB;
		Blackbody mBlackbody;
	};
	std::string mString;
	Spectrum mSpectrum;
	Animation mAnimation;
};

// --------------- Object
class TPM_LIB Object {
public:
	inline explicit Object(ObjectType type, const std::string& pluginType, const std::string& id)
		: mType(type)
		, mPluginType(pluginType)
		, mID(id)
	{
	}

	Object(const Object& other) = default;
	Object(Object&& other)		= default;

	Object& operator=(const Object& other) = default;
	Object& operator=(Object&& other) = default;

	TPM_NODISCARD inline ObjectType type() const { return mType; }
	TPM_NODISCARD inline const std::string& pluginType() const { return mPluginType; }
	TPM_NODISCARD inline const std::string& id() const { return mID; }

	TPM_NODISCARD inline bool hasPluginType() const { return !mPluginType.empty(); }
	TPM_NODISCARD inline bool hasID() const { return !mID.empty(); }

	TPM_NODISCARD inline Property property(const std::string& key) const
	{
		return mProperties.count(key) ? mProperties.at(key) : Property();
	}

	inline void setProperty(const std::string& key, const Property& prop) { mProperties[key] = prop; }
	TPM_NODISCARD inline const std::unordered_map<std::string, Property>& properties() const { return mProperties; }

	TPM_NODISCARD inline Property& operator[](const std::string& key) { return mProperties[key]; }
	TPM_NODISCARD inline Property operator[](const std::string& key) const { return property(key); }

	inline void addAnonymousChild(const std::shared_ptr<Object>& obj) { mChildren.push_back(obj); }
	TPM_NODISCARD inline const std::vector<std::shared_ptr<Object>>& anonymousChildren() const { return mChildren; }

	inline void addNamedChild(const std::string& key, const std::shared_ptr<Object>& obj) { mNamedChildren[key] = obj; }
	TPM_NODISCARD inline const std::unordered_map<std::string, std::shared_ptr<Object>>& namedChildren() const { return mNamedChildren; }
	TPM_NODISCARD inline std::shared_ptr<Object> namedChild(const std::string& key) const
	{
		return mNamedChildren.count(key) ? mNamedChildren.at(key) : nullptr;
	}

private:
	ObjectType mType;
	std::string mPluginType;
	std::string mID;
	std::unordered_map<std::string, Property> mProperties;
	std::vector<std::shared_ptr<Object>> mChildren;
	std::unordered_map<std::string, std::shared_ptr<Object>> mNamedChildren;
};

// --------------- Scene
class TPM_LIB Scene : public Object {
	friend class InternalSceneLoader;

public:
	Scene(const Scene& other) = default;
	Scene(Scene&& other)	  = default;

	Scene& operator=(const Scene& other) = default;
	Scene& operator=(Scene&& other) = default;

	TPM_NODISCARD inline int versionMajor() const { return mVersionMajor; }
	TPM_NODISCARD inline int versionMinor() const { return mVersionMinor; }
	TPM_NODISCARD inline int versionPatch() const { return mVersionPatch; }

private:
	inline Scene()
		: Object(OT_SCENE, "", "")
	{
	}

	int mVersionMajor;
	int mVersionMinor;
	int mVersionPatch;
};

// --------------- SceneLoader
class TPM_LIB SceneLoader {
	friend class InternalSceneLoader;

public:
	inline SceneLoader() = default;

	TPM_NODISCARD inline Scene loadFromFile(const std::string& path)
	{
		return loadFromFile(path.c_str());
	}

	TPM_NODISCARD inline Scene loadFromString(const std::string& str)
	{
		return loadFromString(str.c_str());
	}

#ifdef TPM_HAS_STRING_VIEW
	TPM_NODISCARD inline Scene loadFromString(const std::string_view& str)
	{
		return loadFromString(str.data(), str.size());
	}
#endif

	// TPM_NODISCARD static Scene loadFromStream(std::istream& stream);

	TPM_NODISCARD Scene loadFromFile(const char* path);
	TPM_NODISCARD Scene loadFromString(const char* str);
	TPM_NODISCARD Scene loadFromString(const char* str, size_t max_len);
	TPM_NODISCARD Scene loadFromMemory(const uint8_t* data, size_t size);

	inline void addLookupDir(const std::string& path)
	{
		mLookupPaths.push_back(path);
	}

	inline void addArgument(const std::string& key, const std::string& value)
	{
		mArguments[key] = value;
	}

	inline void disableLowerCaseConversion(bool b = true) { mDisableLowerCaseConversion = b; }
	inline bool isLowerCaseConversionDisabled() const { return mDisableLowerCaseConversion; }

private:
	std::vector<std::string> mLookupPaths;
	std::unordered_map<std::string, std::string> mArguments;
	bool mDisableLowerCaseConversion = false;
};
} // namespace TPM_NAMESPACE