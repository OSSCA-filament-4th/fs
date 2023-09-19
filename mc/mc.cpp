/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <filament/Camera.h>
#include <filament/Engine.h>
#include <filament/IndexBuffer.h>
#include <filament/Material.h>
#include <filament/MaterialInstance.h>
#include <filament/RenderableManager.h>
#include <filament/Scene.h>
#include <filament/Skybox.h>
#include <filament/TransformManager.h>
#include <filament/VertexBuffer.h>
#include <filament/View.h>

#include <utils/EntityManager.h>

#include <filamentapp/Config.h>
#include <filamentapp/FilamentApp.h>

#include <getopt/getopt.h>

#include <cmath>
#include <iostream>


#include "generated/resources/resources.h"

using namespace filament;
using utils::Entity;
using utils::EntityManager;

static const int   gCircleCount = 20;
static const int   gSegments    = 100;
static const float gRadius      = 0.5f;
static const float gSpeed       = 100.0f;

struct Vertex {
    filament::math::float2 mPos;
    uint32_t               mColor;
};

struct Circle {
    filament::math::float2 mCenter;
    filament::math::float2 mVelocity;
    Vertex                 mVertices[gSegments + 1];
    uint16_t               mIndicies[gSegments * 3];
    float                  mRadius;
    float                  mMass;
    uint32_t               mColor;
};

struct Renderable_t
{
    Material*     mMaterial;
    VertexBuffer* mVertexBuffer;
    IndexBuffer*  mIndexBuffer;
    Circle        mCircle;
    Entity        mRenderable;
};

struct App {
    Config        mConfig;
    Camera*       mCamera;
    Skybox*       mSkybox;
    Entity        mCameraObj;
    
    Renderable_t  mRenderableArr[gCircleCount];
    uint32_t      mCurrentCount;
};

float randf() { return (float)rand() / (float)RAND_MAX; }

float randf(float a, float b) { return a + randf() * (b - a); }

float dist(filament::math::float2 left, filament::math::float2 right) 
{
    return sqrt((right.x - left.x) * (right.x - left.x) + (right.y - left.y) * (right.y - left.y));
}

void Collision(Circle& a, Circle& b, float distance)
{
    filament::math::float2 colVec = { b.mCenter.x - a.mCenter.x, b.mCenter.y - a.mCenter.y };
    filament::math::float2 colDir = { colVec.x / distance, colVec.y / distance };
    filament::math::float2 relativeVelocity = { a.mVelocity.x - b.mVelocity.x, a.mVelocity.y - b.mVelocity.y };
    
    float dotProduct = relativeVelocity.x * colDir.x + relativeVelocity.y * colDir.y;
    if (dotProduct > 0) return;

    float j = -(1 + 0.8f) * dotProduct;
    j /= (1 / a.mMass + 1 / b.mMass);

    filament::math::float2 impulse = { j * colDir.x, j * colDir.y };

    float maxSpeed = 5.0f;

    a.mVelocity.x = filament::math::clamp(a.mVelocity.x - ( 1 / a.mMass * impulse.x ), -maxSpeed, maxSpeed);
    a.mVelocity.y = filament::math::clamp(a.mVelocity.y - ( 1 / a.mMass * impulse.y ), -maxSpeed, maxSpeed);

    b.mVelocity.x = filament::math::clamp(b.mVelocity.x + ( 1 / b.mMass * impulse.x ), -maxSpeed, maxSpeed);
    b.mVelocity.y = filament::math::clamp(b.mVelocity.y + ( 1 / b.mMass * impulse.y ), -maxSpeed, maxSpeed);
}

Circle CreateCircle(App& sApp)
{
    Circle sCircle;
    bool   isCollision;

    do {
        isCollision = false;
        sCircle.mRadius   = randf(1.5f, 3.5f);
        sCircle.mMass     = sCircle.mRadius * sCircle.mRadius;
        sCircle.mColor    = (uint32_t)(randf() * (rand() % 0xFFFFFFFF));
        sCircle.mCenter   = { 
            randf(-10.0f, 10.0f), 
            randf(-10.0f, 10.0f) 
        };

        for (int i = 0; i < sApp.mCurrentCount; ++i)
        {
            if (dist(sCircle.mCenter, sApp.mRenderableArr[i].mCircle.mCenter) < (sCircle.mRadius + sApp.mRenderableArr[i].mCircle.mRadius))
            {
                isCollision = true;
                break;
            }
        }
    } while (isCollision);

    sCircle.mVelocity = { 
        randf(-1.5f, 1.5f) * sCircle.mMass,
        randf(-1.5f, 1.5f) * sCircle.mMass
    };

    sCircle.mVertices[0].mPos.x = sCircle.mCenter.x;
    sCircle.mVertices[0].mPos.y = sCircle.mCenter.y;
    sCircle.mVertices[0].mColor = sCircle.mColor;

    for (int i = 0; i < gSegments; ++i) 
    {
        float theta = (2.0f * M_PI * float(i)) / float(gSegments);
        sCircle.mVertices[i + 1].mPos.x = sCircle.mCenter.x + sCircle.mRadius * cosf(theta);
        sCircle.mVertices[i + 1].mPos.y = sCircle.mCenter.y + sCircle.mRadius * sinf(theta);
        sCircle.mVertices[i + 1].mColor = sCircle.mColor;

        sCircle.mIndicies[i * 3]     = 0;
        sCircle.mIndicies[i * 3 + 1] = i + 1;
        sCircle.mIndicies[i * 3 + 2] = (i + 1) % gSegments + 1;
    }

    sApp.mCurrentCount++;
    
    return sCircle;
}
 
void Init(App& aApp)
{
    aApp.mCurrentCount = 0;
    for (int i = 0; i < gCircleCount; ++i)
    {   
        aApp.mRenderableArr[i].mCircle = CreateCircle(aApp);
    }
}

static void printUsage(char* name) {
    std::string exec_name(utils::Path(name).getName());
    std::string usage(
            "HELLOTRIANGLE renders a spinning colored triangle\n"
            "Usage:\n"
            "    HELLOTRIANGLE [options]\n"
            "Options:\n"
            "   --help, -h\n"
            "       Prints this message\n\n"
            "   --api, -a\n"
            "       Specify the backend API: opengl, vulkan, or metal\n"
    );
    const std::string from("HELLOTRIANGLE");
    for (size_t pos = usage.find(from); pos != std::string::npos; pos = usage.find(from, pos)) {
        usage.replace(pos, from.length(), exec_name);
    }
    std::cout << usage;
}

static int handleCommandLineArguments(int argc, char* argv[], App* app) {
    static constexpr const char* OPTSTR = "ha:";
    static const struct option OPTIONS[] = {
            { "help", no_argument,       nullptr, 'h' },
            { "api",  required_argument, nullptr, 'a' },
            { nullptr, 0,                nullptr, 0 }
    };
    int opt;
    int option_index = 0;
    while ((opt = getopt_long(argc, argv, OPTSTR, OPTIONS, &option_index)) >= 0) {
        std::string arg(optarg ? optarg : "");
        switch (opt) {
            default:
            case 'h':
                printUsage(argv[0]);
                exit(0);
            case 'a':
                if (arg == "opengl") {
                    app->mConfig.backend = Engine::Backend::OPENGL;
                } else if (arg == "vulkan") {
                    app->mConfig.backend = Engine::Backend::VULKAN;
                } else if (arg == "metal") {
                    app->mConfig.backend = Engine::Backend::METAL;
                } else {
                    std::cerr << "Unrecognized backend. Must be 'opengl'|'vulkan'|'metal'.\n";
                    exit(1);
                }
                break;
        }
    }
    return optind;
}

int main(int argc, char** argv) {
    App sApp{};
    sApp.mConfig.title = "movingcircles";
    sApp.mConfig.backend = Engine::Backend::OPENGL;
    handleCommandLineArguments(argc, argv, &sApp);

    srand(time(NULL));

    Init(sApp);
    
    auto setup = [&sApp](Engine* engine, View* view, Scene* scene) {
        sApp.mSkybox = Skybox::Builder().color({0.1, 0.125, 0.25, 1.0}).build(*engine);
        scene->setSkybox(sApp.mSkybox);
        
        for (int i = 0; i < gCircleCount; ++i)
        {
            sApp.mRenderableArr[i].mVertexBuffer = VertexBuffer::Builder()
                                                  .vertexCount(gSegments + 1)
                                                  .bufferCount(1)
                                                  .attribute(VertexAttribute::POSITION, 0, VertexBuffer::AttributeType::FLOAT2, 0, 12)
                                                  .attribute(VertexAttribute::COLOR, 0, VertexBuffer::AttributeType::UBYTE4, 8, 12)
                                                  .normalized(VertexAttribute::COLOR)
                                                  .build(*engine);

            sApp.mRenderableArr[i].mVertexBuffer->setBufferAt(*engine, 0, VertexBuffer::BufferDescriptor(
                                                              sApp.mRenderableArr[i].mCircle.mVertices,
                                                              sizeof(Vertex) * (gSegments + 1), nullptr));

            sApp.mRenderableArr[i].mIndexBuffer = IndexBuffer::Builder()
                                                 .indexCount(gSegments * 3)
                                                 .bufferType(IndexBuffer::IndexType::USHORT)
                                                 .build(*engine);

            sApp.mRenderableArr[i].mIndexBuffer->setBuffer(*engine, IndexBuffer::BufferDescriptor(
                                                           sApp.mRenderableArr[i].mCircle.mIndicies,
                                                           sizeof(uint16_t) * gSegments * 3, nullptr));

            sApp.mRenderableArr[i].mMaterial = Material::Builder()
                                              .package(RESOURCES_BAKEDCOLOR_DATA, RESOURCES_BAKEDCOLOR_SIZE)
                                              .build(*engine);

            sApp.mRenderableArr[i].mRenderable = EntityManager::get().create();

            RenderableManager::Builder(1).boundingBox({{ -1, -1, -1 }, { 1, 1, 1 }})
                                         .material(0, sApp.mRenderableArr[i].mMaterial->getDefaultInstance())
                                         .geometry(0, RenderableManager::PrimitiveType::TRIANGLES, sApp.mRenderableArr[i].mVertexBuffer, sApp.mRenderableArr[i].mIndexBuffer, 0, gSegments * 3)
                                         .culling(false)
                                         .receiveShadows(false)
                                         .castShadows(false)
                                         .build(*engine, sApp.mRenderableArr[i].mRenderable);

            scene->addEntity(sApp.mRenderableArr[i].mRenderable);
        }

        sApp.mCameraObj = utils::EntityManager::get().create();
        sApp.mCamera = engine->createCamera(sApp.mCameraObj);
        view->setCamera(sApp.mCamera);
    };

    auto cleanup = [&sApp](Engine* engine, View*, Scene*) {
        engine->destroy(sApp.mSkybox);
        
        for (int i = 0; i < gCircleCount; ++i)
        {
            engine->destroy(sApp.mRenderableArr[i].mRenderable);
            engine->destroy(sApp.mRenderableArr[i].mMaterial);
            engine->destroy(sApp.mRenderableArr[i].mVertexBuffer);
            engine->destroy(sApp.mRenderableArr[i].mIndexBuffer);
        }
        
        engine->destroyCameraComponent(sApp.mCameraObj);
        utils::EntityManager::get().destroy(sApp.mCameraObj);
    };

    FilamentApp::get().animate([&sApp](Engine* engine, View* view, double now) {
        constexpr float ZOOM = 20.0f;
        const uint32_t w = view->getViewport().width;
        const uint32_t h = view->getViewport().height;
        const float aspect = (float) w / h;
        sApp.mCamera->setProjection(Camera::Projection::ORTHO,
            -aspect * ZOOM, aspect * ZOOM,
            -ZOOM, ZOOM, 0, 1);

        auto& tcm = engine->getTransformManager();
        for (int i = 0; i < gCircleCount; ++i)
        {
            sApp.mRenderableArr[i].mCircle.mCenter.x += sApp.mRenderableArr[i].mCircle.mVelocity.x / gSpeed;
            sApp.mRenderableArr[i].mCircle.mCenter.y += sApp.mRenderableArr[i].mCircle.mVelocity.y / gSpeed;

            tcm.setTransform(tcm.getInstance(sApp.mRenderableArr[i].mRenderable), 
                             filament::math::mat4f::translation(filament::math::float3(
                                sApp.mRenderableArr[i].mCircle.mCenter.x,
                                sApp.mRenderableArr[i].mCircle.mCenter.y, 0)));
        }

        for (int i = 0; i < gCircleCount; ++i)
        {
            for (int j = i + 1; j < gCircleCount; ++j)
            {
                float distance = dist(sApp.mRenderableArr[i].mCircle.mCenter,
                                      sApp.mRenderableArr[j].mCircle.mCenter);
                if (distance <= sApp.mRenderableArr[i].mCircle.mRadius + sApp.mRenderableArr[j].mCircle.mRadius)
                {
                    Collision(sApp.mRenderableArr[i].mCircle, sApp.mRenderableArr[j].mCircle, distance);
                }
            }
        }
    });

    FilamentApp::get().run(sApp.mConfig, setup, cleanup);

    return 0;
}
