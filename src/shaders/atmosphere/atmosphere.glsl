// Copyright (c) 2021 Felix Westin
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef ATMOSPHERE_INCLUDED
#define ATMOSPHERE_INCLUDED

// -------------------------------------
// Defines
#define ATMOSPHERE_EPS                 1e-6
#define ATMOSPHERE_PI                  3.14159265359
#define ATMOSPHERE_INFINITY            1.0 / 0.0
#define ATMOSPHERE_PLANET_RADIUS       6371000
#define ATMOSPHERE_PLANET_CENTER       vec3(0, -ATMOSPHERE_PLANET_RADIUS, 0)
#define ATMOSPHERE_ATMOSPHERE_HEIGHT   100000
#define ATMOSPHERE_RAYLEIGH_HEIGHT     (ATMOSPHERE_ATMOSPHERE_HEIGHT * 0.08)
#define ATMOSPHERE_MIE_HEIGHT          (ATMOSPHERE_ATMOSPHERE_HEIGHT * 0.012)

// -------------------------------------
// Coefficients
#define ATMOSPHERE_C_RAYLEIGH          (vec3(5.802, 13.558, 33.100) * 1e-6)
#define ATMOSPHERE_C_MIE               (vec3(3.996,  3.996,  3.996) * 1e-6)
#define ATMOSPHERE_C_OZONE             (vec3(0.650,  1.881,  0.085) * 1e-6)

#define ATMOSPHERE_ATMOSPHERE_DENSITY  1
#define ATMOSPHERE_EXPOSURE            20

// -------------------------------------
// Math
vec2 sphere_intersection (vec3 rayStart, vec3 rayDir, vec3 sphereCenter, float sphereRadius)
{
	rayStart -= sphereCenter;
	float a = dot(rayDir, rayDir);
	float b = 2.0 * dot(rayStart, rayDir);
	float c = dot(rayStart, rayStart) - (sphereRadius * sphereRadius);
	float d = b * b - 4 * a * c;
	if (d < 0)
	{
		return vec2(-1);
	}
	else
	{
		d = sqrt(d);
		return vec2(-b - d, -b + d) / (2 * a);
	}
}
vec2 planet_intersection (vec3 rayStart, vec3 rayDir)
{
	return sphere_intersection(rayStart, rayDir, ATMOSPHERE_PLANET_CENTER * vec3(0, 1.00001, 0), ATMOSPHERE_PLANET_RADIUS);
}
vec2 atmosphere_intersection (vec3 rayStart, vec3 rayDir)
{
	return sphere_intersection(rayStart, rayDir, ATMOSPHERE_PLANET_CENTER, ATMOSPHERE_PLANET_RADIUS + ATMOSPHERE_ATMOSPHERE_HEIGHT);
}

// -------------------------------------
// Phase functions
float PhaseRayleigh (float costh)
{
	return 3 * (1 + costh*costh) / (16 * ATMOSPHERE_PI);
}
float PhaseMie (float costh, float g)
{
	g = min(g, 0.9381);
	float k = 1.55*g - 0.55*g*g*g;
	float kcosth = k*costh;
	return (1 - k*k) / ((4 * ATMOSPHERE_PI) * (1-kcosth) * (1-kcosth));
}

// -------------------------------------
// Atmosphere
float AtmosphereHeight (vec3 positionWS)
{
	return distance(positionWS, ATMOSPHERE_PLANET_CENTER) - ATMOSPHERE_PLANET_RADIUS;
}
float DensityRayleigh (float h)
{
	return exp(-max(0, h / ATMOSPHERE_RAYLEIGH_HEIGHT));
}
float DensityMie (float h)
{
	return exp(-max(0, h / ATMOSPHERE_MIE_HEIGHT));
}
float DensityOzone (float h)
{
	// The ozone layer is represented as a tent function with a width of 30km, centered around an altitude of 25km.
	return max(0, 1 - abs(h - 25000.0) / 15000.0);
}
vec3 AtmosphereDensity (float h)
{
	return vec3(DensityRayleigh(h), DensityMie(h), DensityOzone(h));
}

// Optical depth is a unitless measurement of the amount of absorption of a participating medium (such as the atmosphere).
// This function calculates just that for our three atmospheric elements:
// R: Rayleigh
// G: Mie
// B: Ozone
// If you find the term "optical depth" confusing, you can think of it as "how much density was found along the ray in total".
vec3 IntegrateOpticalDepth (vec3 rayStart, vec3 rayDir)
{
	vec2 intersection = atmosphere_intersection(rayStart, rayDir);
	float  rayLength    = intersection.y;

	int    sampleCount  = 8;
	float  stepSize     = rayLength / sampleCount;
	
	vec3 opticalDepth = vec3(0.0);

	for (int i = 0; i < sampleCount; i++)
	{
		vec3 localPosition = rayStart + rayDir * (i + 0.5) * stepSize;
		float  localHeight   = AtmosphereHeight(localPosition);
		vec3 localDensity  = AtmosphereDensity(localHeight);

		opticalDepth += localDensity * stepSize;
	}

	return opticalDepth;
}

// Calculate a luminance transmittance value from optical depth.
vec3 Absorb (vec3 opticalDepth)
{
	// Note that Mie results in slightly more light absorption than scattering, about 10%
	return exp(-(opticalDepth.x * ATMOSPHERE_C_RAYLEIGH + opticalDepth.y * ATMOSPHERE_C_MIE * 1.1 + opticalDepth.z * ATMOSPHERE_C_OZONE) * ATMOSPHERE_ATMOSPHERE_DENSITY);
}

// Integrate scattering over a ray for a single directional light source.
// Also return the transmittance for the same ray as we are already calculating the optical depth anyway.
vec3 integrate_scattering (vec3 rayStart, vec3 rayDir, float rayLength, vec3 lightDir, vec3 lightColor, out vec3 transmittance)
{
	// We can reduce the number of atmospheric samples required to converge by spacing them exponentially closer to the camera.
	// This breaks space view however, so let's compensate for that with an exponent that "fades" to 1 as we leave the atmosphere.
	float  rayHeight = AtmosphereHeight(rayStart);
	float  sampleDistributionExponent = 1 + clamp(1 - rayHeight / ATMOSPHERE_ATMOSPHERE_HEIGHT, 0, 1) * 8; // Slightly arbitrary max exponent of 9

	vec2 intersection = atmosphere_intersection(rayStart, rayDir);
	rayLength = min(rayLength, intersection.y);
	if (intersection.x > 0)
	{
		// Advance ray to the atmosphere entry point
		rayStart += rayDir * intersection.x;
		rayLength -= intersection.x;
	}

	float  costh    = dot(rayDir, lightDir);
	float  phaseR   = PhaseRayleigh(costh);
	float  phaseM   = PhaseMie(costh, 0.85);

	int    sampleCount  = 64;

	vec3 opticalDepth = vec3(0);
	vec3 rayleigh     = vec3(0);
	vec3 mie          = vec3(0);

	float  prevRayTime  = 0;

	for (int i = 0; i < sampleCount; i++)
	{
		float  rayTime = pow(float(i) / sampleCount, sampleDistributionExponent) * rayLength;
		// Because we are distributing the samples exponentially, we have to calculate the step size per sample.
		float  stepSize = (rayTime - prevRayTime);

		vec3 localPosition = rayStart + rayDir * rayTime;
		float  localHeight   = AtmosphereHeight(localPosition);
		vec3 localDensity  = AtmosphereDensity(localHeight);

		opticalDepth += localDensity * stepSize;

		// The atmospheric transmittance from rayStart to localPosition
		vec3 viewTransmittance = Absorb(opticalDepth);

		vec3 opticalDepthlight  = IntegrateOpticalDepth(localPosition, lightDir);
		// The atmospheric transmittance of light reaching localPosition
		vec3 lightTransmittance = Absorb(opticalDepthlight);

		rayleigh += viewTransmittance * lightTransmittance * phaseR * localDensity.x * stepSize;
		mie      += viewTransmittance * lightTransmittance * phaseM * localDensity.y * stepSize;

		prevRayTime = rayTime;
	}

	transmittance = Absorb(opticalDepth);

	return (rayleigh * ATMOSPHERE_C_RAYLEIGH + mie * ATMOSPHERE_C_MIE) * lightColor * ATMOSPHERE_EXPOSURE;
}

#endif // ATMOSPHERE_INCLUDED