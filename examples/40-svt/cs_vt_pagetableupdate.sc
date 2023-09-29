#include "bgfx_compute.sh" 

uniform vec4 u_value;
uniform uvec4 u_offset;

IMAGE2D_WR(s_output, rgba8, 0);

NUM_THREADS(1, 1, 1)
void main() 
{
   uvec2 id = uvec2(gl_GlobalInvocationID.xy); 
   imageStore(s_output, id.xy + u_offset.xy, u_value);
}
