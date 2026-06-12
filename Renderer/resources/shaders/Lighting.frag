#version 450
#extension GL_EXT_nonuniform_qualifier : require

struct GPULight
{
    vec4 posDir;
    vec4 color;
};

layout(location = 0) in vec2 fragUV;

layout(set = 0, binding = 0) uniform sampler2D gAlbedo;
layout(set = 0, binding = 1) uniform sampler2D gNormal;
layout(set = 0, binding = 2) uniform sampler2D gMaterial;
layout(set = 0, binding = 3) uniform sampler2D gDepth;

layout(set = 1, binding = 0) uniform SceneData
{
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec4 cameraPos;
    uint lightCount;
} sceneData;

layout(set = 2, binding = 0) readonly buffer LightBuffer
{
    GPULight lights[];
} lightBuffer;

layout(set = 3, binding = 0) uniform samplerCube skyBox;

layout(location = 0) out vec4 outColor;

//==================
// Const Values
//==================

const float PI = 3.14159265359;
const float AO = 1.0; // constant ambient occlusion

//==================
// Normal Decoding
//==================

vec2 OctWrap(vec2 v)
{
    return (1.0 - abs(v.yx)) * sign(v);
}

vec3 DecodeNormal(vec2 e)
{
    e = e * 2.0 - 1.0;

    vec3 n;
    n.z = 1.0 - abs(e.x) - abs(e.y);
    n.xy = n.z >= 0.0 ? e.xy : OctWrap(e.xy);

    return normalize(n);
}

// ==================
// World Position Reconstruction
// ==================

vec3 ReconstructWorldPos(in float depth)
{
    
    //vec4 clipPos = vec4(fragUV * 2.0f - 1.0f, depth * 2.0 - 1.0, 1.0f);
    vec4 clipPos = vec4(fragUV * 2.0f - 1.0f, depth, 1.0f);
    //inverse projection to viewspace
    vec4 viewPos = inverse(sceneData.proj) * clipPos;
    viewPos /= viewPos.w;

    //inverse view to worldspace
    vec4 worldPos = inverse(sceneData.view) * viewPos;

    return worldPos.xyz;
}

vec3 TransformDirectionToCubeMap(in vec3 dir)
{
    vec3 transformedDir = vec3(dir.z, dir.y, -dir.x);
    return transformedDir;
}

//==================
// Lighting functions (declarations)
//==================

vec3 KelvinToRgb(in float kelvin);

vec3 fresnelSchlick(float cosTheta, vec3 F0);
float DistributionGGX(vec3 N, vec3 H, float roughness);
float GeometrySchlickGGX(float NdotV, float roughness);
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness);

//==================
// Main
//==================

void main()
{
    //albedo
    //vec4 albedo = texture(gAlbedo, fragUV);
    //outColor = vec4(albedo.rgb, 1.0);

    //normal
    //vec4 normal = texture(gNormal, fragUV);
    //vec3 worldNormal = DecodeNormal(normal.xy);
    //outColor = vec4(worldNormal, 1.f);

    //metalness
    //vec4 albedo = texture(gMaterial, fragUV);
    //outColor = vec4(albedo.b, albedo.b, albedo.b, 1.0);

    //roughness
    //vec4 albedo = texture(gMaterial, fragUV);
    //outColor = vec4(albedo.g, albedo.g, albedo.g, 1.0);

    //depth
    //float depth = texture(gDepth, fragUV).r;
    //outColor = vec4(vec3(depth), 1.0);

    //normal
    //vec4 normal = texture(gNormal, fragUV);
    //vec3 worldNormal = DecodeNormal(normal.xy);
    //outColor = vec4(worldNormal.r, worldNormal.r, worldNormal.r, 1.f);

    ///*

    vec4 albedoSample = texture(gAlbedo, fragUV);
    vec4 normalSample = texture(gNormal, fragUV);
    vec4 materialSample = texture(gMaterial, fragUV);
    float depth = texelFetch(gDepth, ivec2(gl_FragCoord.xy), 0).r;

    vec3 albedo = albedoSample.rgb;
    float roughness = materialSample.g;  // adjust channels to match your GBuffer layout
    float metallic = materialSample.b;

    vec3 N = DecodeNormal(normalSample.rg);
    vec3 worldPos = ReconstructWorldPos(depth);
    vec3 V = normalize(sceneData.cameraPos.xyz - worldPos);
    
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 Lo = vec3(0.0);

    if(depth >= 1.0)
    {
        outColor = texture(skyBox, TransformDirectionToCubeMap(-V));
        return;
    }


    for(int i = 0; i < sceneData.lightCount; ++i) //todo: loop over actual lights
    {
        float illuminance;
        vec3 L;

        if (lightBuffer.lights[i].posDir.w < 0.0) // directional light
        {
            illuminance = lightBuffer.lights[i].color.a;
            L = normalize(-lightBuffer.lights[i].posDir.xyz);
        } else //point light
        {
            const float lumen = lightBuffer.lights[i].color.a;
            const float luminousIntensity = lumen / (4.0 * PI);

            L = lightBuffer.lights[i].posDir.xyz - worldPos;
            const float dist = length(L);
            L = normalize(L);
            
            const float attenuation = 1.0 / max(dist * dist, 0.000001);
            illuminance = luminousIntensity * attenuation;
        }

        const vec3 irradiance = lightBuffer.lights[i].color.rgb * illuminance;

        vec3 H = normalize(V + L);

        vec3 radiance = irradiance;// / PI; //todo determine correct approach

        float NDF = DistributionGGX(N, H, roughness);        
        float G = GeometrySmith(N, V, L, roughness);      
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;

        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;

        float NdotL = max(dot(N, L), 0.0);                
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    vec3 ambient = vec3(0.01) * albedo * AO;
    vec3 color = Lo;// ambient + Lo;

    //color = color / (color + vec3(1.0));
    //color = pow(color, vec3(1.0 / 2.2)); gamma correction

    outColor = vec4(color, 1.0);
    //*/
}

//based on the slides, move to cpu
vec3 KelvinToRgb(in float kelvin)
{
    const float temp = kelvin / 100.0f;

    // Red
    const float r = (temp <= 66.0f) 
        ? 1.0f 
        : clamp(1.292935618906f * pow(temp - 60.0f, -0.1332047592f), 0.0f, 1.0f);

    // Green
    const float g = (temp <= 66.0f)
        ? clamp(0.390081578769f * log(temp) - 0.63141844378f, 0.0f, 1.0f)
        : clamp(1.12989086089f * pow(temp - 60.0f, -0.0755148492f), 0.0f, 1.0f);

    // Blue
    const float b = (temp >= 66.0f)
        ? 1.0f
        : (temp <= 19.0f) 
            ? 0.0f
            : clamp(0.54320678911f * log(temp - 10.0f) - 1.19625408914f, 0.0f, 1.0f);

    return vec3(r, g, b);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}  

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness; // Unity convention
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}