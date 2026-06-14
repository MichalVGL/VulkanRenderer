#version 450
#extension GL_EXT_nonuniform_qualifier : require

struct GPULight
{
    vec4  posDir;
    vec4  color;
    mat4  lightView;
    mat4  lightProj;
    uint  depthIndex;
    uint  pad[3];
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

layout(set = 3, binding = 0) uniform samplerCube IBLDiffuse;

layout(set = 4, binding = 0) uniform sampler2DShadow DirectionalShadowMaps[];

layout(location = 0) out vec4 outColor;

//==================
// Const Values
//==================

const float PI = 3.14159265359;

//constants that could also be implemented later on as parameters from the cpu
const float AO = 1.0; // constant ambient occlusion
const float EXPOSURECOMPENSATION = 0.5; // for IBL diffuse

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

vec3 GetShadowUV(in GPULight light, in vec3 worldPos)
{
    vec4 lightSpacePosition = light.lightProj * light.lightView * vec4(worldPos, 1.0);
    lightSpacePosition.xyz /= lightSpacePosition.w;
    vec2 shadowMapUV = lightSpacePosition.xy * 0.5 + 0.5; 
    shadowMapUV.y = 1.0 - shadowMapUV.y;
    
    return vec3(shadowMapUV, lightSpacePosition.z);
}

// ==================
// Sample logic
// ==================

vec3 SampleIBL_Diffuse(in vec3 nomralWS, in float environmentIntensity)
{
    const vec3 prefilteredDiffuseIrradiance = texture(IBLDiffuse
    , TransformDirectionToCubeMap(nomralWS)).rgb;

    const vec3 exposedPrefilteredDiffuseIrradianc = prefilteredDiffuseIrradiance * environmentIntensity;
    return exposedPrefilteredDiffuseIrradianc;
}

//==================
// Lighting functions (declarations)
//==================

vec3 KelvinToRgb(in float kelvin);

vec3 FresnelSchlick(float cosTheta, vec3 F0);
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness);
float DistributionGGX(vec3 N, vec3 H, float roughness);
//float GeometrySchlickGGX(float NdotV, float roughness); //not used directly in main
float GeometrySmith_Direct(vec3 N, vec3 V, vec3 L, float roughness);
float GeometrySmith_Indirect(vec3 N, vec3 V, vec3 L, float roughness);

//==================
// Main
//==================

void main()
{
    //================================
    // Raw GBuffer Tests

    //albedo
    //vec4 albedo = texture(gAlbedo, fragUV);
    //outColor = vec4(albedo.rgb, 1.0);

    //normal
    //vec4 normal = texture(gNormal, fragUV);
    //vec3 worldNormal = DecodeNormal(normal.xy);
    // outColor = vec4(worldNormal, 1.f);

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

    float depth = texelFetch(gDepth, ivec2(gl_FragCoord.xy), 0).r;
    vec3 worldPos = ReconstructWorldPos(depth);

    GPULight light = lightBuffer.lights[0];
    //vec3 shadowUV = GetShadowUV(light, worldPos);
    //float shadowDepth = texture(DirectionalShadowMaps[0], shadowUV).r;

    float shadowTerm = 1.0;

    vec3 shadowUV = GetShadowUV(light, worldPos);

    // bounds check — outside frustum = lit
    if (shadowUV.x >= 0.0 && shadowUV.x <= 1.0 &&
        shadowUV.y >= 0.0 && shadowUV.y <= 1.0 &&
        shadowUV.z >= 0.0 && shadowUV.z <= 1.0)
    {
        // sampler2DShadow + vec3 UV → hardware does depth compare + PCF
        shadowTerm = texture(DirectionalShadowMaps[nonuniformEXT(light.depthIndex)], shadowUV);
    }

    outColor = vec4(vec3(shadowTerm), 1.0);
    /*

    //================================
    // PBR Lighting

    vec4 albedoSample = texture(gAlbedo, fragUV);
    //vec4 albedoSample = texelFetch(gAlbedo, ivec2(gl_FragCoord.xy), 0);
    vec4 normalSample = texture(gNormal, fragUV);
    //vec4 normalSample = texelFetch(gNormal, ivec2(gl_FragCoord.xy), 0);
    vec4 materialSample = texture(gMaterial, fragUV);
    //vec4 materialSample = texelFetch(gMaterial, ivec2(gl_FragCoord.xy), 0);
    float depth = texelFetch(gDepth, ivec2(gl_FragCoord.xy), 0).r;

    vec3 albedo = albedoSample.rgb;
    float roughness = materialSample.g;  // adjust channels to match your GBuffer layout
    float metallic = materialSample.b;

    vec3 N = DecodeNormal(normalSample.rg);
    vec3 worldPos = ReconstructWorldPos(depth);
    vec3 V = normalize(sceneData.cameraPos.xyz - worldPos);
    
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 Lo = vec3(0.0);

    //Skybox test
    //if(depth >= 1.0)
    //{
        //skybox
        //outColor = texture(IBLDiffuse, TransformDirectionToCubeMap(-V));
        //return;
    //}
    

    for(int i = 0; i < sceneData.lightCount; ++i)
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

        float NDF = DistributionGGX(N, H, roughness);        
        float G = GeometrySmith_Direct(N, V, L, roughness);      
        vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;

        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;

        float NdotL = max(dot(N, L), 0.0);                
        Lo += (kD * albedo / PI + specular) * irradiance * NdotL;
    }

    //IBL
    vec3 ambient = vec3(0.0);
    {
        vec3 F = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
        vec3 kS = F;
        vec3 kD = 1.0 - kS;
        kD *= 1.0 - metallic;

        const vec3 prefilteredDiffuseIrradiance = SampleIBL_Diffuse(N, 1.0); //todo, add environment intensity functionality
        const vec3 diffuse = prefilteredDiffuseIrradiance * albedo;

        //ambient = prefilteredDiffuseIrradiance * kD; //test the diffuse part of IBL only
        ambient = kD * diffuse * EXPOSURECOMPENSATION;
    }
    
    vec3 color = ambient + Lo;

    outColor = vec4(color, 1.0);
    //*/
}

//based on the slides, moved to cpu due to unnecessary calculations
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

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}  

vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
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

float GeometrySchlickGGX_Direct(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}

float GeometrySchlickGGX_Indirect(float NdotV, float roughness)
{
    float a = roughness;
    float k = (a * a) / 2.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float GeometrySmith_Direct(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX_Direct(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX_Direct(NdotL, roughness);
	
    return ggx1 * ggx2;
}

float GeometrySmith_Indirect(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX_Indirect(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX_Indirect(NdotL, roughness);
	
    return ggx1 * ggx2;
}