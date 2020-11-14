#include "Common_3/OS/Interfaces/IApp.h"
#include "Common_3/OS/Interfaces/IFileSystem.h"
#include "Common_3/Renderer/IRenderer.h"
#include "Common_3/Renderer/IResourceLoader.h"
#include "Common_3/OS/Interfaces/IProfiler.h"
#include "Middleware_3/UI/AppUI.h"
#include "Common_3/OS/Interfaces/IInput.h"

// raytracing
#include "Common_3/Renderer/IRay.h"

const uint32_t gImageCount = 3;

Renderer* pRenderer = nullptr;

Queue*   pGraphicsQueue = nullptr;
CmdPool* pCmdPools[gImageCount] = { nullptr };
Cmd*     pCmds[gImageCount] = { nullptr };

SwapChain*    pSwapChain = nullptr;
RenderTarget* pDepthBuffer = nullptr;
Fence*        pRenderCompleteFences[gImageCount] = { nullptr };
Semaphore*    pImageAcquiredSemaphore = nullptr;
Semaphore*    pRenderCompleteSemaphores[gImageCount] = { nullptr };

Shader*   pTriangleShader = nullptr;
Buffer*   pTriangleVertexBuffer = nullptr;
Pipeline* pTrianglePipeline = nullptr;

RootSignature* pRootSignature = nullptr;

uint32_t gFrameIndex = 0;

/// UI and profiler
ProfileToken gGpuProfileToken = PROFILE_INVALID_TOKEN;
GuiComponent* pGuiWindow;
UIApp gAppUI;
TextDrawDesc gFrameTimeDraw = TextDrawDesc(0, 0xff00ffff, 18);

bool bToggleVSync = false;

// Uniform
struct UniformBlock
{
    uint32_t prog = 1;
    float mixWeight = 0.0f;
};

Texture* pTex0; // use as compute texture
Texture* pTex1;
Sampler* pTexSampler;
DescriptorSet* pDescriptorSetTexture = nullptr;
DescriptorSet* pDescriptorSetUniforms = nullptr;
Buffer* pUniformBuffer[gImageCount] = { nullptr };

UniformBlock gUniformData;

const char* pImageFileNames[] = { "brickwall"};

// compute shader
Shader* pComputeShader = nullptr;
Pipeline* pComputePipeline = nullptr;
RootSignature* pComputeRootSignature = nullptr;

DescriptorSet* pDescriptorSetComputeTexture = nullptr;
DescriptorSet* pDescriptorSetComputeUniforms = nullptr;
Buffer* pComputeUniformBuffer[gImageCount] = { nullptr };

struct ComputeUniformBlock
{
    float time = 0.0f;
};

ComputeUniformBlock gComputeUniformData;

#pragma region raytracing
Raytracing*             pRaytracing = nullptr;
AccelerationStructure*  pAS = nullptr;
Shader*	                pShaderRayGen = nullptr;
Shader*	                pShaderClosestHit = nullptr;
Shader*	                pShaderMiss = nullptr;
DescriptorSet*          pRaytracingASDescriptorSet = nullptr;
DescriptorSet*          pRaytracingRayGenCBDescriptorSet = nullptr;
Buffer*                 pRaytracingUniformBuffer[gImageCount] = {nullptr};
RootSignature*          pRaytracingRootSignature = nullptr;
Pipeline*               pRaytracingPipeline = nullptr;
RaytracingShaderTable*  pShaderTable = nullptr;

struct Viewport
{
    float left;
    float top;
    float right;
    float bottom;
};

struct PathTracingData
{
    Viewport viewport;
    Viewport stencil;
};

PathTracingData gPathTracingData = {};
#pragma endregion raytracing

class MyApplication : public IApp
{
public:
    MyApplication()
    {
        bToggleVSync = mSettings.mDefaultVSyncEnabled;
    }

    bool Init() override
    {
        // FILE PATHS
#if VULKAN
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_SOURCES, "Shaders/vk");
#elif DIRECT3D12
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_SOURCES, "Shaders/dx");
#endif
        fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG,   RD_SHADER_BINARIES, "CompiledShaders");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_GPU_CONFIG, "GPUCfg");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES, "Textures");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS, "Fonts");

        // Window and Renderer setup
        RendererDesc settings = { nullptr };
        settings.mShaderTarget = shader_target_6_3; // VERY IMPORTANT!!!!!!
        initRenderer(GetName(), &settings, &pRenderer);

        if (!pRenderer)
            return false;

        QueueDesc queueDesc = {};
        queueDesc.mType = QUEUE_TYPE_GRAPHICS;
        queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
        addQueue(pRenderer, &queueDesc, &pGraphicsQueue);

        initResourceLoaderInterface(pRenderer);

        // Load texture
//        TextureLoadDesc textureDesc0 = {};
//        textureDesc0.pFileName = pImageFileNames[0];
//        textureDesc0.ppTexture = &pTex0;
//        addResource(&textureDesc0, nullptr);

        // Create empty texture for output of compute shader
        TextureLoadDesc texture0LoadDesc = {};
        TextureDesc texture0Desc = {};
        texture0Desc.mWidth = 512;
        texture0Desc.mHeight = 512;
        texture0Desc.mDepth = 1;
        texture0Desc.mArraySize = 1;
        texture0Desc.mMipLevels = 1;
        texture0Desc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
        texture0Desc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
        texture0Desc.mSampleCount = SAMPLE_COUNT_1;
        texture0Desc.mHostVisible = false;
        texture0Desc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
        texture0LoadDesc.pDesc = &texture0Desc;
        texture0LoadDesc.ppTexture = &pTex0;
        addResource(&texture0LoadDesc, nullptr);

        TextureLoadDesc textureDesc1 = {};
        textureDesc1.pFileName = pImageFileNames[0];
        textureDesc1.ppTexture = &pTex1;
        addResource(&textureDesc1, nullptr);

        for (uint32_t i = 0; i < gImageCount; i++)
        {
            CmdPoolDesc cmdPoolDesc = {};
            cmdPoolDesc.pQueue = pGraphicsQueue;
            addCmdPool(pRenderer, &cmdPoolDesc, &pCmdPools[i]);

            CmdDesc cmdDesc = {};
            cmdDesc.pPool = pCmdPools[i];
            addCmd(pRenderer, &cmdDesc, &pCmds[i]);

            addFence(pRenderer, &pRenderCompleteFences[i]);
            addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
        }
        addSemaphore(pRenderer, &pImageAcquiredSemaphore);

        ShaderLoadDesc triangleShaderLoadDesc = {};
        triangleShaderLoadDesc.mStages[0] = { "texture.vert", nullptr, 0 };
        triangleShaderLoadDesc.mStages[1] = { "texture.frag", nullptr, 0 };
        addShader(pRenderer, &triangleShaderLoadDesc, &pTriangleShader);

        ShaderLoadDesc computeShaderLoadDesc = {};
        computeShaderLoadDesc.mStages[0] = {"noise.comp", NULL, 0, "MainCS"};
        addShader(pRenderer, &computeShaderLoadDesc, &pComputeShader);

        // sampler desc
        SamplerDesc samplerDesc = { FILTER_LINEAR,
                                    FILTER_LINEAR,
                                    MIPMAP_MODE_LINEAR,
                                    ADDRESS_MODE_REPEAT,
                                    ADDRESS_MODE_REPEAT,
                                    ADDRESS_MODE_REPEAT };
        addSampler(pRenderer, &samplerDesc, &pTexSampler);

        const char* pStaticSamplers[] = { "uSampler0" };
        RootSignatureDesc rootSignatureDesc = {};
        rootSignatureDesc.mStaticSamplerCount = 1;
        rootSignatureDesc.ppStaticSamplerNames = pStaticSamplers;
        rootSignatureDesc.ppStaticSamplers = &pTexSampler;
        rootSignatureDesc.mShaderCount = 1;
        rootSignatureDesc.ppShaders = &pTriangleShader;
        addRootSignature(pRenderer, &rootSignatureDesc, &pRootSignature);

        RootSignatureDesc computeRootDesc = {};
        computeRootDesc.mShaderCount = 1;
        computeRootDesc.ppShaders = &pComputeShader;
        addRootSignature(pRenderer, &computeRootDesc, &pComputeRootSignature);

        DescriptorSetDesc desc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
        addDescriptorSet(pRenderer, &desc, &pDescriptorSetTexture);
        desc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
        addDescriptorSet(pRenderer, &desc, &pDescriptorSetUniforms);

        desc = { pComputeRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
        addDescriptorSet(pRenderer, &desc, &pDescriptorSetComputeTexture);
        desc = { pComputeRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
        addDescriptorSet(pRenderer, &desc, &pDescriptorSetComputeUniforms);


        // Generate vertex buffer
        float pPoints[] = {
                // Positions        // Colors         // UVs
                 0.5f,  0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, // Top Right
                -0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, // Bottom Left
                -0.5f,  0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, // Top Left

                 0.5f,  0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, // Top Right
                 0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, // Bottom Right
                -0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, // Bottom Left
        };

        uint64_t pointDataSize = 6 * 8 * sizeof(float);
        BufferLoadDesc pointVbDesc = {};
        pointVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
        pointVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_ONLY;
        pointVbDesc.mDesc.mSize = pointDataSize;
        pointVbDesc.pData = pPoints;
        pointVbDesc.ppBuffer = &pTriangleVertexBuffer;
        addResource(&pointVbDesc, nullptr);

        // uniform buffer
        BufferLoadDesc ubDesc = {};
        ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        ubDesc.mDesc.mSize = sizeof(UniformBlock);
        ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        ubDesc.pData = nullptr;
        for (uint32_t i = 0; i < gImageCount; i++)
        {
            ubDesc.ppBuffer = &pUniformBuffer[i];
            addResource(&ubDesc, nullptr);
        }

        ubDesc.mDesc.mSize = sizeof(ComputeUniformBlock);
        for (uint32_t i = 0; i < gImageCount; i++)
        {
            ubDesc.ppBuffer = &pComputeUniformBuffer[i];
            addResource(&ubDesc, nullptr);
        }

        // GUI & profiler
        if (!gAppUI.Init(pRenderer))
            return false;

        gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf");

        if (!initInputSystem(pWindow))
            return false;

        // Initialize microprofiler and it's UI.
        initProfiler();

        // Gpu profiler can only be added after initProfile.
        gGpuProfileToken = addGpuProfiler(pRenderer, pGraphicsQueue, "Graphics");

        // GUI
        GuiDesc guiDesc = {};
        guiDesc.mStartPosition = vec2(mSettings.mWidth * 0.01f, mSettings.mHeight * 0.2f);
        pGuiWindow = gAppUI.AddGuiComponent(GetName(), &guiDesc);
        pGuiWindow->AddWidget(CheckboxWidget("Toggle VSync\t\t\t\t\t", &bToggleVSync));

        static const char* progLabels[] = { "Vertex Color", "Tex0", "Tex1", "Tex blend", nullptr };
        static const uint32_t ProgIndex[] = { 0, 1, 2, 3, 4 };
        const size_t NUM_THEMES = sizeof(progLabels) / sizeof(const char*) - 1;    // -1 for the NULL element
        DropdownWidget ProgDropdown("Theme", &gUniformData.prog, progLabels, ProgIndex, NUM_THEMES);
        pGuiWindow->AddWidget(ProgDropdown);

        SliderFloatWidget MixWeightSlider("mixWeight", &gUniformData.mixWeight, 0.0f, 1.0f);
        pGuiWindow->AddWidget(MixWeightSlider);

        // App Actions
        InputActionDesc actionDesc = { InputBindings::BUTTON_DUMP, [](InputActionContext* ctx) {  dumpProfileData(((Renderer*)ctx->pUserData), ((Renderer*)ctx->pUserData)->pName); return true; }, pRenderer };
        addInputAction(&actionDesc);
        actionDesc = { InputBindings::BUTTON_FULLSCREEN, [](InputActionContext* ctx) { toggleFullscreen(((IApp*)ctx->pUserData)->pWindow); return true; }, this };
        addInputAction(&actionDesc);
        actionDesc = { InputBindings::BUTTON_EXIT, [](InputActionContext* ctx) { requestShutdown(); return true; } };
        addInputAction(&actionDesc);
        actionDesc =
                {
                    InputBindings::BUTTON_ANY,
                    [](InputActionContext* ctx)
                    {
                        bool capture = gAppUI.OnButton(ctx->mBinding, ctx->mBool, ctx->pPosition);
                        setEnableCaptureInput(capture && INPUT_ACTION_PHASE_CANCELED != ctx->mPhase);
                        return true;
                    },
                    this
                };
        addInputAction(&actionDesc);

#pragma region Init Raytracing
        if (!isRaytracingSupported(pRenderer))
        {
            pGuiWindow->AddWidget(LabelWidget("Raytracing is not supported on this GPU"));
            return true;
        }

        /************************************************************************/
        // Raytracing setup
        /************************************************************************/
        initRaytracing(pRenderer, &pRaytracing);

        uint32_t indices[] = { 0, 1, 2 };

        float depthValue = 1.0;
        float offset = 0.7f;
        float vertices[] =
                {
                    0, -offset, depthValue ,
                    -offset, offset, depthValue,
                    offset, offset, depthValue
                };

        /************************************************************************/
        // Creation Acceleration Structure
        /************************************************************************/
        AccelerationStructureGeometryDesc geometryDesc = {};
        geometryDesc.mFlags = ACCELERATION_STRUCTURE_GEOMETRY_FLAG_OPAQUE;
        geometryDesc.pVertexArray = vertices;
        geometryDesc.mVertexCount = 3;
        geometryDesc.pIndices32 = indices;
        geometryDesc.mIndexCount = 3;
        geometryDesc.mIndexType = INDEX_TYPE_UINT32;

        AccelerationStructureDescBottom bootomASDesc = {};
        bootomASDesc.mDescCount = 1;
        bootomASDesc.pGeometryDescs = &geometryDesc;
        bootomASDesc.mFlags = ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

        AccelerationStructureDescTop topASDesc = {};
        topASDesc.mBottomASDesc = &bootomASDesc;

        // The transformation matrices for the instances
        mat4 transformation = mat4::identity(); // Identity

        //Construct descriptions for Acceleration Structures Instances
        AccelerationStructureInstanceDesc instanceDesc = {};
        instanceDesc.mFlags = ACCELERATION_STRUCTURE_INSTANCE_FLAG_NONE;
        instanceDesc.mInstanceContributionToHitGroupIndex = 0;
        instanceDesc.mInstanceID = 0;
        instanceDesc.mInstanceMask = 1;
        memcpy(instanceDesc.mTransform, &transformation, sizeof(float[12]));
        instanceDesc.mAccelerationStructureIndex = 0;

        topASDesc.mInstancesDescCount = 1;
        topASDesc.pInstanceDescs = &instanceDesc;
        addAccelerationStructure(pRaytracing, &topASDesc, &pAS);

        waitForAllResourceLoads();

        // Build Acceleration Structure
        RaytracingBuildASDesc buildAsDesc = {};
        unsigned bottomASIndices[] = { 0 };
        buildAsDesc.ppAccelerationStructures = &pAS;
        buildAsDesc.pBottomASIndices = &bottomASIndices[0];
        buildAsDesc.mBottomASIndicesCount = 1;
        buildAsDesc.mCount = 1;
        beginCmd(pCmds[0]);
        cmdBuildAccelerationStructure(pCmds[0], pRaytracing, &buildAsDesc);
        endCmd(pCmds[0]);

        QueueSubmitDesc submitDesc = {};
        submitDesc.mCmdCount = 1;
        submitDesc.ppCmds = pCmds;
        submitDesc.pSignalFence = pRenderCompleteFences[0];
        submitDesc.mSubmitDone = true;
        queueSubmit(pGraphicsQueue, &submitDesc);
        waitForFences(pRenderer, 1, &pRenderCompleteFences[0]);

        /************************************************************************/
        // Create Raytracing Shaders
        /************************************************************************/
        {
            ShaderLoadDesc desc = {};
            desc.mStages[0] = { "RayGen.rgen", nullptr, 0, "rayGen" };
            desc.mTarget = shader_target_6_3;
            addShader(pRenderer, &desc, &pShaderRayGen);

            desc.mStages[0] = { "ClosestHit.rchit", nullptr, 0, "chs" };
            addShader(pRenderer, &desc, &pShaderClosestHit);

            desc.mStages[0] = { "Miss.rmiss", nullptr, 0, "miss" };
            addShader(pRenderer, &desc, &pShaderMiss);
        }

        Shader* pRayTracingShaders[] = {pShaderRayGen, pShaderClosestHit, pShaderMiss};
        RootSignatureDesc raytracingSignatureDesc = {};
        raytracingSignatureDesc.ppShaders = pRayTracingShaders;
        raytracingSignatureDesc.mShaderCount = 3;
        raytracingSignatureDesc.mStaticSamplerCount = 0;
        addRootSignature(pRenderer, &raytracingSignatureDesc, &pRaytracingRootSignature);

        /************************************************************************/
        // Create Raytracing Pipelines
        /************************************************************************/
        RaytracingHitGroup hitGroups[2] = {};
        hitGroups[0].pClosestHitShader = pShaderClosestHit;
        hitGroups[0].pHitGroupName = "hitGroup";

        hitGroups[1].pHitGroupName = "missHitGroup";

        Shader* pMissShaders[] = { pShaderMiss };
        PipelineDesc rtPipelineDesc = {};
        rtPipelineDesc.mType = PIPELINE_TYPE_RAYTRACING;
        RaytracingPipelineDesc& pipelineDesc = rtPipelineDesc.mRaytracingDesc;
        pipelineDesc.mAttributeSize	         = sizeof(float2);
        pipelineDesc.mMaxTraceRecursionDepth = 5;
        pipelineDesc.mPayloadSize            = sizeof(float4);
        pipelineDesc.pGlobalRootSignature	 = pRaytracingRootSignature;
        pipelineDesc.pRayGenShader			 = pShaderRayGen;
        pipelineDesc.pRayGenRootSignature	 = nullptr;// pRayGenSignature; //nullptr to bind empty LRS
        pipelineDesc.ppMissShaders			 = pMissShaders;
        pipelineDesc.mMissShaderCount		 = 1;
        pipelineDesc.pHitGroups				 = hitGroups;
        pipelineDesc.mHitGroupCount			 = 2;
        pipelineDesc.pRaytracing			 = pRaytracing;
        pipelineDesc.mMaxRaysCount = 512 * 512;
        addPipeline(pRenderer, &rtPipelineDesc, &pRaytracingPipeline);

        /************************************************************************/
        // Create Shader Binding Table to connect Pipeline with Acceleration Structure
        /************************************************************************/
        BufferLoadDesc rtUBDesc = {};
        rtUBDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        rtUBDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        rtUBDesc.mDesc.mStartState = RESOURCE_STATE_COMMON;
        rtUBDesc.mDesc.mSize = sizeof(PathTracingData);
        for (uint32_t i = 0; i < gImageCount; i++)
        {
            ubDesc.ppBuffer = &pRaytracingUniformBuffer[i];
            addResource(&ubDesc, NULL);
        }

        desc = { pRaytracingRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
        addDescriptorSet(pRenderer, &desc, &pRaytracingASDescriptorSet);
        desc = { pRaytracingRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
        addDescriptorSet(pRenderer, &desc, &pRaytracingRayGenCBDescriptorSet);

        const char* hitGroupNames[2] = { "hitGroup", "missHitGroup" };
        const char* missShaderNames[2] = { "miss" };

        RaytracingShaderTableDesc shaderTableDesc = {};
        shaderTableDesc.pPipeline = pRaytracingPipeline;
        shaderTableDesc.pRayGenShader = "rayGen";
        shaderTableDesc.mMissShaderCount = 1;
        shaderTableDesc.pMissShaders = missShaderNames;
        shaderTableDesc.mHitGroupCount = 2;
        shaderTableDesc.pHitGroups = hitGroupNames;
        addRaytracingShaderTable(pRaytracing, &shaderTableDesc, &pShaderTable);

        waitForAllResourceLoads();

        DescriptorData rtParams[1] = {};
        rtParams[0].pName = "rayGenCB";
        for (uint32_t i = 0; i < gImageCount; i++)
        {
            rtParams[0].ppBuffers = &pRaytracingUniformBuffer[i];
            updateDescriptorSet(pRenderer, i, pRaytracingRayGenCBDescriptorSet, 1, rtParams);
        }

        gPathTracingData.stencil.top = 0.9f;
        gPathTracingData.stencil.bottom = -0.9f;
        gPathTracingData.stencil.left = -0.9f;
        gPathTracingData.stencil.right = 0.9f;

        gPathTracingData.viewport.top = 1.0f;
        gPathTracingData.viewport.bottom = -1.0f;
        gPathTracingData.viewport.left = -1.0f;
        gPathTracingData.viewport.right = 1.0f;

#pragma endregion Init Raytracing

        waitForAllResourceLoads();

        // Prepare descriptor sets
        DescriptorData params[2] = {};
        params[0].pName = "Tex0";
        params[0].ppTextures = &pTex0;
        params[1].pName = "Tex1";
        params[1].ppTextures = &pTex1;
        updateDescriptorSet(pRenderer, 0, pDescriptorSetTexture, 2, params);

        params[0].pName = "Tex0";
        params[0].ppTextures = &pTex0;
        updateDescriptorSet(pRenderer, 0, pDescriptorSetComputeTexture, 1, params);

        for (uint32_t i = 0; i < gImageCount; ++i)
        {
            DescriptorData ubparams[1] = {};
            ubparams[0].pName = "uniformBlock";
            ubparams[0].ppBuffers = &pUniformBuffer[i];
            updateDescriptorSet(pRenderer, i, pDescriptorSetUniforms, 1, ubparams);

            ubparams[0].pName = "computeUniformBlock";
            ubparams[0].ppBuffers = &pComputeUniformBuffer[i];
            updateDescriptorSet(pRenderer, i, pDescriptorSetComputeUniforms, 1, ubparams);
        }

        return true;
    }
    void Exit() override
    {
        waitQueueIdle(pGraphicsQueue);

        exitInputSystem();

        // GUI & profiler
        gAppUI.Exit();
        exitProfiler();

#pragma region remove raytracing
        if (pRaytracing != nullptr)
        {
            removeDescriptorSet(pRenderer, pRaytracingASDescriptorSet);
            removeDescriptorSet(pRenderer, pRaytracingRayGenCBDescriptorSet);

            removeRaytracingShaderTable(pRaytracing, pShaderTable);
            removePipeline(pRenderer, pRaytracingPipeline);
            removeRootSignature(pRenderer, pRaytracingRootSignature);

            for (uint32_t i = 0; i < gImageCount; i++)
            {
                removeResource(pRaytracingUniformBuffer[i]);
            }

            removeShader(pRenderer, pShaderRayGen);
            removeShader(pRenderer, pShaderClosestHit);
            removeShader(pRenderer, pShaderMiss);
            removeAccelerationStructure(pRaytracing, pAS);
            removeRaytracing(pRenderer, pRaytracing);
        }
#pragma endregion remove raytracing

        // uniforms
        for (uint32_t i = 0; i < gImageCount; ++i)
        {
            removeResource(pUniformBuffer[i]);
            removeResource(pComputeUniformBuffer[i]);
        }

        removeDescriptorSet(pRenderer, pDescriptorSetComputeUniforms);
        removeDescriptorSet(pRenderer, pDescriptorSetComputeTexture);
        removeDescriptorSet(pRenderer, pDescriptorSetTexture);
        removeDescriptorSet(pRenderer, pDescriptorSetUniforms);

        removeResource(pTex0);
        removeResource(pTex1);

        removeSampler(pRenderer, pTexSampler);

        removeResource(pTriangleVertexBuffer);

        removeRootSignature(pRenderer, pRootSignature);
        removeRootSignature(pRenderer, pComputeRootSignature);
        removeShader(pRenderer, pTriangleShader);
        removeShader(pRenderer, pComputeShader);

        for (uint32_t i = 0; i < gImageCount; ++i)
        {
            removeFence(pRenderer, pRenderCompleteFences[i]);
            removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);

            removeCmd(pRenderer, pCmds[i]);
            removeCmdPool(pRenderer, pCmdPools[i]);
        }
        removeSemaphore(pRenderer, pImageAcquiredSemaphore);

        exitResourceLoaderInterface(pRenderer);
        removeQueue(pRenderer, pGraphicsQueue);
        removeRenderer(pRenderer);
    }

    bool Load() override
    {
        if (!addSwapChain())
            return false;

        if (!gAppUI.Load(pSwapChain->ppRenderTargets, 1))
            return false;

        loadProfilerUI(&gAppUI, mSettings.mWidth, mSettings.mHeight);

        // layout and pipeline for triangle draw
        VertexLayout vertexLayout = {};
        vertexLayout.mAttribCount = 3;
        vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
        vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
        vertexLayout.mAttribs[0].mBinding = 0;
        vertexLayout.mAttribs[0].mLocation = 0;
        vertexLayout.mAttribs[0].mOffset = 0;
        vertexLayout.mAttribs[1].mSemantic = SEMANTIC_COLOR;
        vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
        vertexLayout.mAttribs[1].mBinding = 0;
        vertexLayout.mAttribs[1].mLocation = 1;
        vertexLayout.mAttribs[1].mOffset = 3 * sizeof(float);
        vertexLayout.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
        vertexLayout.mAttribs[2].mFormat = TinyImageFormat_R32G32_SFLOAT;
        vertexLayout.mAttribs[2].mBinding = 0;
        vertexLayout.mAttribs[2].mLocation = 2;
        vertexLayout.mAttribs[2].mOffset = 6 * sizeof(float);

        RasterizerStateDesc rasterizerStateDesc = {};
        rasterizerStateDesc.mCullMode = CULL_MODE_NONE;

        PipelineDesc pipelineDesc = {};
        pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
        GraphicsPipelineDesc& graphicsPipelineDesc = pipelineDesc.mGraphicsDesc;
        graphicsPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        graphicsPipelineDesc.mRenderTargetCount = 1;
        graphicsPipelineDesc.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
        graphicsPipelineDesc.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
        graphicsPipelineDesc.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
        graphicsPipelineDesc.pRootSignature = pRootSignature;
        graphicsPipelineDesc.pShaderProgram = pTriangleShader;
        graphicsPipelineDesc.pVertexLayout = &vertexLayout;
        graphicsPipelineDesc.pRasterizerState = &rasterizerStateDesc;
        addPipeline(pRenderer, &pipelineDesc, &pTrianglePipeline);

        pipelineDesc = {};
        pipelineDesc.mType = PIPELINE_TYPE_COMPUTE;
        ComputePipelineDesc& computePipelineDesc = pipelineDesc.mComputeDesc;
        computePipelineDesc.pRootSignature = pComputeRootSignature;
        computePipelineDesc.pShaderProgram = pComputeShader;
        addPipeline(pRenderer, &pipelineDesc, &pComputePipeline);

#pragma region raytracing
        if (pRaytracing != nullptr)
        {
            DescriptorData params[2] = {};
            params[0].pName = "Tex0";
            params[0].ppTextures = &pTex0;
            params[1].pName = "gRtScene";
            params[1].ppAccelerationStructures = &pAS;
            updateDescriptorSet(pRenderer, 0, pRaytracingASDescriptorSet, 2, params);
        }
#pragma endregion raytracing

        return true;
    }
    void Unload() override
    {
        waitQueueIdle(pGraphicsQueue);

        unloadProfilerUI();
        gAppUI.Unload();

        removePipeline(pRenderer, pComputePipeline);
        removePipeline(pRenderer, pTrianglePipeline);

        removeSwapChain(pRenderer, pSwapChain);
    }

    void Update(float deltaTime) override
    {
        if (pSwapChain->mEnableVsync != bToggleVSync)
        {
            waitQueueIdle(pGraphicsQueue);
            gFrameIndex = 0;
            ::toggleVSync(pRenderer, &pSwapChain);
        }

        updateInputSystem(mSettings.mWidth, mSettings.mHeight);

        gAppUI.Update(deltaTime);

        // Uniform update
        //gUniformData.time += deltaTime * 1000.0f;
        gComputeUniformData.time += deltaTime;
    }
    void Draw() override
    {
        uint32_t swapchainImageIndex;
        acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, nullptr, &swapchainImageIndex);

        RenderTarget* renderTarget = pSwapChain->ppRenderTargets[swapchainImageIndex];
        Semaphore* renderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
        Fence* renderCompleteFence = pRenderCompleteFences[gFrameIndex];

        // Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
        FenceStatus fenceStatus;
        getFenceStatus(pRenderer, renderCompleteFence, &fenceStatus);
        if (fenceStatus == FENCE_STATUS_INCOMPLETE)
            waitForFences(pRenderer, 1, &renderCompleteFence);

        //  Update uniform buffers
        BufferUpdateDesc bufferCbv = { pUniformBuffer[gFrameIndex] };
        beginUpdateResource(&bufferCbv);
        *(UniformBlock*)bufferCbv.pMappedData = gUniformData;
        endUpdateResource(&bufferCbv, NULL);

        bufferCbv = { pComputeUniformBuffer[gFrameIndex] };
        beginUpdateResource(&bufferCbv);
        *(ComputeUniformBlock*)bufferCbv.pMappedData = gComputeUniformData;
        endUpdateResource(&bufferCbv, NULL);

#pragma region raytracing
        BufferUpdateDesc bufferUpdate = { pRaytracingUniformBuffer[gFrameIndex] };
        beginUpdateResource(&bufferUpdate);
        *(PathTracingData*)bufferUpdate.pMappedData = gPathTracingData;
        endUpdateResource(&bufferUpdate, NULL);
#pragma endregion raytracing

        // Reset cmd pool for this frame
        resetCmdPool(pRenderer, pCmdPools[gFrameIndex]);

        Cmd* cmd = pCmds[gFrameIndex];
        beginCmd(cmd);

        cmdBeginGpuFrameProfile(cmd, gGpuProfileToken);

        // compute shader
//        cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Compute Pass");
//
//        cmdBindPipeline(cmd, pComputePipeline);
//        cmdBindDescriptorSet(cmd, 0, pDescriptorSetComputeTexture);
//        cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetComputeUniforms);
//
//        const uint32_t* pThreadGroupSize = pComputeShader->pReflection->mStageReflections[0].mNumThreadsPerGroup;
//        cmdDispatch(cmd, (512 / pThreadGroupSize[0]), (512 / pThreadGroupSize[1]), pThreadGroupSize[2]);
//
//        cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);

#pragma region raytracing
        // raytracing pass(compute)
        if (pRaytracing != nullptr)
        {
            cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Raytracing Pass");

            cmdBindPipeline(cmd, pRaytracingPipeline);

            cmdBindDescriptorSet(cmd, 0, pRaytracingASDescriptorSet);
            cmdBindDescriptorSet(cmd, gFrameIndex, pRaytracingRayGenCBDescriptorSet);

            RaytracingDispatchDesc dispatchDesc = {};
            dispatchDesc.mHeight = 512;
            dispatchDesc.mWidth = 512;
            dispatchDesc.pShaderTable = pShaderTable;

            cmdDispatchRays(cmd, pRaytracing, &dispatchDesc);

            cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
        }
#pragma endregion raytracing

        TextureBarrier texbarriers[] = {
                { pTex0, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE },
        };
        RenderTargetBarrier rtbarriers[] = {
                { renderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
        };
        cmdResourceBarrier(cmd, 0, nullptr, 1, texbarriers, 1, rtbarriers);

        // simply record the screen cleaning command
        LoadActionsDesc loadActionsDesc = {};
        loadActionsDesc.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
        loadActionsDesc.mClearColorValues[0].r = 0.2f;
        loadActionsDesc.mClearColorValues[0].g = 0.3f;
        loadActionsDesc.mClearColorValues[0].b = 0.3f;
        loadActionsDesc.mClearColorValues[0].a = 1.0f;
        cmdBindRenderTargets(cmd, 1, &renderTarget, nullptr, &loadActionsDesc, nullptr, nullptr, -1, -1);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)renderTarget->mWidth, (float)renderTarget->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, renderTarget->mWidth, renderTarget->mHeight);

        // draw triangle
        cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw pass");
        const uint32_t triangleVbStride = sizeof(float) * 8;
        cmdBindPipeline(cmd, pTrianglePipeline);
        cmdBindDescriptorSet(cmd, 0, pDescriptorSetTexture);
        cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetUniforms);
        cmdBindVertexBuffer(cmd, 1, &pTriangleVertexBuffer, &triangleVbStride, nullptr);
        cmdDraw(cmd, 6, 0);
        cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);

        // draw GUI
        loadActionsDesc = {};
        loadActionsDesc.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
        cmdBindRenderTargets(cmd, 1, &renderTarget, NULL, &loadActionsDesc, NULL, NULL, -1, -1);
        cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "UI pass");

        const float txtIndent = 8.f;
        float2 txtSizePx = cmdDrawCpuProfile(cmd, float2(txtIndent, 15.f), &gFrameTimeDraw);
        cmdDrawGpuProfile(cmd, float2(txtIndent, txtSizePx.y + 30.f), gGpuProfileToken, &gFrameTimeDraw);

        cmdDrawProfilerUI();

        gAppUI.Gui(pGuiWindow);
        gAppUI.Draw(cmd);

        cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);

        // end draw
        cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

        rtbarriers[0] = {renderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
        texbarriers[0] = {pTex0, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS};
        cmdResourceBarrier(cmd, 0, nullptr, 1, texbarriers, 1, rtbarriers);


        cmdEndGpuFrameProfile(cmd, gGpuProfileToken);
        endCmd(cmd);


        QueueSubmitDesc submitDesc = {};
        submitDesc.mCmdCount = 1;
        submitDesc.mSignalSemaphoreCount = 1;
        submitDesc.mWaitSemaphoreCount = 1;
        submitDesc.ppCmds = &cmd;
        submitDesc.ppSignalSemaphores = &renderCompleteSemaphore;
        submitDesc.ppWaitSemaphores = &pImageAcquiredSemaphore;
        submitDesc.pSignalFence = renderCompleteFence;
        queueSubmit(pGraphicsQueue, &submitDesc);

        QueuePresentDesc presentDesc = {};
        presentDesc.mIndex = swapchainImageIndex;
        presentDesc.mWaitSemaphoreCount = 1;
        presentDesc.pSwapChain = pSwapChain;
        presentDesc.ppWaitSemaphores = &renderCompleteSemaphore;
        presentDesc.mSubmitDone = true;
        queuePresent(pGraphicsQueue, &presentDesc);

        flipProfiler();

        gFrameIndex = (gFrameIndex + 1) % gImageCount;
    }

    const char* GetName() override { return "Raytracing triangle"; }

    bool addSwapChain()
    {
        SwapChainDesc swapChainDesc = {};
        swapChainDesc.mWindowHandle = pWindow->handle;
        swapChainDesc.mPresentQueueCount = 1;
        swapChainDesc.ppPresentQueues = &pGraphicsQueue;
        swapChainDesc.mWidth = mSettings.mWidth;
        swapChainDesc.mHeight = mSettings.mHeight;
        swapChainDesc.mImageCount = gImageCount;
        swapChainDesc.mColorFormat = getRecommendedSwapchainFormat(true);
        swapChainDesc.mEnableVsync = mSettings.mDefaultVSyncEnabled;
        ::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

        return pSwapChain != nullptr;
    }
};

//DEFINE_APPLICATION_MAIN(MyApplication);

extern int WindowsMain(int argc, char** argv, IApp* app);

int main(int argc, char** argv)
{
    MyApplication app;
    return WindowsMain(argc, argv, &app);
}
