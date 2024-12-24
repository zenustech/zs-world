#include "virtual_file_helper.h"
#include "shader_runtime.h"

#include <iostream>
#include <filesystem>
#include <fstream>

using namespace std;

void init();

const char* SHADER1_CONTENT = R"(
// Combined Slang shader with two entry points from the given GLSL shaders

// Input/Output structs for vertex shader
struct VSInput {
    float3 inPos     : POSITION;
    float3 inNormal  : NORMAL;
    float3 inColor   : COLOR;
    float2 inUV      : TEXCOORD;
};

struct VSOutput {
    float4 position   : SV_POSITION;
    float3 outNormal  : NORMAL;
    float3 outColor   : COLOR;
    float2 outUV      : TEXCOORD;
    float3 outViewVec : VIEWDIR;
    float3 outLightVec: LIGHTDIR;
};

// Uniform Buffers
cbuffer SceneCamera : register(b0, space0) {
    float4x4 projection;
    float4x4 view;
}

cbuffer ModelParams : register(b1, space0) {
    float4x4 model;
}

// Vertex Shader Entry Point
VSOutput mainVS(VSInput input) {
    VSOutput output;

    // Compute world-space position and transform to clip space
    float4 worldPosition = mul(model, float4(input.inPos, 1.0));
    float4 viewPosition = mul(view, worldPosition);
    output.position = mul(projection, viewPosition);

    // Pass through other inputs to outputs
    output.outColor = input.inColor;
    output.outUV = input.inUV;

    // Compute transformed normal in view space
    output.outNormal = mul((float3x3)view, input.inNormal);

    // Compute light vector and view vector in view space
    float3 lightPos = float3(1.0, 1.0, 1.0);
    output.outLightVec = lightPos - viewPosition.xyz;
    output.outViewVec = -viewPosition.xyz;

    return output;
}

// Input/Output structs for fragment shader
struct FSInput {
    float3 inNormal   : NORMAL;
    float3 inColor    : COLOR;
    float2 inUV       : TEXCOORD;
    float3 inViewVec  : VIEWDIR;
    float3 inLightVec : LIGHTDIR;
};

struct FSOutput {
    float4 outFragColor : SV_TARGET0;
    int3 outTag         : SV_TARGET1;
};

// Push Constants
cbuffer FragmentPushConstants : register(b2, space0) {
    int objId;
    int useTexture;
}

// Texture Sampler
Texture2D TEX_0 : register(t0, space1);
SamplerState samp : register(s0, space1);

// Fragment Shader Entry Point
FSOutput mainPS(FSInput input) {
    FSOutput output;

    // Normalizing input vectors
    float3 N = normalize(input.inNormal);
    float3 L = normalize(input.inLightVec);
    float3 V = normalize(input.inViewVec);
    float3 R = reflect(-L, N);

    // Ambient, diffuse, and specular lighting components
    float3 ambient = float3(0.1, 0.1, 0.1);
    float3 diffuse = max(dot(N, L), 0.0) * float3(1.0, 1.0, 1.0);
    float3 specular = pow(max(dot(R, V), 0.0), 16.0) * float3(0.75, 0.75, 0.75);

    // Determine base color
    float3 color = input.inColor;
    if (useTexture > 0) {
        color = TEX_0.Sample(samp, input.inUV).rgb;
    }

    // Final fragment color
    output.outFragColor = float4((ambient + diffuse) * color + specular, 1.0);

    // Set outTag information
    output.outTag = int3(objId, -1, 0); // -1 for placeholder, 0 for gl_PrimitiveID

    return output;
}

// Entry points
[shader("vertex")] VSOutput mainVS(VSInput input);
[shader("pixel")] FSOutput mainPS(FSInput input);
)";

int main() {

  init();
  zs::IShaderLibrary* shader_library = zs::IShaderManager::get().load_shader({ "/cwd/test" });
  auto spirv_code =  shader_library->get_entrypoint_by_name("mainVS")->get_spirv_code(nullptr);
  std::cout << spirv_code.size() << std::endl;
  std::ofstream os("test.spirv", std::ios::out | std::ios::binary | std::ios::trunc);
  os.write(reinterpret_cast<const char*>(spirv_code.data()), static_cast<ptrdiff_t>(spirv_code.size()));

  return 0;
}

void init() {
  const auto working_dir = absolute(std::filesystem::current_path()).generic_string();
  std::cout << "Current working dir: " << working_dir << std::endl;

  zs::IVirtualFileMapping* mapper = zs::IShaderManager::get().get_path_mapper();
  mapper->add_mapping("/cwd", working_dir);

  std::ofstream shader1(std::filesystem::current_path() / "test.slang", std::ios::binary | std::ios::trunc);
  shader1.write(SHADER1_CONTENT, std::strlen(SHADER1_CONTENT));
}
