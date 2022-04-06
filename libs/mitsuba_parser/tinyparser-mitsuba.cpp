#include "LumenPCH.h"
#include "tinyparser-mitsuba.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include <tinyxml2/tinyxml2.h>

namespace TPM_NAMESPACE {
using LookupPaths		= std::vector<std::string>;
using ArgumentContainer = std::unordered_map<std::string, std::string>;

// ------------- File IO
static inline bool doesFileExist(const std::string& fileName)
{
	return std::ifstream(fileName).good();
}

static inline std::string concactPaths(const std::string& a, const std::string& b)
{
	if (a.empty())
		return b;
	else if (b.empty())
		return a;

	if (a.back() != '/' && a.back() != '\\' && a.back() != b.front())
		return a + '/' + b;
	else
		return a + b;
}

static inline std::string extractDirectoryOfPath(const std::string& str)
{
	size_t found = str.find_last_of("/\\");
	return (found == std::string::npos) ? "" : str.substr(0, found);
}

static inline std::string resolvePath(const std::string& path, const LookupPaths& lookups)
{
	for (const auto& dir : lookups) {
		const std::string p = concactPaths(dir, path);
		if (doesFileExist(p))
			return p;
	}

	if (doesFileExist(path))
		return path;
	else
		return "";
}

// ------------- String stuff
static inline std::string handleCamelCase(const std::string& camelCase)
{
	std::string str(1, tolower(camelCase[0]));

	// First place underscores between contiguous lower and upper case letters.
	// For example, `_LowerCamelCase` becomes `_Lower_Camel_Case`.
	for (auto it = camelCase.begin() + 1; it != camelCase.end(); ++it) {
		if (isupper(*it) && *(it - 1) != '_' && islower(*(it - 1))) {
			str += "_";
		}
		str += *it;
	}

	// Then convert it to lower case.
	std::transform(str.begin(), str.end(), str.begin(), ::tolower);

	return str;
}

// Simple function to ease other parts
static inline std::string convertCC(const std::string& camelCase, bool select)
{
	return select ? handleCamelCase(camelCase) : camelCase;
}

// ------------- Vector Math
static inline Vector normalize(const Vector& v)
{
	const Number n = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
	return Vector(v.x / n, v.y / n, v.z / n);
}

static inline Vector cross(const Vector& a, const Vector& b)
{
	return Vector(a.y * b.z - a.z * b.y,
				  a.z * b.x - a.x * b.z,
				  a.x * b.y - a.y * b.x);
}

// ------------ Transform
Transform Transform::fromIdentity()
{
	return Transform(Transform::Array{ { Number(1), Number(0), Number(0), Number(0),
										 Number(0), Number(1), Number(0), Number(0),
										 Number(0), Number(0), Number(1), Number(0),
										 Number(0), Number(0), Number(0), Number(1) } });
}

Transform Transform::fromTranslation(const Vector& delta)
{
	return Transform(Transform::Array{ { Number(1), Number(0), Number(0), delta.x,
										 Number(0), Number(1), Number(0), delta.y,
										 Number(0), Number(0), Number(1), delta.z,
										 Number(0), Number(0), Number(0), Number(1) } });
}
Transform Transform::fromScale(const Vector& scale)
{
	return Transform(Transform::Array{ { scale.x, Number(0), Number(0), Number(0),
										 Number(0), scale.y, Number(0), Number(0),
										 Number(0), Number(0), scale.z, Number(0),
										 Number(0), Number(0), Number(0), Number(1) } });
}

Transform Transform::fromRotation(const Vector& axis, Number angle_degree)
{
	const auto angle_rad = degToRad(angle_degree);
	const Vector aa		 = normalize(axis);
	const auto sa		 = std::sin(angle_rad);
	const auto ca		 = std::cos(angle_rad);
	const auto nca		 = Number(1) - ca;

	return Transform(Transform::Array{ { ca + aa.x * aa.x * nca, aa.x * aa.y * nca - aa.z * sa, aa.x * aa.z * nca + aa.y * sa, Number(0),
										 aa.y * aa.x * nca + aa.z * sa, ca + aa.y * aa.y * nca, aa.y * aa.z * nca - aa.x * sa, Number(0),
										 aa.z * aa.x * nca - aa.y * sa, aa.z * aa.y * nca + aa.x * sa, ca + aa.z * aa.z * nca, Number(0),
										 Number(0), Number(0), Number(0), Number(1) } });
}

Transform Transform::fromLookAt(const Vector& origin, const Vector& target, const Vector& up)
{
	const Vector fwd	= normalize(Vector(target.x - origin.x, target.y - origin.y, target.z - origin.z));
	const Vector left	= normalize(cross(up, fwd));
	const Vector alt_up = normalize(cross(fwd, left));

	return Transform(Transform::Array{ { left.x, alt_up.x, fwd.x, origin.x,
										 left.y, alt_up.y, fwd.y, origin.y,
										 left.z, alt_up.z, fwd.z, origin.z,
										 Number(0), Number(0), Number(0), Number(1) } });
}

Transform Transform::multiplyFromRight(const Transform& other) const
{
	Transform result;

	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			Number sum = 0;
			for (int k = 0; k < 4; ++k)
				sum += (*this)(i, k) * other(k, j);
			result(i, j) = sum;
		}
	}

	return result;
}

// ------------- Basic Parser
template <typename T, typename Func>
inline static int _parseScalars(const std::string& str, T* numbers, int amount, Func func)
{
	size_t offset = 0;
	int counter	  = 0;
	for (counter = 0; counter < amount && offset < str.size(); ++counter) {
		try {
			size_t off;
			numbers[counter] = func(str.substr(offset), &off);
			offset += off;
			if (offset >= str.size())
				return counter + 1;
			if (std::ispunct(str[offset]))
				offset += 1;
		} catch (...) {
			return counter;
		}
	}

	return counter;
}

inline static Integer _parseInteger(const std::string& str, Integer* numbers, int amount)
{
	return _parseScalars(str, numbers, amount,
						 [](const std::string& s, size_t* idx) { return (Integer)std::stoll(s, idx); });
}

inline static int _parseNumber(const std::string& str, Number* numbers, int amount)
{
	return _parseScalars(str, numbers, amount,
						 [](const std::string& s, size_t* idx) { return std::stof(s, idx); });
}

static void _parseVersion(const char* v, int& major, int& minor, int& patch)
{
	major = 0;
	minor = 6;
	patch = 0;

	if (!v)
		return;

	std::string version = v;
	const auto s1		= version.find_first_of('.');

	major = std::stoi(version.substr(0, s1));

	if (s1 == std::string::npos) {
		if (major > 0) {
			minor = 0;
			patch = 0;
		}
		return;
	}

	const auto s2 = version.find_first_of('.', s1 + 1);
	minor		  = std::stoi(version.substr(s1 + 1, s2));

	if (s2 == std::string::npos)
		return;

	patch = std::stoi(version.substr(s2 + 1));
}

static std::string unpackValues(const char* str, const ArgumentContainer& cnt)
{
	std::string unpackedStr;
	for (int i = 0; str[i];) {
		if (str[i] == '$') {
			std::string variable;
			++i;
			for (; str[i] && std::isalnum(str[i]); ++i) {
				variable += str[i];
			}
			if (!variable.empty()) {
				if (!cnt.count(variable))
					throw std::runtime_error("Unknown variable " + variable);

				unpackedStr += cnt.at(variable);
			}
		} else {
			unpackedStr += str[i];
			++i;
		}
	}
	return unpackedStr;
}

static inline bool unpackInteger(const char* str, const ArgumentContainer& cnt, Integer* value)
{
	if (!str)
		return false;

	const std::string valueStr = unpackValues(str, cnt);
	return _parseInteger(valueStr, value, 1) == 1;
}

static inline bool unpackNumber(const char* str, const ArgumentContainer& cnt, Number* value)
{
	if (!str)
		return false;

	const std::string valueStr = unpackValues(str, cnt);
	return _parseNumber(valueStr, value, 1) == 1;
}

static inline bool unpackVector(const char* str, const ArgumentContainer& cnt, Vector* value, Number fill = Number(0))
{
	if (!str)
		return false;

	const std::string valueStr = unpackValues(str, cnt);
	Number tmp[3];
	auto c = _parseNumber(valueStr, tmp, 3);
	if (c >= 1) {
		value->x = tmp[0];
		value->y = c >= 2 ? tmp[1] : fill;
		value->z = c >= 3 ? tmp[2] : fill;
		return true;
	} else {
		return false;
	}
}

//--------------- ID Container
class IDContainer {
public:
	inline void registerID(const std::string& id, const std::shared_ptr<Object>& entity)
	{
		mMap[id] = entity;
	}

	inline bool hasID(const std::string& id) const { return mMap.count(id) > 0; }

	inline std::shared_ptr<Object> get(const std::string& id) const
	{
		return hasID(id) ? mMap.at(id) : nullptr;
	}

	inline void makeAlias(const std::string& id, const std::string& as)
	{
		if (!mMap.count(id))
			return;

		mMap[as] = mMap.at(id);
	}

private:
	std::unordered_map<std::string, std::shared_ptr<Object>> mMap;
};

// Object type to parser flag
#define OT_PF(x) (1 << (x))

enum ParseFlags {
	PF_BSDF		  = OT_PF(OT_BSDF),
	PF_EMITTER	  = OT_PF(OT_EMITTER),
	PF_FILM		  = OT_PF(OT_FILM),
	PF_INTEGRATOR = OT_PF(OT_INTEGRATOR),
	PF_MEDIUM	  = OT_PF(OT_MEDIUM),
	PF_PHASE	  = OT_PF(OT_PHASE),
	PF_RFILTER	  = OT_PF(OT_RFILTER),
	PF_SAMPLER	  = OT_PF(OT_SAMPLER),
	PF_SENSOR	  = OT_PF(OT_SENSOR),
	PF_SHAPE	  = OT_PF(OT_SHAPE),
	PF_SUBSURFACE = OT_PF(OT_SUBSURFACE),
	PF_TEXTURE	  = OT_PF(OT_TEXTURE),
	PF_VOLUME	  = OT_PF(OT_VOLUME),

	PF_REFERENCE = OT_PF(_OT_COUNT + 0),
	PF_DEFAULT	 = OT_PF(_OT_COUNT + 1),
	PF_ALIAS	 = OT_PF(_OT_COUNT + 2),
	PF_PARAMETER = OT_PF(_OT_COUNT + 3),
	PF_INCLUDE	 = OT_PF(_OT_COUNT + 4),
	PF_NULL		 = OT_PF(_OT_COUNT + 5),

	// Compositions extracted from the official schema
	PF_C_OBJECTGROUP = PF_PARAMETER | PF_DEFAULT,
	PF_C_BSDF		 = PF_C_OBJECTGROUP | PF_PHASE | PF_TEXTURE | PF_BSDF | PF_REFERENCE,
	PF_C_EMITTER	 = PF_C_OBJECTGROUP | PF_TEXTURE | PF_EMITTER | PF_MEDIUM | PF_REFERENCE,
	PF_C_FILM		 = PF_C_OBJECTGROUP | PF_RFILTER,
	PF_C_INTEGRATOR	 = PF_C_OBJECTGROUP | PF_INTEGRATOR | PF_SAMPLER,
	PF_C_MEDIUM		 = PF_C_OBJECTGROUP | PF_SHAPE | PF_VOLUME | PF_PHASE,
	PF_C_PHASE		 = PF_C_OBJECTGROUP | PF_PHASE,
	PF_C_SAMPLER	 = PF_C_OBJECTGROUP,
	PF_C_SCENE		 = PF_C_OBJECTGROUP | PF_ALIAS | PF_SENSOR | PF_TEXTURE | PF_BSDF | PF_SUBSURFACE | PF_INTEGRATOR | PF_EMITTER | PF_SHAPE | PF_MEDIUM | PF_PHASE | PF_INCLUDE | PF_NULL,
	PF_C_SENSOR		 = PF_C_OBJECTGROUP | PF_SENSOR | PF_FILM | PF_MEDIUM | PF_SAMPLER | PF_REFERENCE,
	PF_C_SHAPE		 = PF_C_OBJECTGROUP | PF_BSDF | PF_SUBSURFACE | PF_SENSOR | PF_EMITTER | PF_SHAPE | PF_MEDIUM | PF_TEXTURE | PF_RFILTER | PF_REFERENCE,
	PF_C_SUBSURFACE	 = PF_C_OBJECTGROUP | PF_PHASE | PF_BSDF,
	PF_C_RFILTER	 = PF_C_OBJECTGROUP,
	PF_C_TEXTURE	 = PF_C_OBJECTGROUP | PF_TEXTURE | PF_RFILTER | PF_REFERENCE,
	PF_C_VOLUME		 = PF_C_OBJECTGROUP | PF_VOLUME,
};

struct ParseContext {
	const TPM_NAMESPACE::ArgumentContainer& Arguments;
	const TPM_NAMESPACE::LookupPaths& LookupPaths;
	const bool ConvertCamelCase;
};

static inline Property parseInteger(const ParseContext& ctx, const tinyxml2::XMLElement* element)
{
	Integer value;
	return unpackInteger(element->Attribute("value"), ctx.Arguments, &value) ? Property::fromInteger(value) : Property();
}

static inline Property parseFloat(const ParseContext& ctx, const tinyxml2::XMLElement* element)
{
	Number value;
	return unpackNumber(element->Attribute("value"), ctx.Arguments, &value) ? Property::fromNumber(value) : Property();
}

static inline Property parseVector(const ParseContext& ctx, const tinyxml2::XMLElement* element)
{
	auto attrib = element->Attribute("value");
	if (!attrib) { // Try legacy way
		Number x, y, z;
		if (!unpackNumber(element->Attribute("x"), ctx.Arguments, &x))
			return Property();
		if (!unpackNumber(element->Attribute("y"), ctx.Arguments, &y))
			return Property();
		if (!unpackNumber(element->Attribute("z"), ctx.Arguments, &z))
			return Property();

		return Property::fromVector(Vector(x, y, z));
	} else {
		Vector v;
		if (!unpackVector(attrib, ctx.Arguments, &v))
			return Property();

		return Property::fromVector(v);
	}
}

static inline Property parseBool(const ParseContext& ctx, const tinyxml2::XMLElement* element)
{
	auto attrib = element->Attribute("value");
	if (!attrib)
		return Property();

	const auto valueStr = unpackValues(attrib, ctx.Arguments);
	if (valueStr == "true")
		return Property::fromBool(true);
	else if (valueStr == "false")
		return Property::fromBool(false);
	else
		return Property();
}

static Property parseRGB(const ParseContext& ctx, const tinyxml2::XMLElement* element)
{
	// TODO
	auto intent = element->Attribute("intent");
	(void)intent;

	auto attrib = element->Attribute("value");
	if (!attrib) { // Try legacy way
		Number r, g, b;
		if (!unpackNumber(element->Attribute("r"), ctx.Arguments, &r))
			return Property();
		if (!unpackNumber(element->Attribute("g"), ctx.Arguments, &g))
			return Property();
		if (!unpackNumber(element->Attribute("b"), ctx.Arguments, &b))
			return Property();

		return Property::fromColor(Color(r, g, b));
	} else {
		Vector v;
		if (!unpackVector(attrib, ctx.Arguments, &v))
			return Property();

		return Property::fromColor(Color(v.x, v.y, v.z));
	}
}

static Property parseSpectrum(const ParseContext& ctx, const tinyxml2::XMLElement* element)
{
	auto intent = element->Attribute("intent");
	(void)intent;

	auto filename = element->Attribute("filename");
	if (filename) { // Load from .spd files!
		const std::string unpacked_filename = unpackValues(filename, ctx.Arguments);
		const std::string full_path			= resolvePath(unpacked_filename, ctx.LookupPaths);

		if (full_path.empty())
			throw std::runtime_error("File " + std::string(unpacked_filename) + " not found");

		std::vector<int> wvls;
		std::vector<Number> weights;
		std::ifstream stream(full_path, std::ios::in);
		std::string line;
		while (std::getline(stream, line)) {
			size_t comment_start = line.find_first_of('#');
			const auto part		 = line.substr(0, comment_start);
			if (part.empty())
				continue;

			Number tmp[2];
			int i = _parseNumber(part, tmp, 2);
			if (i == 2) {
				wvls.push_back((int)tmp[0]);
				weights.push_back(tmp[1]);
			}
		}

		return Property::fromSpectrum(Spectrum(wvls, weights));
	} else {
		auto value = element->Attribute("value");
		if (!value)
			return Property();

		const auto valueStr = unpackValues(value, ctx.Arguments);

		constexpr size_t MAX_SPEC = 1024;
		Number tmp[MAX_SPEC];
		auto c = _parseNumber(valueStr, tmp, MAX_SPEC);

		if (c == 1) { // Uniform
			return Property::fromSpectrum(Spectrum(tmp[0]));
		} else if (c % 2 == 0) { // Wavelength + Weight pairs
			std::vector<int> wvls;
			std::vector<Number> weights;
			wvls.resize(c / 2);
			weights.resize(c / 2);

			for (int i = 0; i < c / 2; ++i) {
				wvls[i]	   = tmp[i * 2];
				weights[i] = tmp[i * 2 + 1];
			}
			return Property::fromSpectrum(Spectrum(wvls, weights));
		} else {
			return Property();
		}
	}
}

static Property parseBlackbody(const ParseContext& ctx, const tinyxml2::XMLElement* element)
{
	Number temp, scale;
	if (!unpackNumber(element->Attribute("temperature"), ctx.Arguments, &temp))
		return Property();

	if (!unpackNumber(element->Attribute("scale"), ctx.Arguments, &scale))
		scale = Number(1);

	return Property::fromBlackbody(Blackbody(temp, scale));
}

static Property parseString(const ParseContext& ctx, const tinyxml2::XMLElement* element)
{
	auto attrib = element->Attribute("value");
	if (!attrib)
		return Property();
	return Property::fromString(unpackValues(attrib, ctx.Arguments));
}

// ---------- Transform Parameter
Transform parseTransformTranslate(const ParseContext& ctx, const tinyxml2::XMLElement* element)
{
	Vector delta;
	auto value = element->Attribute("value");
	if (value) {
		if (!unpackVector(value, ctx.Arguments, &delta))
			delta = Vector(0, 0, 0);
	} else {
		if (!unpackNumber(element->Attribute("x"), ctx.Arguments, &delta.x))
			delta.x = 0;
		if (!unpackNumber(element->Attribute("y"), ctx.Arguments, &delta.y))
			delta.y = 0;
		if (!unpackNumber(element->Attribute("z"), ctx.Arguments, &delta.z))
			delta.z = 0;
	}

	return Transform::fromTranslation(delta);
}

Transform parseTransformScale(const ParseContext& ctx, const tinyxml2::XMLElement* element)
{
	Vector scale;
	auto uniformScaleA = element->Attribute("value");
	if (uniformScaleA) {
		if (!unpackVector(uniformScaleA, ctx.Arguments, &scale, Number(1)))
			scale = Vector(1, 1, 1);
	} else {
		if (!unpackNumber(element->Attribute("x"), ctx.Arguments, &scale.x))
			scale.x = 1;
		if (!unpackNumber(element->Attribute("y"), ctx.Arguments, &scale.y))
			scale.y = 1;
		if (!unpackNumber(element->Attribute("z"), ctx.Arguments, &scale.z))
			scale.z = 1;
	}

	return Transform::fromScale(scale);
}

Transform parseTransformRotate(const ParseContext& ctx, const tinyxml2::XMLElement* element)
{
	Vector axis;
	auto value = element->Attribute("axis");
	if (value) {
		if (!unpackVector(value, ctx.Arguments, &axis))
			axis = Vector(0, 0, 1);
	} else {
		if (!unpackNumber(element->Attribute("x"), ctx.Arguments, &axis.x))
			axis.x = 0;
		if (!unpackNumber(element->Attribute("y"), ctx.Arguments, &axis.y))
			axis.y = 0;
		if (!unpackNumber(element->Attribute("z"), ctx.Arguments, &axis.z))
			axis.z = 1;
	}

	Number angle;
	if (!unpackNumber(element->Attribute("angle"), ctx.Arguments, &angle))
		return Transform::fromIdentity();

	return Transform::fromRotation(axis, angle);
}

Transform parseTransformLookAt(const ParseContext& ctx, const tinyxml2::XMLElement* element)
{
	Vector origin, target, up;
	if (!unpackVector(element->Attribute("origin"), ctx.Arguments, &origin))
		return Transform::fromIdentity();

	if (!unpackVector(element->Attribute("target"), ctx.Arguments, &target))
		return Transform::fromIdentity();

	if (!unpackVector(element->Attribute("up"), ctx.Arguments, &up))
		up = Vector(0, 0, 1);

	return Transform::fromLookAt(origin, target, up);
}

Transform parseTransformMatrix(const ParseContext& ctx, const tinyxml2::XMLElement* element)
{
	auto value = element->Attribute("value");
	if (!value)
		return Transform::fromIdentity();

	const auto valueStr = unpackValues(value, ctx.Arguments);
	Number tmp[16];
	auto c = _parseNumber(valueStr, tmp, 16);

	// Row-Major
	if (c == 9) {
		return Transform(Transform::Array{ { tmp[0], tmp[1], tmp[2], Number(0),
											 tmp[3], tmp[4], tmp[5], Number(0),
											 tmp[6], tmp[7], tmp[8], Number(0),
											 Number(0), Number(0), Number(0), Number(1) } });
	}
#ifndef TPM_NO_EXTENSIONS
	else if (c == 12) { // Official mitsuba does not support this
		return Transform(Transform::Array{ { tmp[0], tmp[1], tmp[2], tmp[3],
											 tmp[4], tmp[5], tmp[6], tmp[7],
											 tmp[8], tmp[9], tmp[10], tmp[11],
											 Number(0), Number(0), Number(0), Number(1) } });
	}
#endif
	else if (c == 16) {
		return Transform(Transform::Array{ { tmp[0], tmp[1], tmp[2], tmp[3],
											 tmp[4], tmp[5], tmp[6], tmp[7],
											 tmp[8], tmp[9], tmp[10], tmp[11],
											 tmp[12], tmp[13], tmp[14], tmp[15] } });
	}

	return Transform::fromIdentity();
}

using TransformParseCallback = Transform (*)(const ParseContext&, const tinyxml2::XMLElement*);
struct {
	const char* Name;
	TransformParseCallback Callback;
} _transformParseElements[] = {
	{ "translate", parseTransformTranslate },
	{ "scale", parseTransformScale },
	{ "rotate", parseTransformRotate },
	{ "lookAt", parseTransformLookAt },
	{ "lookat", parseTransformLookAt },
	{ "matrix", parseTransformMatrix },
	{ nullptr, nullptr }
};

Transform parseInnerMatrix(const ParseContext& ctx, const tinyxml2::XMLElement* element)
{
	Transform inner = Transform::fromIdentity();
	for (auto childElement = element->FirstChildElement();
		 childElement;
		 childElement = childElement->NextSiblingElement()) {

		for (int i = 0; _transformParseElements[i].Name; ++i) {
			if (strcmp(childElement->Name(), _transformParseElements[i].Name) == 0) {
				inner = _transformParseElements[i].Callback(ctx, childElement) * inner;
				break;
			}
		}
	}

	return inner;
}

static Property parseTransform(const ParseContext& ctx, const tinyxml2::XMLElement* element)
{
	const auto transform = parseInnerMatrix(ctx, element);
	return Property::fromTransform(transform);
}

static Property parseAnimation(const ParseContext& ctx, const tinyxml2::XMLElement* element)
{
	Animation anim;
	for (auto childElement = element->FirstChildElement();
		 childElement;
		 childElement = childElement->NextSiblingElement()) {
		if (strcmp(childElement->Name(), "transform") != 0)
			throw std::runtime_error("Animation entries are only of type transform");

		Number time;
		if (!unpackNumber(childElement->Attribute("time"), ctx.Arguments, &time))
			throw std::runtime_error("Animation entry missing time attribute");

		anim.addKeyFrame(time, parseInnerMatrix(ctx, childElement));
	}

	return Property::fromAnimation(anim);
}

using PropertyParseCallback = Property (*)(const ParseContext&, const tinyxml2::XMLElement*);

static const struct {
	const char* Name;
	PropertyParseCallback Callback;
} _propertyParseElement[] = {
	{ "integer", parseInteger },
	{ "float", parseFloat },
	{ "vector", parseVector },
	{ "point", parseVector },
	{ "boolean", parseBool },
	{ "string", parseString },
	{ "rgb", parseRGB },
	{ "spectrum", parseSpectrum },
	{ "blackbody", parseBlackbody },
	{ "transform", parseTransform },
	{ "animation", parseAnimation },
	{ nullptr, nullptr }
};

bool parseParameter(Object* obj, const ParseContext& ctx, const tinyxml2::XMLElement* element)
{
	auto name = element->Attribute("name");
	if (!name)
		return false;

	const std::string lcname = convertCC(name, ctx.ConvertCamelCase);
	for (int i = 0; _propertyParseElement[i].Name; ++i) {
		if (strcmp(element->Name(), _propertyParseElement[i].Name) == 0) {
			auto prop = _propertyParseElement[i].Callback(ctx, element);
			if (prop.isValid())
				obj->setProperty(lcname, prop);
			return true;
		}
	}

	return false;
}

static void handleAlias(IDContainer& idcontainer, const tinyxml2::XMLElement* element)
{
	auto id = element->Attribute("id");
	auto as = element->Attribute("as");

	if (!id || !as)
		throw std::runtime_error("Invalid alias element");

	if (!idcontainer.hasID(id))
		throw std::runtime_error("Unknown id " + std::string(id));

	if (idcontainer.hasID(as))
		throw std::runtime_error("Id " + std::string(as) + " already existent");

	idcontainer.makeAlias(id, as);
}

static void handleDefault(ArgumentContainer& cnt, const tinyxml2::XMLElement* element)
{
	auto name  = element->Attribute("name");
	auto value = element->Attribute("value");

	if (!name || !value)
		throw std::runtime_error("Invalid default element");

	if (!cnt.count(name))
		cnt[name] = value;
}

static void handleReference(Object* obj, const ParseContext& ctx, const IDContainer& ids, const tinyxml2::XMLElement* element, int flags)
{
	auto id	  = element->Attribute("id");
	auto name = element->Attribute("name");

	if (!id)
		throw std::runtime_error("Invalid ref element");

	const auto ref_id = unpackValues(id, ctx.Arguments);

	if (!ids.hasID(ref_id))
		throw std::runtime_error("Id " + ref_id + " does not exists");

	auto ref = ids.get(ref_id);

	if (flags & OT_PF(obj->type())) {
		if (name)
			obj->addNamedChild(convertCC(name, ctx.ConvertCamelCase), ref);
		else
			obj->addAnonymousChild(ref);
	} else {
		throw std::runtime_error("Id " + ref_id + " not of allowed type");
	}
}

static void parseObject(Object*, const ParseContext&, IDContainer&, const tinyxml2::XMLElement*, int);
static void handleInclude(Object* obj, const ParseContext& ctx, IDContainer& ids,
						  const tinyxml2::XMLElement* element)
{
	// TODO: Any default statement inside a include is not visible in the parent scope. But should that not be the case?

	auto filename = element->Attribute("filename");
	if (!filename)
		throw std::runtime_error("Invalid include element");

	const std::string unpacked_filename = unpackValues(filename, ctx.Arguments);
	const std::string full_path			= resolvePath(unpacked_filename, ctx.LookupPaths);

	if (full_path.empty())
		throw std::runtime_error("File " + std::string(unpacked_filename) + " not found");

	// Load xml
	tinyxml2::XMLDocument xml;
	xml.LoadFile(full_path.c_str());

	const auto rootScene = xml.RootElement();
	if (strcmp(rootScene->Name(), "scene") != 0)
		throw std::runtime_error("Expected root element to be 'scene'");

	// Ignore version

	// Parse as scene
	parseObject(obj, ctx, ids, rootScene, PF_C_SCENE);
}

static const struct {
	const char* Name;
	ObjectType Type;
	int Flags;
} _parseElements[] = {
	{ "bsdf", OT_BSDF, PF_C_BSDF },
	{ "emitter", OT_EMITTER, PF_C_EMITTER },
	{ "film", OT_FILM, PF_C_FILM },
	{ "integrator", OT_INTEGRATOR, PF_C_INTEGRATOR },
	{ "medium", OT_MEDIUM, PF_C_MEDIUM },
	{ "phase", OT_PHASE, PF_C_PHASE },
	{ "sampler", OT_SAMPLER, PF_C_SAMPLER },
	{ "sensor", OT_SENSOR, PF_C_SENSOR },
	{ "shape", OT_SHAPE, PF_C_SHAPE },
	{ "subsurface", OT_SUBSURFACE, PF_C_SUBSURFACE },
	{ "rfilter", OT_RFILTER, PF_C_RFILTER },
	{ "texture", OT_TEXTURE, PF_C_TEXTURE },
	{ "volume", OT_VOLUME, PF_C_VOLUME },
	{ nullptr, ObjectType(0), 0 }
};

static void parseObject(Object* obj, const ParseContext& ctx, IDContainer& ids, const tinyxml2::XMLElement* element, int flags)
{
	// Copy container to make sure recursive elements do not overwrite it
	ArgumentContainer cnt = ctx.Arguments;
	ParseContext nextCtx{ cnt, ctx.LookupPaths, ctx.ConvertCamelCase };

	for (auto childElement = element->FirstChildElement();
		 childElement;
		 childElement = childElement->NextSiblingElement()) {

		if ((flags & PF_PARAMETER) && parseParameter(obj, nextCtx, childElement))
			continue;

		if ((flags & PF_REFERENCE) && strcmp(childElement->Name(), "ref") == 0) {
			handleReference(obj, ctx, ids, childElement, flags);
		} else if ((flags & PF_DEFAULT) && strcmp(childElement->Name(), "default") == 0) {
			handleDefault(cnt, childElement);
		} else if ((flags & PF_INCLUDE) && strcmp(childElement->Name(), "include") == 0) {
			handleInclude(obj, nextCtx, ids, childElement);
		} else if ((flags & PF_ALIAS) && strcmp(childElement->Name(), "alias") == 0) {
			handleAlias(ids, childElement);
		} else if ((flags & PF_NULL) && strcmp(childElement->Name(), "null") == 0) {
			// Handle null
		} else {
			std::shared_ptr<Object> child;

			for (int i = 0; _parseElements[i].Name; ++i) {
				if ((OT_PF(_parseElements[i].Type) & flags)
					&& strcmp(childElement->Name(), _parseElements[i].Name) == 0) {

					auto pluginType = childElement->Attribute("type");
					auto id			= childElement->Attribute("id");
					child			= std::make_shared<Object>(_parseElements[i].Type, pluginType ? pluginType : "", id ? id : "");
					parseObject(child.get(), nextCtx, ids, childElement, _parseElements[i].Flags);
					break;
				}
			}

			if (child) {
				if (child->hasID()) {
					if (!ids.hasID(child->id())) {
						ids.registerID(child->id(), child);
					} else {
						// TODO: Warning
					}
				}

				auto name = childElement->Attribute("name");
				if (name)
					obj->addNamedChild(convertCC(name, ctx.ConvertCamelCase), child);
				else
					obj->addAnonymousChild(child);
			} else {
				std::stringstream stream;
				stream << "Found invalid tag '" << childElement->Name() << "'";
				throw std::runtime_error(stream.str());
			}
		}
	}
}

class InternalSceneLoader {
public:
	static Scene loadFromXML(const SceneLoader& loader, const tinyxml2::XMLDocument& xml)
	{
		if (xml.Error())
			throw std::runtime_error(xml.ErrorStr());

		const auto rootScene = xml.RootElement();
		if (!rootScene)
			throw std::runtime_error("Root element is null");

		if (strcmp(rootScene->Name(), "scene") != 0)
			throw std::runtime_error("Expected root element to be 'scene'");

		Scene scene;
		IDContainer idcontainer;

		try {
			_parseVersion(rootScene->Attribute("version"), scene.mVersionMajor, scene.mVersionMinor, scene.mVersionPatch);
		} catch (...) {
			throw std::runtime_error("Invalid version element");
		}

		const bool convertFromCamelCase = !loader.mDisableLowerCaseConversion && (scene.mVersionMajor == 0);
		parseObject(&scene, ParseContext{ loader.mArguments, loader.mLookupPaths, convertFromCamelCase }, idcontainer, rootScene, PF_C_SCENE);

		return scene;
	}
};

Scene SceneLoader::loadFromFile(const char* path)
{
	tinyxml2::XMLDocument xml;
	xml.LoadFile(path);

	const auto dir = extractDirectoryOfPath(path);
	if (dir.empty()) {
		return InternalSceneLoader::loadFromXML(*this, xml);
	} else {
		mLookupPaths.insert(mLookupPaths.begin(), dir);
		const auto res = InternalSceneLoader::loadFromXML(*this, xml);
		mLookupPaths.erase(mLookupPaths.begin());
		return res;
	}
}

Scene SceneLoader::loadFromString(const char* str)
{
	tinyxml2::XMLDocument xml;
	xml.Parse(str);
	return InternalSceneLoader::loadFromXML(*this, xml);
}

Scene SceneLoader::loadFromString(const char* str, size_t max_len)
{
	tinyxml2::XMLDocument xml;
	xml.Parse(str, max_len);
	return InternalSceneLoader::loadFromXML(*this, xml);
}

/*Scene SceneLoader::loadFromStream(std::istream& stream)
{
	// TODO
	return Scene();
}*/

Scene SceneLoader::loadFromMemory(const uint8_t* data, size_t size)
{
	tinyxml2::XMLDocument xml;
	xml.Parse(reinterpret_cast<const char*>(data), size);
	return InternalSceneLoader::loadFromXML(*this, xml);
}
} // namespace TPM_NAMESPACE