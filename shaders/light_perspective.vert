#version 330
// A vertex shader for rendering vertices with normal vectors and texture coordinates,
// which creates outputs needed for a Phong reflection fragment shader.
layout (location=0) in vec3 vPosition;
layout (location=1) in vec3 vNormal;
layout (location=2) in vec2 vTexCoord;
layout (location=3) in vec3 vTangent;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
//light space matrix
uniform mat4 lightSpaceMatrix;

out vec2 TexCoord;
out vec3 Normal;
out vec3 FragWorldPos;
out vec4 FragPosLightSpace;

// update vertex shader for normal mapping. 
// source: https://learnopengl.com/Advanced-Lighting/Normal-Mapping
out mat3 TBN;

void main() {
    // Transform the vertex position from local space to clip space.
    gl_Position = projection * view * model * vec4(vPosition, 1.0);
    // Pass along the vertex texture coordinate.
    TexCoord = vTexCoord;

    // DONE: transform the vertex position into world space, and assign it to FragWorldPos.
    FragWorldPos = vec3(model * vec4(vPosition, 1.0));

    // Transform the vertex normal from local space to world space, using the Normal matrix.
    mat4 normalMatrix = transpose(inverse(model));
    Normal = mat3(normalMatrix) * vNormal;

    //calculate the light space fragment position
    FragPosLightSpace = lightSpaceMatrix * vec4(FragWorldPos, 1.0);

    // Implement the TBN values for a normal map
    vec3 T = normalize(mat3(normalMatrix) * vTangent);    
    vec3 N = normalize(mat3(normalMatrix) * vNormal);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);

    TBN = transpose(mat3(T, B, N));
}