/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quetoo.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#version 330

uniform float brightness;
uniform float contrast;
uniform float saturation;
uniform float gamma;

/**
 * @brief Clamps to [0.0, 1.0], like in HLSL.
 */
float saturate(float x) {
	return clamp(x, 0.0, 1.0);
}

/**
 * @brief Clamps vec3 components to [0.0, 1.0], like in HLSL.
 */
vec3 saturate3(vec3 v) {
	v.x = saturate(v.x);
	v.y = saturate(v.y);
	v.z = saturate(v.z);
	return v;
}

/**
 * @brief Brightness, contrast, saturation and gamma.
 */
vec3 color_filter(vec3 color) {

	vec3 luminance = vec3(0.2125, 0.7154, 0.0721);
	vec3 bias = vec3(0.5);

	vec3 scaled = mix(vec3(1.0), color, gamma) * brightness;

	color = mix(bias, mix(vec3(dot(luminance, scaled)), scaled, saturation), contrast);

	return color;
}

uniform vec3 fog_parameters;
uniform vec3 fog_color;

/**
 * @brief Global fog.
 */
vec3 fog(in vec3 position, in vec3 color) {

	float near = fog_parameters.x;
	float far = fog_parameters.y;
	float density = fog_parameters.z;

	float strength = density * ((length(position) - near) / (far - near));
	return mix(color.rgb, fog_color, saturate(strength));
}

/**
 * @brief Phong BRDF for specular highlights.
 */
vec3 brdf_phong(vec3 view_dir, vec3 light_dir, vec3 normal,
	vec3 light_color, float specular_intensity, float specular_exponent) {

	vec3 reflection = reflect(light_dir, normal);
	float r_dot_v = max(-dot(view_dir, reflection), 0.0);
	return light_color * specular_intensity * pow(r_dot_v, 16.0 * specular_exponent);
}

/**
 * @brief Blinn-Phong BRDF for specular highlights.
 */
vec3 brdf_blinn(vec3 view_dir, vec3 light_dir, vec3 normal,
	vec3 light_color, float specular_intensity, float specular_exponent) {

	const float exp_scale = 64.0; // roughly matches phong this way
	vec3 half_angle = normalize(light_dir + view_dir);
	float n_dot_h = max(dot(normal, half_angle), 0.0);
	return light_color * specular_intensity * pow(n_dot_h, specular_exponent * exp_scale);
}

/**
 * @brief Lambert BRDF for diffuse lighting.
 */
vec3 brdf_lambert(vec3 light_dir, vec3 normal, vec3 light_color) {
	return light_color * max(dot(light_dir, normal), 0.0);
}

/**
 * @brief Half-Lambert BRDF for diffuse lighting.
 */
vec3 brdf_halflambert(vec3 light_dir, vec3 normal, vec3 light_color) {
	return light_color * (1.0 - (dot(normal, light_dir) * 0.5 + 0.5));
}

/**
 * @brief Prevents surfaces from becoming overexposed by lights (looks bad).
 */
vec3 tonemap(vec3 color) {
	color *= exp(color);
	color /= color + 0.825;
	return color;
}

/**
 * Converts uniform distribution into triangle-shaped distribution. Used for dithering.
 */
float remap_triangular(float v) {
	
    float original = v * 2.0 - 1.0;
    v = original / sqrt(abs(original));
    v = max(-1.0, v);
    v = v - sign(original) + 0.5;

    return v;

    // result is range [-0.5,1.5] which is useful for actual dithering.
    // convert to [0,1] for output
    // return (v + 0.5f) * 0.5f;
}

/**
 * Converts uniform distribution into triangle-shaped distribution for vec3. Used for dithering.
 */
vec3 remap_triangular_3(vec3 c) {
    return vec3(remap_triangular(c.r), remap_triangular(c.g), remap_triangular(c.b));
}

/**
 * Applies dithering before quantizing to 8-bit values to remove color banding.
 */
vec3 dither(vec3 color) {

	// The function is adapted from slide 49 of Alex Vlachos's
	// GDC 2015 talk: "Advanced VR Rendering".
	// http://alex.vlachos.com/graphics/Alex_Vlachos_Advanced_VR_Rendering_GDC2015.pdf
	// original shadertoy implementation by Zavie:
	// https://www.shadertoy.com/view/4dcSRX
	// modification with triangular distribution by Hornet (loopit.dk):
	// https://www.shadertoy.com/view/Md3XRf

	vec3 pattern;
	// generate dithering pattern
	pattern = vec3(dot(vec2(131.0, 312.0), gl_FragCoord.xy));
    pattern = fract(pattern / vec3(103.0, 71.0, 97.0));
	// remap distribution for smoother results
	pattern = remap_triangular_3(pattern);
	// scale the magnitude to be the distance between two 8-bit colors
	pattern /= 255.0;
	// apply the pattern, causing some fractional color values to be
	// rounded up and others down, thus removing banding artifacts.
	return saturate3(color + pattern);
}

vec3 pow3(vec3 v, float exponent) {
	v.x = pow(v.x, exponent);
	v.y = pow(v.y, exponent);
	v.z = pow(v.z, exponent);
	return v;
}