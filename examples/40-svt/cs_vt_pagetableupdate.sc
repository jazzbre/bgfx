#include "bgfx_compute.sh" 

uniform vec4 u_value;
uniform vec4 u_offset;

IMAGE2D_WR(s_output, rgba8, 0);

NUM_THREADS(1, 1, 1)
void main() 
{
   uvec2 id = uvec2(gl_GlobalInvocationID.xy) + uvec2(u_offset.xy); 
   imageStore(s_output, id.xy, u_value);
}
