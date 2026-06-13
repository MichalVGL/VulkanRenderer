#version 450

layout(location = 0) in vec3 fragLocalPosition;

layout(set = 0, binding = 0) uniform samplerCube cubeMap;

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

void TangentToWorld(in vec3 normal, out vec3 tangent, out vec3 bitangent)
{
    vec3 up = normalize(abs(normal.y)) < 0.999f
        ? vec3(0.f, -1.f, 0.f) : vec3(1.f, 0.f, 0.f);
    tangent = normalize(cross(normal, up));
    bitangent = cross(tangent, normal);
}

vec3 TangentToWorld(in vec3 normal, in vec3 tangentSample)
{
    vec3 up = abs(normal.y) < 0.999f
    ? vec3(0.f, -1.f, 0.f) : vec3(1.f, 0.f, 0.f);
    const vec3 tangent = normalize(cross(normal, up));
    const vec3 bitangent = cross(tangent, normal);

    return vec3(
        tangentSample.x * tangent +
        tangentSample.y * bitangent +
        tangentSample.z * normal);
}

void main()
{
    vec3 normal = normalize(fragLocalPosition);
    normal.y *= -1.0; 

    vec3 irradiance = vec3(0.f);
    float nrSamples = 0.f;
    const float sampleDelta = 0.025f;
    for(float phi = 0.f; phi < 2.f * PI; phi += sampleDelta)
    {
        for(float theta = 0.f; theta < 0.5f * PI; theta += sampleDelta)
        {
            // From spherical coordinates to cartesian coordinates
            const vec3 sampleTS = vec3(
                sin(theta) * cos(phi),
                sin(theta) * sin(phi),
                cos(theta));

            // From tangent-space vector to world-space sample vector
            const vec3 sampleWS = TangentToWorld(normal, sampleTS);

            vec3 sampleColor = texture(cubeMap, sampleWS).rgb;

            // Sample and accumulate
            irradiance += sampleColor * cos(theta) * sin(theta);
            ++nrSamples;
        }
    }
    // Integrate and store result
    irradiance = PI * irradiance * (1.f / float(nrSamples));
    outColor = vec4(irradiance, 1.f);
}