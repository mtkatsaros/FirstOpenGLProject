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
    sampler2D baseTexture;
    sampler2D specularMap;
    sampler2D normalMap;
    float shininess;
};

// struct for directional lighting
struct DirLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

// struct for point lights
struct PointLight {
    vec3 position;
    float constant;
    float linear;
    float quadratic;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

// struct for spotlights
struct SpotLight {
    vec3 position;
    vec3 direction;
    float cutOff;
    float outerCutOff;
    float constant;
    float linear;
    float quadratic;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;       
};


// Inputs: the texture coordinates, world-space normal, and world-space position
// of this fragment, interpolated between its vertices.
in vec2 TexCoord;
in vec3 Normal;
in vec3 FragWorldPos;
in mat3 TBN;

// Uniforms: MUST BE PROVIDED BY THE APPLICATION.

// The mesh's base (diffuse) texture.
uniform sampler2D baseTexture;

// Material parameters for the whole mesh: k_a, k_d, k_s, shininess.
// uniform vec4 material; //manual version
uniform Material material;

// directional light to be modified in application
uniform DirLight dirLight;

// point lights for application. define a constant for array size
#define MAX_POINT_LIGHTS 20
uniform int numPointLights; //arrays must be defined at compile time, so loop through with this
uniform PointLight pointLights[MAX_POINT_LIGHTS];

#define MAX_SPOTLIGHTS 10
uniform int numSpotLights;
uniform SpotLight spotLights[MAX_SPOTLIGHTS];

// Ambient light color.
//uniform vec3 ambientColor;

// Direction and color of a single directional light.
//uniform vec3 directionalLight; // this is the "I" vector, not the "L" vector.
//uniform vec3 directionalColor;



// Location of the camera.
uniform vec3 viewPos;

//Calculates directional lighting with specular map
vec3 CalcDirLight(DirLight light, vec3 norm, vec3 eyeDir){
    // Ambient
    vec3 ambientIntensity = vec3(texture(material.baseTexture, TexCoord)) * light.ambient;

    // Diffuse components
    vec3 diffuseIntensity = vec3(0);
    
    vec3 lightDir = -light.direction;
    float lambertFactor = dot(norm, normalize(lightDir));

    // Specular components
    vec3 specularIntensity = vec3(0);

    // Lambert calculations can be combined
    if (lambertFactor > 0) {
        // Diffuse Lambert logic
        diffuseIntensity = vec3(texture(material.baseTexture, TexCoord)) * light.diffuse * lambertFactor;
        vec3 reflectDir = normalize(reflect(-lightDir, norm));
        // Specular Lambert logic
        float spec = dot(reflectDir, eyeDir);
        if (spec > 0)
            specularIntensity = texture(material.specularMap, TexCoord).x  * light.specular * pow(spec, material.shininess);
    }
   

    return ambientIntensity + diffuseIntensity + specularIntensity;
}


// Calculates point light with specular map
vec3 CalcPointLight(PointLight light, vec3 norm, vec3 eyeDir){
    //we now must consider position of the light for point lights
    vec3 lightDir = normalize(light.position - FragWorldPos);

    // We also must calculate attenuation: the dropoff of intensity of light over a given distance
    float dist = length(light.position - FragWorldPos);
    float attenuation = 1.0 / (light.constant + light.linear * dist + light.quadratic * (dist * dist));

    //now calculate the final ambient, diffuse, and specular lighting with attenuation
    vec3 diffuseIntensity = vec3(0);
    vec3 specularIntensity = vec3(0);

    vec3 ambientIntensity = vec3(texture(material.baseTexture, TexCoord)) * light.ambient * attenuation;
    float lambertFactor = dot(norm, normalize(lightDir));
    // Lambert calculations can be combined
    if (lambertFactor > 0) {
        // Diffuse Lambert logic
        diffuseIntensity = vec3(texture(material.baseTexture, TexCoord)) * light.diffuse * lambertFactor * attenuation;
        vec3 reflectDir = normalize(reflect(-lightDir, norm));
        // Specular Lambert logic
        float spec = dot(reflectDir, eyeDir);
        if (spec > 0)
            specularIntensity = vec3(texture(material.specularMap, TexCoord)) * light.specular * pow(spec, material.shininess) * attenuation;
    }
    
    return ambientIntensity + diffuseIntensity + specularIntensity;

}

// Calculates spotlight with specular map
vec3 CalcSpotLight(SpotLight light, vec3 norm, vec3 eyeDir){
    vec3 lightDir = normalize(light.position - FragWorldPos);

    // spot lights also use attenuation: the dropoff of intensity of light over a given distance
    float dist = length(light.position - FragWorldPos);
    float attenuation = 1.0 / (light.constant + light.linear * dist + light.quadratic * (dist * dist));

    // Spotlights need everything from the other two, as well as a second direction for the light and 
    // two angles: a cutoff angle and an anlge between the light direction and the spot direction 
    
    // Calculate the spotlight intensity with spotlight specific values
    // theta : the dot product of the light direction and the spot direction
    float theta = dot(lightDir, normalize(-light.direction));
    // epsilon : the inner cutoff vs the outer cutoff of the spotlight
    float epsilon = light.cutOff - light.outerCutOff;
    // intensity: take these values and "clamp" them, meaning the inner cutoff is 1.0 and the outer cutoff is 0.
    // essentially a "gradient" of intensities.
    float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);

    //now calculate the final ambient, diffuse, and specular lighting with attenuation
    vec3 diffuseIntensity = vec3(0);

    // Specular components
    vec3 specularIntensity = vec3(0);
   
    //now apply the intensity to the ambient, specular, and diffuse components
    vec3 ambientIntensity = vec3(texture(material.baseTexture, TexCoord)) * light.ambient * attenuation * intensity;
    float lambertFactor = dot(norm, normalize(lightDir));
    // Lambert calculations can be combined
    if (lambertFactor > 0) {
        // Diffuse Lambert logic
        diffuseIntensity = vec3(texture(material.baseTexture, TexCoord)) * light.diffuse * lambertFactor * attenuation * intensity;
        vec3 reflectDir = normalize(reflect(-lightDir, norm));
        // Specular Lambert logic
        float spec = dot(reflectDir, eyeDir);
        if (spec > 0)
            specularIntensity = vec3(texture(material.specularMap, TexCoord)) * light.specular * pow(spec, material.shininess) * attenuation * intensity;
    }

    return ambientIntensity + diffuseIntensity + specularIntensity;

}

// The main driver adds everything together
void main() {
    // DONE: using the lecture notes, compute ambientIntensity, diffuseIntensity, 
    // and specularIntensity.
    vec3 norm = normalize(Normal);

    //norm = vec3(texture(material.normalMap, TexCoord));
    //norm = normalize(TBN * (norm * 2.0 - 1.0));
    vec3 eyeDir = normalize(viewPos - FragWorldPos);
    
    // directional lighting
    vec3 result = CalcDirLight(dirLight, norm, eyeDir);

    // point lighting
    for(int i = 0; i < numPointLights; i++)
        result += CalcPointLight(pointLights[i], norm, eyeDir);
    
    // spotlights
    for(int i = 0; i < numSpotLights; i++)
        result += CalcSpotLight(spotLights[i], norm, eyeDir);
    
    
    FragColor = vec4(result, 1);
}