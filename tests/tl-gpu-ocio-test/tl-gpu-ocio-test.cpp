// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

// OpenColorIO display transforms through an SDL GPU pipeline, checked
// against OpenColorIO's own CPU processor.
//
// OpenColorIO emits Metal Shading Language as a function that takes its
// textures and samplers as arguments. The fragment entry point is written
// here from that signature, and each texture is bound at one of SDL GPU's
// slots. Metal compiles the source itself.
//
// Vulkan wants SPIR-V, which is what glslang makes of the GLSL OpenColorIO
// emits for it. That GLSL declares its samplers itself, in the set and from
// the binding asked for, so each texture is bound where the description
// says it was put.
#include <ftk/GPU/Shader.h>
#include <OpenColorIO/OpenColorIO.h>
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace OCIO = OCIO_NAMESPACE;

namespace
{
    struct Param { std::string type; std::string name; };

    // The parameters of the free function OCIO emits last.
    std::vector<Param> getParams(const std::string& text, const std::string& fn)
    {
        std::vector<Param> out;
        const size_t i = text.rfind("float4 " + fn + "(");
        if (std::string::npos == i) throw std::runtime_error("no entry point");
        const size_t j = text.find(')', i);
        std::string s = text.substr(i + 8 + fn.size(), j - (i + 8 + fn.size()));
        std::replace(s.begin(), s.end(), ',', ' ');
        std::stringstream ss(s);
        Param p;
        while (ss >> p.type >> p.name) out.push_back(p);
        return out;
    }

    SDL_GPUTexture* upload(SDL_GPUDevice* device, SDL_GPUTextureType type, SDL_GPUTextureFormat format,
        uint32_t w, uint32_t h, uint32_t d, const void* data, size_t byteCount)
    {
        SDL_GPUTextureCreateInfo info = {};
        info.type = type;
        info.format = format;
        info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
        info.width = w; info.height = h; info.layer_count_or_depth = d; info.num_levels = 1;
        SDL_GPUTexture* out = SDL_CreateGPUTexture(device, &info);
        if (!out) throw std::runtime_error(SDL_GetError());
        SDL_GPUTransferBufferCreateInfo tinfo = {};
        tinfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        tinfo.size = static_cast<uint32_t>(byteCount);
        SDL_GPUTransferBuffer* tb = SDL_CreateGPUTransferBuffer(device, &tinfo);
        std::memcpy(SDL_MapGPUTransferBuffer(device, tb, false), data, byteCount);
        SDL_UnmapGPUTransferBuffer(device, tb);
        SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
        SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(cmd);
        SDL_GPUTextureTransferInfo src = {};
        src.transfer_buffer = tb;
        SDL_GPUTextureRegion dst = {};
        dst.texture = out; dst.w = w; dst.h = h; dst.d = d;
        SDL_UploadToGPUTexture(pass, &src, &dst, false);
        SDL_EndGPUCopyPass(pass);
        SDL_SubmitGPUCommandBuffer(cmd);
        SDL_ReleaseGPUTransferBuffer(device, tb);
        return out;
    }

    // SDL GPU has no three channel textures.
    std::vector<float> toRGBA(const float* rgb, size_t count)
    {
        std::vector<float> out(count * 4);
        for (size_t i = 0; i < count; ++i)
        {
            out[i * 4 + 0] = rgb[i * 3 + 0];
            out[i * 4 + 1] = rgb[i * 3 + 1];
            out[i * 4 + 2] = rgb[i * 3 + 2];
            out[i * 4 + 3] = 1.F;
        }
        return out;
    }

    bool run(SDL_GPUDevice* device, const char* label, OCIO::ConstProcessorRcPtr processor)
    {
        const bool msl = SDL_GetGPUShaderFormats(device) & SDL_GPU_SHADERFORMAT_MSL;
        auto gpu = processor->getDefaultGPUProcessor();
        auto desc = OCIO::GpuShaderDesc::CreateShaderDesc();
        desc->setLanguage(msl ? OCIO::GPU_LANGUAGE_MSL_2_0 : OCIO::GPU_LANGUAGE_GLSL_VK_4_6);
        desc->setFunctionName("ocioDisplay");
        desc->setResourcePrefix("ocio");
        desc->setAllowTexture1D(false);
        if (!msl)
        {
            // The set a fragment stage's samplers are in, after the image.
            desc->setDescriptorSetIndex(2, 1);
        }
        gpu->extractGpuShaderInfo(desc);
        const std::string ocio = desc->getShaderText();

        // The textures, by name. Slot zero is the image. Metal's are given
        // slots in the order they come; the GLSL has chosen its own.
        struct Tex { SDL_GPUTexture* texture; SDL_GPUSampler* sampler; uint32_t slot; };
        std::map<std::string, Tex> textures;
        uint32_t slot = 1;
        const auto place = [msl, &slot](unsigned binding)
        {
            const uint32_t out = msl ? slot : binding;
            slot = std::max(slot, out + 1);
            return out;
        };
        const auto sampler = [device](OCIO::Interpolation interp)
        {
            SDL_GPUSamplerCreateInfo info = {};
            const SDL_GPUFilter f = OCIO::INTERP_NEAREST == interp ? SDL_GPU_FILTER_NEAREST : SDL_GPU_FILTER_LINEAR;
            info.min_filter = f; info.mag_filter = f;
            info.address_mode_u = info.address_mode_v = info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
            return SDL_CreateGPUSampler(device, &info);
        };
        for (unsigned i = 0; i < desc->getNum3DTextures(); ++i)
        {
            const char* textureName = nullptr; const char* samplerName = nullptr;
            unsigned edge = 0; OCIO::Interpolation interp = OCIO::INTERP_LINEAR;
            desc->get3DTexture(i, textureName, samplerName, edge, interp);
            const float* values = nullptr;
            desc->get3DTextureValues(i, values);
            const auto rgba = toRGBA(values, edge * edge * edge);
            textures[textureName] = { upload(device, SDL_GPU_TEXTURETYPE_3D, SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,
                edge, edge, edge, rgba.data(), rgba.size() * sizeof(float)), sampler(interp),
                place(desc->get3DTextureShaderBindingIndex(i)) };
        }
        for (unsigned i = 0; i < desc->getNumTextures(); ++i)
        {
            const char* textureName = nullptr; const char* samplerName = nullptr;
            unsigned w = 0, h = 0;
            OCIO::GpuShaderDesc::TextureType channel = OCIO::GpuShaderDesc::TEXTURE_RGB_CHANNEL;
            OCIO::GpuShaderDesc::TextureDimensions dims = OCIO::GpuShaderDesc::TEXTURE_2D;
            OCIO::Interpolation interp = OCIO::INTERP_LINEAR;
            desc->getTexture(i, textureName, samplerName, w, h, channel, dims, interp);
            const float* values = nullptr;
            desc->getTextureValues(i, values);
            h = std::max(h, 1u);
            if (OCIO::GpuShaderDesc::TEXTURE_RED_CHANNEL == channel)
            {
                textures[textureName] = { upload(device, SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREFORMAT_R32_FLOAT,
                    w, h, 1, values, w * h * sizeof(float)), sampler(interp),
                    place(desc->getTextureShaderBindingIndex(i)) };
            }
            else
            {
                const auto rgba = toRGBA(values, w * h);
                textures[textureName] = { upload(device, SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,
                    w, h, 1, rgba.data(), rgba.size() * sizeof(float)), sampler(interp),
                    place(desc->getTextureShaderBindingIndex(i)) };
            }
        }

        std::stringstream fs;
        if (msl)
        {
            // The fragment entry point, from the signature OCIO emitted.
            const auto params = getParams(ocio, "ocioDisplay");
            fs << "#include <metal_stdlib>\nusing namespace metal;\n" << ocio << "\n";
            fs << "struct VertexOut { float4 position [[position]]; float2 uv; };\n";
            fs << "fragment float4 fragmentMain(VertexOut in [[stage_in]],\n";
            fs << "    texture2d<float> image [[texture(0)]], sampler imageSampler [[sampler(0)]]";
            std::stringstream call;
            for (const auto& p : params)
            {
                if ("inPixel" == p.name) continue;
                const bool isSampler = "sampler" == p.type;
                const std::string textureName = isSampler ? p.name.substr(0, p.name.size() - 7) : p.name;
                const auto i = textures.find(textureName);
                if (i == textures.end()) throw std::runtime_error("unbound parameter: " + p.type + " " + p.name);
                fs << ",\n    " << p.type << " " << p.name << " [[" << (isSampler ? "sampler" : "texture") << "(" << i->second.slot << ")]]";
                call << p.name << ", ";
            }
            fs << ")\n{\n    return ocioDisplay(" << call.str() << "image.sample(imageSampler, in.uv));\n}\n";
        }
        else
        {
            fs << "#version 450\n";
            fs << "layout(location = 0) in vec2 uv;\n";
            fs << "layout(location = 0) out vec4 outColor;\n";
            fs << "layout(set = 2, binding = 0) uniform sampler2D image;\n";
            fs << ocio << "\n";
            fs << "void main()\n{\n    outColor = ocioDisplay(texture(image, uv));\n}\n";
        }
        const std::string fsSource = fs.str();
        const std::string vsSourceGLSL =
            "#version 450\n"
            "layout(location = 0) out vec2 uv;\n"
            "void main()\n"
            "{\n"
            "    vec2 p = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);\n"
            "    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n"
            "    uv = vec2(p.x, 1.0 - p.y);\n"
            "}\n";
        const std::string vsSource =
            "#include <metal_stdlib>\nusing namespace metal;\n"
            "struct VertexOut { float4 position [[position]]; float2 uv; };\n"
            "vertex VertexOut vertexMain(uint id [[vertex_id]])\n"
            "{\n"
            "    VertexOut out;\n"
            "    float2 p = float2((id << 1) & 2, id & 2);\n"
            "    out.position = float4(p * 2.0 - 1.0, 0.0, 1.0);\n"
            "    out.uv = float2(p.x, 1.0 - p.y);\n"
            "    return out;\n"
            "}\n";

        const Uint64 t0 = SDL_GetTicksNS();
        SDL_GPUShaderCreateInfo vsInfo = {};
        vsInfo.code = reinterpret_cast<const Uint8*>(vsSource.c_str()); vsInfo.code_size = vsSource.size() + 1;
        vsInfo.entrypoint = "vertexMain"; vsInfo.format = SDL_GPU_SHADERFORMAT_MSL; vsInfo.stage = SDL_GPU_SHADERSTAGE_VERTEX;
        SDL_GPUShaderCreateInfo fsInfo = {};
        fsInfo.code = reinterpret_cast<const Uint8*>(fsSource.c_str()); fsInfo.code_size = fsSource.size() + 1;
        fsInfo.entrypoint = "fragmentMain"; fsInfo.format = SDL_GPU_SHADERFORMAT_MSL; fsInfo.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
        fsInfo.num_samplers = slot;
        std::vector<uint32_t> vsSPIRV;
        std::vector<uint32_t> fsSPIRV;
        if (!msl)
        {
            vsSPIRV = ftk::gpu::compileGLSL(vsSourceGLSL, ftk::gpu::ShaderStage::Vertex);
            fsSPIRV = ftk::gpu::compileGLSL(fsSource, ftk::gpu::ShaderStage::Fragment);
            vsInfo.code = reinterpret_cast<const Uint8*>(vsSPIRV.data()); vsInfo.code_size = vsSPIRV.size() * sizeof(uint32_t);
            vsInfo.entrypoint = "main"; vsInfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
            fsInfo.code = reinterpret_cast<const Uint8*>(fsSPIRV.data()); fsInfo.code_size = fsSPIRV.size() * sizeof(uint32_t);
            fsInfo.entrypoint = "main"; fsInfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
        }
        SDL_GPUShader* vs = SDL_CreateGPUShader(device, &vsInfo);
        if (!vs) throw std::runtime_error(std::string("vertex shader: ") + SDL_GetError());
        SDL_GPUShader* fragment = SDL_CreateGPUShader(device, &fsInfo);
        if (!fragment) throw std::runtime_error(std::string("fragment shader: ") + SDL_GetError());

        const SDL_GPUTextureFormat targetFormat = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
        SDL_GPUColorTargetDescription targetDesc = {};
        targetDesc.format = targetFormat;
        SDL_GPUGraphicsPipelineCreateInfo pInfo = {};
        pInfo.vertex_shader = vs; pInfo.fragment_shader = fragment;
        pInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        // The device is made without depth clamping.
        pInfo.rasterizer_state.enable_depth_clip = true;
        pInfo.target_info.color_target_descriptions = &targetDesc;
        pInfo.target_info.num_color_targets = 1;
        SDL_GPUGraphicsPipeline* pipeline = SDL_CreateGPUGraphicsPipeline(device, &pInfo);
        if (!pipeline) throw std::runtime_error(std::string("pipeline: ") + SDL_GetError());
        const double compileMs = (SDL_GetTicksNS() - t0) / 1e6;

        // The picture: scene linear values past 1.0 in both directions.
        const uint32_t w = 256, h = 256;
        std::vector<float> image(w * h * 4);
        for (uint32_t y = 0; y < h; ++y)
            for (uint32_t x = 0; x < w; ++x)
            {
                float* p = &image[(y * w + x) * 4];
                p[0] = x / float(w - 1) * 4.F;
                p[1] = y / float(h - 1) * 2.F;
                p[2] = (x + y) / float(w + h - 2);
                p[3] = 1.F;
            }
        SDL_GPUTexture* imageTexture = upload(device, SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,
            w, h, 1, image.data(), image.size() * sizeof(float));
        SDL_GPUSamplerCreateInfo sInfo = {};
        sInfo.min_filter = sInfo.mag_filter = SDL_GPU_FILTER_NEAREST;
        sInfo.address_mode_u = sInfo.address_mode_v = sInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        SDL_GPUSampler* imageSampler = SDL_CreateGPUSampler(device, &sInfo);

        SDL_GPUTextureCreateInfo tInfo = {};
        tInfo.type = SDL_GPU_TEXTURETYPE_2D; tInfo.format = targetFormat;
        tInfo.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
        tInfo.width = w; tInfo.height = h; tInfo.layer_count_or_depth = 1; tInfo.num_levels = 1;
        SDL_GPUTexture* target = SDL_CreateGPUTexture(device, &tInfo);

        SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
        SDL_GPUColorTargetInfo color = {};
        color.texture = target; color.load_op = SDL_GPU_LOADOP_CLEAR; color.store_op = SDL_GPU_STOREOP_STORE;
        SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cmd, &color, 1, nullptr);
        SDL_BindGPUGraphicsPipeline(pass, pipeline);
        std::vector<SDL_GPUTextureSamplerBinding> bindings(slot);
        bindings[0] = { imageTexture, imageSampler };
        for (const auto& i : textures) bindings[i.second.slot] = { i.second.texture, i.second.sampler };
        SDL_BindGPUFragmentSamplers(pass, 0, bindings.data(), static_cast<Uint32>(bindings.size()));
        SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
        SDL_EndGPURenderPass(pass);

        SDL_GPUTransferBufferCreateInfo dInfo = {};
        dInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
        dInfo.size = w * h * 4 * sizeof(float);
        SDL_GPUTransferBuffer* download = SDL_CreateGPUTransferBuffer(device, &dInfo);
        SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
        SDL_GPUTextureRegion region = {};
        region.texture = target; region.w = w; region.h = h; region.d = 1;
        SDL_GPUTextureTransferInfo transfer = {};
        transfer.transfer_buffer = download;
        SDL_DownloadFromGPUTexture(copy, &region, &transfer);
        SDL_EndGPUCopyPass(copy);
        SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
        SDL_WaitForGPUFences(device, true, &fence, 1);
        SDL_ReleaseGPUFence(device, fence);
        std::vector<float> result(w * h * 4);
        std::memcpy(result.data(), SDL_MapGPUTransferBuffer(device, download, false), result.size() * sizeof(float));
        SDL_UnmapGPUTransferBuffer(device, download);

        // Everything made here, before the device goes: Vulkan's validation
        // counts what is left.
        SDL_ReleaseGPUTransferBuffer(device, download);
        SDL_ReleaseGPUTexture(device, target);
        SDL_ReleaseGPUTexture(device, imageTexture);
        SDL_ReleaseGPUSampler(device, imageSampler);
        for (const auto& i : textures)
        {
            SDL_ReleaseGPUTexture(device, i.second.texture);
            SDL_ReleaseGPUSampler(device, i.second.sampler);
        }
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
        SDL_ReleaseGPUShader(device, vs);
        SDL_ReleaseGPUShader(device, fragment);

        // What the CPU makes of the same picture.
        std::vector<float> reference = image;
        auto cpu = processor->getDefaultCPUProcessor();
        OCIO::PackedImageDesc img(reference.data(), w, h, 4);
        cpu->apply(img);
        float maxDiff = 0.F; double sum = 0.0;
        for (size_t i = 0; i < result.size(); ++i)
        {
            const float d = std::fabs(result[i] - reference[i]);
            maxDiff = std::max(maxDiff, d); sum += d;
        }
        const float* c = &result[(128 * w + 64) * 4];
        const float* r = &reference[(128 * w + 64) * 4];
        std::printf("%s: %u textures (%u 3D), shader %zu bytes, compiled in %.1f ms\n", label, slot - 1,
            desc->getNum3DTextures(), fsSource.size(), compileMs);
        std::printf("    GPU %.4f %.4f %.4f, CPU %.4f %.4f %.4f at (64,128)\n", c[0], c[1], c[2], r[0], r[1], r[2]);
        std::printf("    max difference %.5f, mean %.6f\n", maxDiff, sum / result.size());
        // The LUT is where the two differ most: the GPU blends eight
        // points of the cube and the CPU four.
        return maxDiff < 0.005F;
    }
}

int main(int, char**)
{
    bool ok = true;
    try
    {
        if (!SDL_Init(SDL_INIT_VIDEO)) throw std::runtime_error(SDL_GetError());
        SDL_GPUDevice* device = SDL_CreateGPUDevice(
            SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_SPIRV, true, nullptr);
        if (!device) throw std::runtime_error(SDL_GetError());
        std::printf("GPU driver: %s\n", SDL_GetGPUDeviceDriver(device));
        auto config = OCIO::Config::CreateFromBuiltinConfig("studio-config-latest");
        const std::string display = config->getDefaultDisplay();
        struct Case { std::string display; std::string view; };
        for (const Case& c : {
            Case{ display, config->getDefaultView(display.c_str()) },
            Case{ display, "Un-tone-mapped" },
            Case{ "Rec.2100-PQ - Display", "" } })
        {
            const std::string view = c.view.empty() ? config->getDefaultView(c.display.c_str()) : c.view;
            auto t = OCIO::DisplayViewTransform::Create();
            t->setSrc("ACEScg");
            t->setDisplay(c.display.c_str());
            t->setView(view.c_str());
            auto processor = config->getProcessor(t);
            const std::string label = c.display + " / " + view;
            ok &= run(device, label.c_str(), processor);
        }
        {
            // A LUT file, which is a 3D texture.
            // A 17 point cube with something nonlinear in it.
            const std::string cube =
                (std::filesystem::temp_directory_path() / "tl-gpu-ocio-test.cube").string();
            {
                std::ofstream f(cube);
                const int n = 17;
                f << "LUT_3D_SIZE " << n << "\n";
                for (int b = 0; b < n; ++b)
                    for (int g = 0; g < n; ++g)
                        for (int r = 0; r < n; ++r)
                        {
                            const float R = r / float(n - 1), G = g / float(n - 1), B = b / float(n - 1);
                            f << std::pow(R, .6F) * .9F + .1F * G << " " << std::pow(G, 1.4F) << " " <<
                                .5F * B + .5F * R * B << "\n";
                        }
            }
            auto t = OCIO::FileTransform::Create();
            t->setSrc(cube.c_str());
            t->setInterpolation(OCIO::INTERP_LINEAR);
            auto processor = OCIO::Config::CreateRaw()->getProcessor(t);
            ok &= run(device, "A 3D LUT file", processor);
        }
        SDL_DestroyGPUDevice(device);
        SDL_Quit();
    }
    catch (const std::exception& e) { std::printf("ERROR: %s\n", e.what()); return 1; }
    std::printf(ok ? "PASS\n" : "FAIL\n");
    return ok ? 0 : 1;
}
