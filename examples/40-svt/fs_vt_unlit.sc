$input v_texcoord0

/*
 * Copyright 2011-2023 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx/blob/master/LICENSE
 */

/*
 * Reference(s):
 * - Based on Virtual Texture Demo by Brad Blanchard
 *   http://web.archive.org/web/20190103162638/http://linedef.com/virtual-texture-demo.html
 */ 
 
#include "../common/common.sh"
#include "virtualtexture.sh"

SAMPLER2D(s_tex2,  4);

void main()
{
   vec4 color = pow(VirtualTexture(v_texcoord0.xy), 1.0 / 1.5);
   vec4 height = texture2D(s_tex2, v_texcoord0.xy);
   color = mix(color, vec4(33.0 / 255.0, 64.0 / 255.0, 86.0 / 255.0, 1.0), smoothstep(0.001, -0.00001, height.x));
   gl_FragColor = color;
}

