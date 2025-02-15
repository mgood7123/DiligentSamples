
#include "SamplePlugin.h"
#include "GraphicsUtilities.h"
#include "MapHelper.hpp"

using namespace Diligent;

static const Char* VSSource = R"(
cbuffer Constants
{
    float4x4 g_WorldViewProj;
};

struct PSInput 
{ 
    float4 Pos : SV_POSITION; 
    float4 Color : COLOR0; 
};

void main(float3 pos   : ATTRIB0,
          float4 color : ATTRIB1,
          out PSInput PSIn) 
{
    PSIn.Pos = mul( float4(pos,1.0), g_WorldViewProj);
    PSIn.Color = color;
}
)";

static const Char* PSSource = R"(
struct PSInput 
{ 
    float4 Pos : SV_POSITION; 
    float4 Color : COLOR0; 
};

float4 main(PSInput PSIn) : SV_TARGET
{
    return PSIn.Color; 
}
)";

SamplePlugin::SamplePlugin(Diligent::IRenderDevice *pDevice, bool UseReverseZ, TEXTURE_FORMAT RTVFormat, TEXTURE_FORMAT DSVFormat)
{
    auto deviceType = pDevice->GetDeviceInfo().Type;
    {
        GraphicsPipelineStateCreateInfo PSOCreateInfo;
        PipelineStateDesc&    PSODesc          = PSOCreateInfo.PSODesc;
        GraphicsPipelineDesc& GraphicsPipeline = PSOCreateInfo.GraphicsPipeline;

        PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
        PSODesc.Name         = "Render sample cube PSO";

        GraphicsPipeline.NumRenderTargets = 1;
        GraphicsPipeline.RTVFormats[0] = RTVFormat;
        GraphicsPipeline.DSVFormat = DSVFormat;
        GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;
        GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = deviceType == RENDER_DEVICE_TYPE_D3D11 || deviceType == RENDER_DEVICE_TYPE_D3D12 ? true : false;
        GraphicsPipeline.DepthStencilDesc.DepthFunc = UseReverseZ ? COMPARISON_FUNC_GREATER_EQUAL : COMPARISON_FUNC_LESS_EQUAL;

        GraphicsPipeline.BlendDesc.RenderTargets[0].BlendEnable = True;
        GraphicsPipeline.BlendDesc.RenderTargets[0].SrcBlend = BLEND_FACTOR_SRC_ALPHA;
        GraphicsPipeline.BlendDesc.RenderTargets[0].DestBlend = BLEND_FACTOR_INV_SRC_ALPHA;
        GraphicsPipeline.BlendDesc.RenderTargets[0].SrcBlendAlpha = BLEND_FACTOR_ZERO;
        GraphicsPipeline.BlendDesc.RenderTargets[0].DestBlendAlpha = BLEND_FACTOR_ONE;

        ShaderCreateInfo ShaderCI;
        ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        ShaderCI.UseCombinedTextureSamplers = true;

        CreateUniformBuffer(pDevice, sizeof(float4x4), "SamplePlugin: VS constants CB", &m_VSConstants);

        RefCntAutoPtr<IShader> pVS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
            ShaderCI.EntryPoint = "main";
            ShaderCI.Desc.Name = "Sample cube VS";
            ShaderCI.Source = VSSource;
            pDevice->CreateShader(ShaderCI, &pVS);
        }

        RefCntAutoPtr<IShader> pPS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
            ShaderCI.EntryPoint = "main";
            ShaderCI.Desc.Name = "Sample cube PS";
            ShaderCI.Source = PSSource;
            pDevice->CreateShader(ShaderCI, &pPS);
        }

        LayoutElement LayoutElems[] =
        {
            LayoutElement{0, 0, 3, VT_FLOAT32, False},
            LayoutElement{1, 0, 4, VT_FLOAT32, False}
        };

        PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
        PSOCreateInfo.pVS = pVS;
        PSOCreateInfo.pPS = pPS;
        GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
        GraphicsPipeline.InputLayout.NumElements    = _countof(LayoutElems);
        pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_PSO);
        m_PSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);
        m_PSO->CreateShaderResourceBinding(&m_SRB, true);
    }

    {
        struct Vertex
        {
            float3 pos;
            float4 color;
        };
        Vertex CubeVerts[8] =
        {
            {float3(-1,-1,-1), float4(1,0,0,0.5)},
            {float3(-1,+1,-1), float4(0,1,0,0.5)},
            {float3(+1,+1,-1), float4(0,0,1,0.5)},
            {float3(+1,-1,-1), float4(1,1,1,0.5)},

            {float3(-1,-1,+1), float4(1,1,0,0.5)},
            {float3(-1,+1,+1), float4(0,1,1,0.5)},
            {float3(+1,+1,+1), float4(1,0,1,0.5)},
            {float3(+1,-1,+1), float4(0.2f,0.2f,0.2f,0.5)},
        };
        BufferDesc VertBuffDesc;
        VertBuffDesc.Name = "SamplePlugin: cube vertex buffer";
        VertBuffDesc.Usage = USAGE_DEFAULT;
        VertBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
        VertBuffDesc.uiSizeInBytes = sizeof(CubeVerts);
        BufferData VBData;
        VBData.pData = CubeVerts;
        VBData.DataSize = sizeof(CubeVerts);
        pDevice->CreateBuffer(VertBuffDesc, &VBData, &m_CubeVertexBuffer);
    }

    {
        Uint32 Indices[] =
        {
            2,0,1, 2,3,0,
            4,6,5, 4,7,6,
            0,7,4, 0,3,7,
            1,0,4, 1,4,5,
            1,5,2, 5,6,2,
            3,6,7, 3,2,6
        };
        BufferDesc IndBuffDesc;
        IndBuffDesc.Name = "SamplePlugin: cube index buffer";
        IndBuffDesc.Usage = USAGE_DEFAULT;
        IndBuffDesc.BindFlags = BIND_INDEX_BUFFER;
        IndBuffDesc.uiSizeInBytes = sizeof(Indices);
        BufferData IBData;
        IBData.pData = Indices;
        IBData.DataSize = sizeof(Indices);
        pDevice->CreateBuffer(IndBuffDesc, &IBData, &m_CubeIndexBuffer);
    }
}

void SamplePlugin::Render(Diligent::IDeviceContext *pContext, const float4x4 &ViewProjMatrix)
{
    {
        MapHelper<float4x4> CBConstants(pContext, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
        *CBConstants = ViewProjMatrix.Transpose();
    }

    Uint32 offset = 0;
    IBuffer *pBuffs[] = {m_CubeVertexBuffer};
    pContext->SetVertexBuffers(0, 1, pBuffs, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    pContext->SetIndexBuffer(m_CubeIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    pContext->SetPipelineState(m_PSO);
    pContext->CommitShaderResources(m_SRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawIndexedAttribs DrawAttrs(36, VT_UINT32, DRAW_FLAG_VERIFY_ALL);
    pContext->DrawIndexed(DrawAttrs);
}
