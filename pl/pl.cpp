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

#include <cmath>

#include "generated/resources/resources.h"

using namespace filament;
using utils::Entity;
using utils::EntityManager;

static constexpr int gLatitude  = 100;
static constexpr int gLongitude = 100;

struct Vertex;
struct Sphere;

struct App {
    VertexBuffer* vb;
    IndexBuffer* ib;
    Material* mat;
    Skybox* skybox;
    Sphere* sphere;
    Entity renderable;
    filament::math::mat4f transform;
};

struct Vertex {
    filament::math::float3 mPos;
    uint32_t color;
};

struct Sphere {
    std::vector<Vertex>     mVertices;
    std::vector<uint16_t>   mIndicies;
    float                   mRadius;
};

float randf() { return (float)rand() / (float)RAND_MAX; }

float randf(float a, float b) { return a + randf() * (b - a); }

void genVertices(Sphere* aSphere)
{
    uint32_t color = 0xFF000000;
    for (int i = 0; i <= gLatitude; ++i) 
    {
        float theta = i * (M_PI / (gLatitude - 1));
        color += 0x02;
        for (int j = 0; j <= gLongitude; ++j)
        {
            float phi = j * (2 * M_PI / gLongitude);

            Vertex vertex = { 
                { 
                    aSphere->mRadius * sin(theta) * cos(phi),
                    aSphere->mRadius * sin(theta) * sin(phi),
                    aSphere->mRadius * cos(theta),
                },
                color
            };

            aSphere->mVertices.push_back(vertex);
        }
    }
}

void genIndicies(Sphere* aSphere)
{
    for (int i = 0; i < gLatitude; ++i) 
    {
        for (int j = 0; j < gLongitude; ++j)
        {
            int idx0 = i * (gLongitude + 1) + j;
            int idx1 = i * (gLongitude + 1) + j + 1;
            int idx2 = (i + 1) * (gLongitude + 1) + j;
            int idx3 = (i + 1) * (gLongitude + 1) + j + 1;

            aSphere->mIndicies.push_back(idx0);
            aSphere->mIndicies.push_back(idx1);
            aSphere->mIndicies.push_back(idx2);

            aSphere->mIndicies.push_back(idx2);
            aSphere->mIndicies.push_back(idx1);
            aSphere->mIndicies.push_back(idx3);
        }
    }
}

int main(int argc, char** argv) {
    Config config;
    config.title = "planet";
    config.backend = Engine::Backend::OPENGL;

    App app;
    auto setup = [&app](Engine* engine, View* view, Scene* scene) {
        app.skybox = Skybox::Builder().color({0.1, 0.125, 0.25, 1.0}).build(*engine);
        scene->setSkybox(app.skybox);
        view->setPostProcessingEnabled(false);

        app.sphere = new Sphere();
        app.sphere->mRadius = 0.5f;
        app.sphere->mVertices.reserve((gLatitude + 1) * (gLongitude + 1));
        app.sphere->mIndicies.reserve(gLatitude * gLongitude * 6);
        genVertices(app.sphere);
        genIndicies(app.sphere);

        printf("%lu\n", app.sphere->mVertices.size());
        printf("%lu\n", app.sphere->mIndicies.size());

        app.vb = VertexBuffer::Builder()
                .vertexCount(app.sphere->mVertices.size()).bufferCount(1)
                .attribute(VertexAttribute::POSITION, 0, VertexBuffer::AttributeType::FLOAT3, 0, 16)
                .attribute(VertexAttribute::COLOR, 0, VertexBuffer::AttributeType::UBYTE4, 12, 16)
                .normalized(VertexAttribute::COLOR).build(*engine);

        app.vb->setBufferAt(*engine, 0,
                VertexBuffer::BufferDescriptor(app.sphere->mVertices.data(), sizeof(Vertex) * app.sphere->mVertices.size(), nullptr));

        app.ib = IndexBuffer::Builder().indexCount(app.sphere->mIndicies.size())
                .bufferType(IndexBuffer::IndexType::USHORT).build(*engine);

        app.ib->setBuffer(*engine,
                IndexBuffer::BufferDescriptor(app.sphere->mIndicies.data(), sizeof(uint16_t) * app.sphere->mIndicies.size(), nullptr));

        app.mat = Material::Builder()
                .package(RESOURCES_BAKEDCOLOR_DATA, RESOURCES_BAKEDCOLOR_SIZE).build(*engine);

        app.renderable = EntityManager::get().create();
        RenderableManager::Builder(1)
                        .material(0, app.mat->getDefaultInstance())
                        .geometry(0, RenderableManager::PrimitiveType::TRIANGLES, app.vb, app.ib, 0, app.sphere->mIndicies.size())
                        .culling(false)
                        .receiveShadows(false)
                        .castShadows(false)
                        .build(*engine, app.renderable);
        
        auto& tcm = engine->getTransformManager();
        auto ti = tcm.getInstance(app.renderable);
        app.transform = filament::math::mat4f { filament::math::mat3f(1), filament::math::float3(0, 0, -4) } * tcm.getWorldTransform(ti);
        scene->addEntity(app.renderable);
        tcm.setTransform(ti, app.transform);
    };

    auto cleanup = [&app](Engine* engine, View*, Scene*) {
        engine->destroy(app.skybox);
        engine->destroy(app.renderable);
        engine->destroy(app.mat);
        engine->destroy(app.vb);
        engine->destroy(app.ib);

        delete app.sphere;
    };

    FilamentApp::get().animate([&app](Engine* engine, View* view, double now) {
        auto& tcm = engine->getTransformManager();
        auto ti = tcm.getInstance(app.renderable);
        tcm.setTransform(ti, filament::math::mat4f::rotation(now, filament::math::float3{ 1, 1, 0 }));
    });

    FilamentApp::get().run(config, setup, cleanup);

    return 0;
}
