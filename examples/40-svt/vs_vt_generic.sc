$input a_position, a_texcoord0
$output v_texcoord0

/*
 * Copyright 2011-2022 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx/blob/master/LICENSE
 */

#include "../common/common.sh"

SAMPLER2D(s_tex2,  4);

void main()
{
   vec3 pos = a_position;
   vec2 cc = cos(a_texcoord0 * 10.0) * 10.0;
   //pos.y += cc.x + cc.y;//cos( texture2DLod(s_tex2, a_texcoord0, 0.0);
   pos.y += texture2DLod(s_tex2, a_texcoord0, 0.0) * 10.0;
   vec3 wpos = mul(u_model[0], vec4(pos, 1.0) ).xyz;
   gl_Position = mul(u_viewProj, vec4(wpos, 1.0) );
   v_texcoord0 = a_texcoord0;
}
