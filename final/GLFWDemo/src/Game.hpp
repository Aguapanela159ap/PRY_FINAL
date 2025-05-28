/*
 *  Copyright 2019-2022 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  In no event and under no legal theory, whether in tort (including negligence),
 *  contract, or otherwise, unless required by applicable law (such as deliberate
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental,
 *  or consequential damages of any character arising as a result of this License or
 *  out of the use or inability to use the software (including but not limited to damages
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and
 *  all other commercial damages or losses), even if such Contributor has been advised
 *  of the possibility of such damages.
 */

#pragma once

#include "GLFWDemo.hpp"

namespace Diligent
{

// Clase principal del juego que hereda de GLFWDemo para manejar la lógica del juego.
class Game final : public GLFWDemo
{
public:
    virtual bool Initialize() override;
    virtual void Update(float dt) override;
    virtual void Draw() override;
    virtual void KeyEvent(Key key, KeyState state) override;
    virtual void MouseEvent(float2 pos) override;

private:
    void GenerateMap();
    void CreateSDFMap();
    void CreatePipelineState();
    void InitPlayer();
    void BindResources();
    void LoadNewMap();

    void GetScreenTransform(float2& XRange, float2& YRange);

private:
    // Tipo: Estructura que define las propiedades y estados del jugador.
    struct
    {
        float2 Pos;
        float2 FlashLightDir   = {5.0f, 5.0f};
        float  FlashLightPower = 0.0f;

        float2 PendingPos;
        float2 MousePos;
        bool   LMBPressed = false;

        float3 AmbientLightColor   = {1.2f, 1.2f, 1.2f};
        float  AmbientLightRadius  = 0.0f;
        float  VertVel             = 0.0f;
        bool   JumpRequested       = false;
        Uint32 JumpCount           = 0;
        float  JumpInputBufferTime = 0.0f;

        RefCntAutoPtr<IBuffer> pConstants;
    } m_Player;

    // Tipo: Estructura que define las propiedades y datos del mapa del juego.
    struct
    {
        float2                                TeleportPos;
        float                                 TeleportWaveAnim       = 0.0f;
        float3                                TeleportLightColor     = {0.0f, 1.0f, 0.0f};
        float                                 TeleportLightIntensity = 1.0f;
        float2                                SpikesPos;
        float                                 SpikesWaveAnim       = 0.0f;
        float3                                SpikesLightColor     = {1.0f, 0.0f, 0.0f};
        float                                 SpikesLightIntensity = 0.5f;
        std::vector<bool>                     MapData;
        RefCntAutoPtr<ITexture>               pMapTex;
        RefCntAutoPtr<IPipelineState>         pPSO;
        RefCntAutoPtr<IShaderResourceBinding> pSRB;
        RefCntAutoPtr<IBuffer>                pConstants;
    } m_Map;

    // Tipo: Estructura que contiene constantes del juego para física, renderizado y dimensiones.
    struct
    {
        const float PlayerRadius          = 0.4f;
        const float AmbientLightRadius    = 20.0f;
        const float FlshLightMaxDist      = 50.0f;
        const float PlayerVelocity        = 30.0f;
        const float FlashLightAttenuation = 20.0f;
        const float MaxDT                 = 1.0f / 30.0f;
        const uint  MaxCollisionSteps     = 8;
        const float GravityAccel          = 90.0f;
        const float JumpSpeed             = 30.0f;
        const float TerminalFallSpeed     = 60.0f;

        const float TeleportRadius = 1.0f;
        const float SpikesRadius   = 1.0f;

        const uint2  MapTexDim       = {64, 64};
        const Uint32 SDFTexScale     = 2;
        const uint2  SDFTexDim       = MapTexDim * SDFTexScale;
        const int    TexFilterRadius = 8;
    } Constants;

    RefCntAutoPtr<IShaderSourceInputStreamFactory> m_pShaderSourceFactory;
    RefCntAutoPtr<IRenderStateNotationLoader>      m_pRSNLoader;
};

} // namespace Diligent