#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec2 fragUV;

layout(set = 0, binding = 0) uniform sampler2D gAlbedo;
layout(set = 0, binding = 1) uniform sampler2D gNormal;
layout(set = 0, binding = 2) uniform sampler2D gMaterial;

layout(set = 1, binding = 0) uniform SceneData
{
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec4 cameraPos;
} sceneData;

layout(location = 0) out vec4 outColor;

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

//==================
// Const Values
//==================

const float PI = 3.14159265359;

//==================
// Lighting functions (declarations)
//==================

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
    vec4 albedo = texture(gAlbedo, fragUV);
    outColor = vec4(albedo.rgb, 1.0);

    //normal
    //vec4 normal = texture(gNormal, fragUV);
    //outColor = vec4(normal.rg, 0.0, 1.0);

    //metalness
    //vec4 albedo = texture(gMaterial, fragUV);
    //outColor = vec4(albedo.b, albedo.b, albedo.b, 1.0);

    //roughness
    //vec4 albedo = texture(gMaterial, fragUV);
    //outColor = vec4(albedo.g, albedo.g, albedo.g, 1.0);

/*
    vec3 N = normalize(Normal);
    vec3 V = normalize(camPos - WorldPos);

    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);
	           
    // reflectance equation
    vec3 Lo = vec3(0.0);
    for(int i = 0; i < 4; ++i) 
    {
        // calculate per-light radiance
        vec3 L = normalize(lightPositions[i] - WorldPos);
        vec3 H = normalize(V + L);
        float distance    = length(lightPositions[i] - WorldPos);
        float attenuation = 1.0 / (distance * distance);
        vec3 radiance     = lightColors[i] * attenuation;        
        
        // cook-torrance brdf
        float NDF = DistributionGGX(N, H, roughness);        
        float G   = GeometrySmith(N, V, L, roughness);      
        vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);       
        
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;	  
        
        vec3 numerator    = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular     = numerator / denominator;  
            
        // add to outgoing radiance Lo
        float NdotL = max(dot(N, L), 0.0);                
        Lo += (kD * albedo / PI + specular) * radiance * NdotL; 
    }   
  
    vec3 ambient = vec3(0.03) * albedo * ao;
    vec3 color = ambient + Lo;
	
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));  
   
    FragColor = vec4(color, 1.0);*/
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}  

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
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