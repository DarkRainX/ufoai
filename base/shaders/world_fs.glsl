#version 110
// default fragment shader

#include "light_fs.glsl"
#include "bump_fs.glsl"
#include "fog_fs.glsl"

uniform int BUMPMAP;
uniform int STATICLIGHT;
uniform float GLOWSCALE;

uniform sampler2D SAMPLER0;
/* lightmap */
uniform sampler2D SAMPLER1;
/* deluxemap */
uniform sampler2D SAMPLER2;
/* normalmap */
uniform sampler2D SAMPLER3;
/* glowmap */
uniform sampler2D SAMPLER4;

const vec3 two = vec3(2.0);
const vec3 negHalf = vec3(-0.5);

varying vec3 lightpos;

/**
 * main
 */
void main(void){

	vec2 offset = vec2(0.0);
	vec3 bump = vec3(1.0);

	// sample the lightmap
	vec3 lightmap = texture2D(SAMPLER1, gl_TexCoord[1].st).rgb;

#if r_bumpmap
	if(BUMPMAP > 0){  // sample deluxemap and normalmap
		vec3 deluxemap = texture2D(SAMPLER2, gl_TexCoord[1].st).rgb;
		deluxemap = normalize(two * (deluxemap + negHalf));

		vec4 normalmap = texture2D(SAMPLER3, gl_TexCoord[0].st);
		normalmap.rgb = normalize(two * (normalmap.rgb + negHalf));

		// resolve parallax offset and bump mapping
		offset = BumpTexcoord(normalmap.a);
		bump = BumpFragment(deluxemap, normalmap.rgb);
	}
#endif

	// sample the diffuse texture, honoring the parallax offset
	vec4 diffuse = texture2D(SAMPLER0, gl_TexCoord[0].st + offset);

	// factor in bump mapping
	diffuse.rgb *= bump;

	// use static lighting if enabled
	if (STATICLIGHT > 0) {
		vec3 lightdir = normalize(lightpos - point);
		float shade = max(0.5, pow(2.0 * dot(normal, lightdir), 2.0));

		vec3 color = vec3(1.0);
		if (gl_Color.r > 0.0 || gl_Color.g > 0.0 || gl_Color.b > 0.0) {
			color = gl_Color.rgb;
		} 
		LightFragment(diffuse, color * shade);
	} else {
		// otherwise, add any dynamic lighting and yield a base fragment color
		LightFragment(diffuse, lightmap);
	}

#if r_fog
	FogFragment();  // add fog
#endif

// developer tools
#if r_lightmap
	gl_FragData[0].rgb = lightmap;
	gl_FragData[0].a = 1.0;
#endif

#if r_deluxemap
	if(BUMPMAP > 0){
		gl_FragData[0].rgb = deluxemap;
		gl_FragData[0].a = 1.0;
	}
#endif

#if r_postprocess
	if(GLOWSCALE > 0.01){
		 vec4 glowcolor = texture2D(SAMPLER4, gl_TexCoord[0].st);
		 gl_FragData[1].rgb = glowcolor.rgb * glowcolor.a * GLOWSCALE;
		 gl_FragData[1].a = 1.0;
	}
#endif
}
