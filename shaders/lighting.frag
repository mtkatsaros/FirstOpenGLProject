#version 330
// A fragment shader for rendering fragments in the Phong reflection model.
layout (location=0) out vec4 FragColor;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// Most my shader logic was learned here: 
// https://learnopengl.com/Lighting/Multiple-lights
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// We can make structs like in c/c++
// learnopengl claims diffuse and ambient have the same k value
// Material struct
struct Material {
    sampler2D diffuse;
    sampler2D specular;
    float shininess;
};

//struct for directional lighting
struct DirLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

// Inputs: the texture coordinates, world-space normal, and world-space position
// of this fragment, interpolated between its vertices.
in vec2 TexCoord;
in vec3 Normal;
in vec3 FragWorldPos;

// Uniforms: MUST BE PROVIDED BY THE APPLICATION.

// The mesh's base (diffuse) texture.
uniform sampler2D baseTexture;

// Material parameters for the whole mesh: k_a, k_d, k_s, shininess.
// uniform vec4 material; //manual version
uniform Material material;
uniform DirLight dirLight;

// Ambient light color.
//uniform vec3 ambientColor;

// Direction and color of a single directional light.
uniform vec3 directionalLight; // this is the "I" vector, not the "L" vector.
//uniform vec3 directionalColor;



// Location of the camera.
uniform vec3 viewPos;

//Calculates directional lighting
vec3 CalcDirLight(DirLight light, vec3 norm, vec3 eyeDir){
    // Ambient
    vec3 ambientIntensity = light.ambient * vec3(texture(material.diffuse, TexCoord));

    // Diffuse components
    vec3 diffuseIntensity = vec3(0);
    
    vec3 lightDir = -light.direction;
    float lambertFactor = dot(norm, normalize(lightDir));

    // Specular components
    vec3 specularIntensity = vec3(0);

    // Lambert calculations can be combined
    if (lambertFactor > 0) {
        // Diffuse Lambert logic
        diffuseIntensity = light.diffuse * lambertFactor * texture(material.diffuse, TexCoord).x;
        vec3 reflectDir = normalize(reflect(-lightDir, norm));
        // Specular Lambert logic
        float spec = dot(reflectDir, eyeDir);
        if (spec > 0){
            specularIntensity = texture(material.specular, TexCoord).x  * light.specular * pow(spec, material.shininess);
        }
    }

    return ambientIntensity + diffuseIntensity + specularIntensity;
}


// The main driver adds everything together
void main() {
    // DONE: using the lecture notes, compute ambientIntensity, diffuseIntensity, 
    // and specularIntensity.
    vec3 norm = normalize(Normal);
    vec3 eyeDir = normalize(viewPos - FragWorldPos);
    
    vec3 result = CalcDirLight(dirLight, norm, eyeDir);

    
    FragColor = vec4(result, 1);
}