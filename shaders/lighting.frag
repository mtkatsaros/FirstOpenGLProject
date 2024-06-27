#version 330
// A fragment shader for rendering fragments in the Phong reflection model.
layout (location=0) out vec4 FragColor;

// Inputs: the texture coordinates, world-space normal, and world-space position
// of this fragment, interpolated between its vertices.
in vec2 TexCoord;
in vec3 Normal;
in vec3 FragWorldPos;

// Uniforms: MUST BE PROVIDED BY THE APPLICATION.

// The mesh's base (diffuse) texture.
uniform sampler2D baseTexture;

// Material parameters for the whole mesh: k_a, k_d, k_s, shininess.
uniform vec4 material;

// Ambient light color.
uniform vec3 ambientColor;

// Direction and color of a single directional light.
uniform vec3 directionalLight; // this is the "I" vector, not the "L" vector.
uniform vec3 directionalColor;



// Location of the camera.
uniform vec3 viewPos;


void main() {
    // DONE: using the lecture notes, compute ambientIntensity, diffuseIntensity, 
    // and specularIntensity.

    // Ambient
    vec3 ambientIntensity = material.x * ambientColor;

    // Diffuse components
    vec3 diffuseIntensity = vec3(0);
    vec3 norm = normalize(Normal);
    vec3 lightDir = -directionalLight;
    float lambertFactor = dot(norm, normalize(lightDir));

    // Specular components
    vec3 specularIntensity = vec3(0);

    // Lambert calculations can be combined
    if (lambertFactor > 0) {
        // Diffuse Lambert logic
        diffuseIntensity = material.y * directionalColor * lambertFactor;

        // Specular Lambert logic
        vec3 eyeDir = normalize(viewPos - FragWorldPos);
        vec3 reflectDir = normalize(reflect(-lightDir, norm));
        float spec = dot(reflectDir, eyeDir);
        if (spec > 0){
            specularIntensity = material.z * directionalColor * pow(spec, material.w);
        }
    }

    vec3 lightIntensity = ambientIntensity + diffuseIntensity + specularIntensity;
    FragColor = vec4(lightIntensity, 1) * texture(baseTexture, TexCoord);
}