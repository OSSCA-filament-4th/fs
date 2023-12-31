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

struct Vertex {
    filament::math::float2 mPos;
    uint32_t               mColor;
};

const int   gCircleCount = 10;
const int   gSegments    = 100;
const float gSpeed       = 20.0f;
Vertex      gVertices[gSegments + 1];
uint16_t    gIndicies[gSegments * 3];
double      gLastTime    = 0.0;

struct Circle {
    uint16_t               mUID;
    filament::math::float2 mCenter;
    filament::math::float2 mVelocity;
    float                  mMass;
    float                  mRadius;
};

struct Wall {
    float up, down, left, right;
};

struct Renderable_t
{
    Material*     mMaterial;
    Circle        mCircle;
    Entity        mRenderable;
};

struct App {
    Config        mConfig;
    Camera*       mCamera;
    Skybox*       mSkybox;
    Entity        mCameraObj;
    VertexBuffer* mVertexBuffer;
    IndexBuffer*  mIndexBuffer;
    
    Renderable_t  mRenderableArr[gCircleCount];
    uint32_t      mCurrentCount;
    Wall          mWall;
};

float randf() { return (float)rand() / (float)RAND_MAX; }

float randf(float a, float b) { return a + randf() * (b - a); }

float dist(filament::math::float2 left, filament::math::float2 right) 
{
    return sqrt((right.x - left.x) * (right.x - left.x) + (right.y - left.y) * (right.y - left.y));
}

void collisionUpdate(Circle& a, Circle& b)
{
	filament::math::float2 temp = a.mVelocity;
	a.mVelocity = a.mVelocity - 2 * (b.mMass / (a.mMass + b.mMass)) * (dot((a.mVelocity - b.mVelocity), (a.mCenter - b.mCenter)) / length2(a.mCenter - b.mCenter)) * (a.mCenter - b.mCenter);
	b.mVelocity = b.mVelocity - 2 * (a.mMass / (a.mMass + b.mMass)) * (dot((b.mVelocity - temp), (b.mCenter - a.mCenter)) / length2(b.mCenter - a.mCenter)) * (b.mCenter - a.mCenter);
}

Circle CreateCircle(App& sApp)
{
    Circle sCircle;
    bool   isCollision;

    sCircle.mRadius   = randf(2.5f, 3.5f);
    sCircle.mVelocity = { 
        randf(-5.0f, 5.0f),
        randf(-5.0f, 5.0f)
    };
    sCircle.mMass     = sCircle.mRadius * sCircle.mRadius;

    do {
        isCollision     = false;
        sCircle.mCenter = { 
            randf(-18.0f, 18.0f), 
            randf(-18.0f, 18.0f) 
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

    sCircle.mUID = sApp.mCurrentCount;
    sApp.mCurrentCount++;
    
    return sCircle;
}
 
void initBuffers()
{
    gVertices[0].mPos   = { 0, 0 };
    gVertices[0].mColor = (uint32_t)(randf() * (rand() % 0xFFFFFFFF));

    for (int i = 0; i < gSegments; ++i)
    {
        float theta = (2.0f * M_PI * float(i)) / float(gSegments);
        gVertices[i + 1].mPos.x = cosf(theta);
        gVertices[i + 1].mPos.y = sinf(theta);
        gVertices[i + 1].mColor = 0xFFFF0000;

        gIndicies[i * 3] = 0;
        gIndicies[i * 3 + 1] = i + 1;
        gIndicies[i * 3 + 2] = (i + 1) % gSegments + 1;
    }
}


void Move(Circle& target, double deltaTime)
{
    for (int i = 0; i < gCircleCount; ++i)
    {
        target.mCenter.x += (target.mVelocity.x * deltaTime);
        target.mCenter.y += (target.mVelocity.y * deltaTime);
    }
}

void WallCollision(filament::math::float4 wall, Circle& target)
{
    // left
    if (abs(wall.x - target.mCenter.x) <= target.mRadius || target.mCenter.x < wall.x)
    {
        target.mCenter.x += target.mRadius - target.mCenter.x + wall.x;
        target.mVelocity.x = -target.mVelocity.x;
    }
    // right
    if (abs(wall.y - target.mCenter.x) <= target.mRadius || target.mCenter.x > wall.y)
    {
        target.mCenter.x -= target.mRadius + target.mCenter.x - wall.y;
        target.mVelocity.x = -target.mVelocity.x;
    }
    // bottom
    if (abs(wall.z - target.mCenter.y) <= target.mRadius || target.mCenter.y < wall.z)
    {
        target.mCenter.y += target.mRadius - target.mCenter.y + wall.z;
        target.mVelocity.y = -target.mVelocity.y;
    }
    // up
    if (abs(wall.w - target.mCenter.y) <= target.mRadius || target.mCenter.y > wall.w)
    {
        target.mCenter.y -= target.mRadius + target.mCenter.y - wall.w;
        target.mVelocity.y = -target.mVelocity.y;
    }
}

void CircleCollision(Circle& target, App& app)
{
    for (int j = 0; j < gCircleCount; ++j)
    {
        Circle& other = app.mRenderableArr[j].mCircle;
        if (target.mUID == other.mUID) continue;
        
        float distance = dist(target.mCenter, other.mCenter);
        float diameter = target.mRadius + other.mRadius;
        float diff = diameter - distance;
        if (distance <= diameter)
        {
            filament::math::float2 firstImpulse = {
                diff / distance * (target.mCenter.x - other.mCenter.x),
                diff / distance * (target.mCenter.y - other.mCenter.y),
            };
            target.mCenter.x += (firstImpulse.x / 2);
            target.mCenter.y += (firstImpulse.y / 2);

            other.mCenter.x -= (firstImpulse.x / 2);
            other.mCenter.y -= (firstImpulse.y / 2);

            collisionUpdate(target, other);
        }
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
    
    auto setup = [&sApp](Engine* engine, View* view, Scene* scene) {
        sApp.mSkybox = Skybox::Builder().color({0.1, 0.125, 0.25, 1.0}).build(*engine);
        scene->setSkybox(sApp.mSkybox);
        auto& tcm = engine->getTransformManager();

        initBuffers();

        sApp.mVertexBuffer = VertexBuffer::Builder()
                                 .vertexCount(gSegments + 1)
                                 .bufferCount(1)
                                 .attribute(VertexAttribute::POSITION, 0, VertexBuffer::AttributeType::FLOAT2, 0, 12)
                                 .attribute(VertexAttribute::COLOR, 0, VertexBuffer::AttributeType::UBYTE4, 8, 12)
                                 .build(*engine);

        sApp.mVertexBuffer->setBufferAt(*engine, 0, VertexBuffer::BufferDescriptor(gVertices, sizeof(Vertex) * (gSegments + 1), nullptr));

        sApp.mIndexBuffer = IndexBuffer::Builder()
                                .indexCount(gSegments * 3)
                                .bufferType(IndexBuffer::IndexType::USHORT)
                                .build(*engine);

        sApp.mIndexBuffer->setBuffer(*engine, IndexBuffer::BufferDescriptor(gIndicies, sizeof(uint16_t) * gSegments * 3, nullptr));

        for (int i = 0; i < gCircleCount; ++i)
        {
            Circle sCircle = CreateCircle(sApp);
            sApp.mRenderableArr[i].mCircle = sCircle;

            sApp.mRenderableArr[i].mMaterial = Material::Builder()
                                              .package(RESOURCES_BAKEDCOLOR_DATA, RESOURCES_BAKEDCOLOR_SIZE)
                                              .build(*engine);

            sApp.mRenderableArr[i].mRenderable = EntityManager::get().create();

            RenderableManager::Builder(1).material(0, sApp.mRenderableArr[i].mMaterial->getDefaultInstance())
                                         .geometry(0, RenderableManager::PrimitiveType::TRIANGLE, sApp.mVertexBuffer, sApp.mIndexBuffer, 0, gSegments * 3)
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
        }

        engine->destroy(sApp.mVertexBuffer);
        engine->destroy(sApp.mIndexBuffer);
        engine->destroyCameraComponent(sApp.mCameraObj);
        utils::EntityManager::get().destroy(sApp.mCameraObj);
    };

    FilamentApp::get().animate([&sApp](Engine* engine, View* view, double now) {
        auto& tcm = engine->getTransformManager();
        double deltaTime = now - gLastTime;
        gLastTime = now;

        constexpr float ZOOM = 30.0f;
        const uint32_t w = view->getViewport().width;
        const uint32_t h = view->getViewport().height;
        const float aspect = (float) w / h;

        // left, right, bottom, top
        filament::math::float4 sWall = { -aspect * ZOOM, aspect * ZOOM, -ZOOM, ZOOM };
        sApp.mCamera->setProjection(Camera::Projection::ORTHO, sWall.x, sWall.y, sWall.z, sWall.w, 0, 1);

        for (int i = 0; i < gCircleCount; ++i) Move(sApp.mRenderableArr[i].mCircle, deltaTime);
        for (int i = 0; i < gCircleCount; ++i) CircleCollision(sApp.mRenderableArr[i].mCircle, sApp);
        for (int i = 0; i < gCircleCount; ++i) WallCollision(sWall, sApp.mRenderableArr[i].mCircle);
        for (int i = 0; i < gCircleCount; ++i)
        {
            Circle& target = sApp.mRenderableArr[i].mCircle;
            filament::math::mat4f s = filament::math::mat4f::scaling(target.mRadius);
            filament::math::mat4f t = filament::math::mat4f::translation(filament::math::float3(target.mCenter, 0));
            filament::math::mat4f r = t * s;

            tcm.setTransform(tcm.getInstance(sApp.mRenderableArr[i].mRenderable), r);
        }
    });

    FilamentApp::get().run(sApp.mConfig, setup, cleanup);

    return 0;
}
