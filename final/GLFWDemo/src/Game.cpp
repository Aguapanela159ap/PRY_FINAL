/*
 *  Copyright 2019-2024 Diligent Graphics LLC
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

#include <random>
#include <vector>

#include "Game.hpp"
#include "ShaderMacroHelper.hpp"
#include "CallbackWrapper.hpp"

namespace Diligent
{

namespace
{

#include "../assets/Structures.fxh"

static_assert(sizeof(MapConstants) % 16 == 0, "debe estar alineado a 16 bytes");
static_assert(sizeof(PlayerConstants) % 16 == 0, "debe estar alineado a 16 bytes");

} // namespace

inline float fract(float x)
{
    return x - floor(x);
}

GLFWDemo* CreateGLFWApp()
{
    return new Game{};
}

// Inicializa el juego configurando el motor de renderizado, shaders, mapa y jugador.
bool Game::Initialize()
{
    try
    {
        GetEngineFactory()->CreateDefaultShaderSourceStreamFactory(nullptr, &m_pShaderSourceFactory);
        CHECK_THROW(m_pShaderSourceFactory);

        RefCntAutoPtr<IRenderStateNotationParser> pRSNParser;
        {
            CreateRenderStateNotationParser({}, &pRSNParser);
            CHECK_THROW(pRSNParser);
            pRSNParser->ParseFile("RenderStates.json", m_pShaderSourceFactory);
        }
        {
            RenderStateNotationLoaderCreateInfo RSNLoaderCI{};
            RSNLoaderCI.pDevice        = GetDevice();
            RSNLoaderCI.pStreamFactory = m_pShaderSourceFactory;
            RSNLoaderCI.pParser        = pRSNParser;
            CreateRenderStateNotationLoader(RSNLoaderCI, &m_pRSNLoader);
            CHECK_THROW(m_pRSNLoader);
        }

        GenerateMap();
        CreateSDFMap();
        CreatePipelineState();
        InitPlayer();
        BindResources();

        return true;
    }
    catch (...)
    {
        return false;
    }
}

// Actualiza el estado del juego en cada fotograma, manejando el movimiento y la física del jugador.
void Game::Update(float dt)
{
    dt = std::min(dt, Constants.MaxDT);

    // Tipo: Inicio del manejo del movimiento horizontal
    {
        float HorizontalDir   = m_Player.PendingPos.x;
        m_Player.PendingPos.x = 0.0f;

        if (std::abs(HorizontalDir) > 0.1f)
        {
            const float2 StartPos = m_Player.Pos;
            const float2 EndPos   = m_Player.Pos + float2{HorizontalDir, 0.0f} * dt * Constants.PlayerVelocity;

            const uint2 TexDim = Constants.MapTexDim;

            auto ReadMap = [&](int x, int y) -> float {
                if (x >= 0 && x < static_cast<int>(TexDim.x) && y >= 0 && y < static_cast<int>(TexDim.y))
                    return m_Map.MapData[x + y * TexDim.x] ? 1.0f : 0.0f;
                return 1.0f;
            };

            for (Uint32 i = 0; i < Constants.MaxCollisionSteps; ++i)
            {
                float  t        = static_cast<float>(i) / (Constants.MaxCollisionSteps - 1);
                float2 Pos      = lerp(StartPos, EndPos, t);
                float2 FetchPos = Pos - float2(0.5f, 0.5f);

                int   x    = static_cast<int>(FetchPos.x);
                int   y    = static_cast<int>(FetchPos.y);
                float c00  = ReadMap(x, y);
                float c10  = ReadMap(x + 1, y);
                float c01  = ReadMap(x, y + 1);
                float c11  = ReadMap(x + 1, y + 1);
                float dist = lerp(lerp(c00, c10, fract(FetchPos.x)), lerp(c01, c11, fract(FetchPos.x)), fract(FetchPos.y));

                if (dist > Constants.PlayerRadius)
                    break;

                m_Player.Pos = Pos;
            }

            if (length(m_Map.TeleportPos - m_Player.Pos) < Constants.TeleportRadius)
                LoadNewMap();
        }
    }
    m_Player.PendingPos = {};

    // Tipo: Inicio del manejo del movimiento vertical y la física
    {
        auto IsGroundBelow = [&](float2 pos) -> bool {
            const uint2 TexDim = Constants.MapTexDim;
            int         x      = static_cast<int>(pos.x);
            int         y      = static_cast<int>(pos.y - Constants.PlayerRadius);
            if (x < 0 || x >= static_cast<int>(TexDim.x) || y < 0 || y >= static_cast<int>(TexDim.y))
                return true;
            bool ground = m_Map.MapData[x + y * TexDim.x];
            std::cout << "Verificando suelo en x=" << x << ", y=" << y << ", Suelo: " << ground << std::endl;
            return ground;
        };

        bool OnGround = IsGroundBelow(m_Player.Pos);
        std::cout << "OnGround: " << OnGround << ", Posición Jugador: (" << m_Player.Pos.x << ", " << m_Player.Pos.y << "), VertVel: " << m_Player.VertVel << std::endl;

        if (m_Player.JumpInputBufferTime > 0.0f)
        {
            m_Player.JumpInputBufferTime -= dt;
            if (m_Player.JumpInputBufferTime <= 0.0f)
            {
                m_Player.JumpRequested = false;
                std::cout << "Tiempo de salto expiró, JumpRequested limpiado" << std::endl;
            }
        }

        if (OnGround)
        {
            m_Player.JumpCount = 0;
            m_Player.VertVel   = 0.0f;
            if (m_Player.JumpRequested)
                std::cout << "En suelo, JumpCount restablecido a 0, JumpRequested: " << m_Player.JumpRequested << std::endl;
        }
        else
        {
            m_Player.VertVel -= Constants.GravityAccel * dt;
            m_Player.VertVel = std::max(m_Player.VertVel, -Constants.TerminalFallSpeed);
            std::cout << "Gravedad aplicada, VertVel: " << m_Player.VertVel << std::endl;
        }

        if (m_Player.JumpRequested && m_Player.JumpCount < 2)
        {
            std::cout << "¡Salto realizado! JumpCount: " << m_Player.JumpCount + 1 << ", VertVel: " << Constants.JumpSpeed << ", Pos: (" << m_Player.Pos.x << ", " << m_Player.Pos.y << ")" << std::endl;
            m_Player.VertVel = Constants.JumpSpeed;
            m_Player.JumpCount++;
            m_Player.JumpRequested       = false;
            m_Player.JumpInputBufferTime = 0.0f;
        }

        const float2 StartPos = m_Player.Pos;
        const float2 EndPos   = m_Player.Pos + float2{0.0f, m_Player.VertVel * dt};

        const uint2 TexDim = Constants.MapTexDim;

        auto ReadMap = [&](int x, int y) -> float {
            if (x >= 0 && x < static_cast<int>(TexDim.x) && y >= 0 && y < static_cast<int>(TexDim.y))
                return m_Map.MapData[x + y * TexDim.x] ? 1.0f : 0.0f;
            return 1.0f;
        };
        for (Uint32 i = 0; i < Constants.MaxCollisionSteps; ++i)
        {
            float  t        = static_cast<float>(i) / (Constants.MaxCollisionSteps - 1);
            float2 Pos      = lerp(StartPos, EndPos, t);
            float2 FetchPos = Pos - float2(0.5f, 0.5f);

            int   x    = static_cast<int>(FetchPos.x);
            int   y    = static_cast<int>(FetchPos.y);
            float c00  = ReadMap(x, y);
            float c10  = ReadMap(x + 1, y);
            float c01  = ReadMap(x, y + 1);
            float c11  = ReadMap(x + 1, y + 1);
            float dist = lerp(lerp(c00, c10, fract(FetchPos.x)), lerp(c01, c11, fract(FetchPos.x)), fract(FetchPos.y));

            if (dist > Constants.PlayerRadius)
                break;

            m_Player.Pos = Pos;
        }
    }
}

// Renderiza la escena del juego, incluyendo el mapa, jugador y efectos de iluminación.
void Game::Draw()
{
    auto* pContext   = GetContext();
    auto* pSwapchain = GetSwapChain();

    ITextureView* pRTV = pSwapchain->GetCurrentBackBufferRTV();
    pContext->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    const float ClearColor[4] = {};
    pContext->ClearRenderTarget(pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_VERIFY);

    {
        const auto&  SCDesc = pSwapchain->GetDesc();
        MapConstants Const;

        GetScreenTransform(Const.ScreenRectLR, Const.ScreenRectTB);

        Const.UVToMap            = Constants.MapTexDim.Recast<float>();
        Const.MapToUV            = float2(1.0f, 1.0f) / Const.UVToMap;
        Const.TeleportRadius     = Constants.TeleportRadius;
        Const.TeleportWaveRadius = Constants.TeleportRadius * m_Map.TeleportWaveAnim;
        Const.TeleportPos        = m_Map.TeleportPos;
        Const.SpikesRadius       = Constants.SpikesRadius;
        Const.SpikesWaveRadius   = Constants.SpikesRadius * m_Map.SpikesWaveAnim;
        Const.SpikesPos          = m_Map.SpikesPos;

        pContext->UpdateBuffer(m_Map.pConstants, 0, sizeof(Const), &Const, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }

    {
        PlayerConstants Const;
        Const.PlayerPos          = m_Player.Pos;
        Const.PlayerRadius       = Constants.PlayerRadius;
        Const.FlashLightDir      = m_Player.FlashLightDir;
        Const.FlashLightPower    = m_Player.FlashLightPower;
        Const.AmbientLightRadius = Constants.AmbientLightRadius;
        Const.FlshLightMaxDist   = Constants.FlshLightMaxDist * 100.5f;

        pContext->UpdateBuffer(m_Player.pConstants, 0, sizeof(Const), &Const, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }

    {
        pContext->SetPipelineState(m_Map.pPSO);
        pContext->CommitShaderResources(m_Map.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        DrawAttribs drawAttr{50, DRAW_FLAG_VERIFY_ALL};
        pContext->Draw(drawAttr);
    }

    pContext->Flush();
    pSwapchain->Present();
}

// Calcula la transformación de la pantalla basada en la relación de aspecto.
void Game::GetScreenTransform(float2& XRange, float2& YRange)
{
    const auto& SCDesc       = GetSwapChain()->GetDesc();
    const float ScreenAspect = static_cast<float>(SCDesc.Width) / SCDesc.Height;
    const float TexAspect    = static_cast<float>(Constants.MapTexDim.x) / Constants.MapTexDim.y;

    if (ScreenAspect > TexAspect)
    {
        XRange.y = (TexAspect / ScreenAspect);
        YRange.y = 1.f;
    }
    else
    {
        XRange.y = 1.f;
        YRange.y = (ScreenAspect / TexAspect);
    }
    XRange.x = -XRange.y;
    YRange.x = -YRange.y;
}

// Maneja la entrada del teclado para el movimiento y acciones del jugador.
void Game::KeyEvent(Key key, KeyState state)
{
    if (state == KeyState::Press || state == KeyState::Repeat)
    {
        switch (key)
        {
            case Key::Up:
            case Key::NP_Up:
                m_Player.PendingPos.y = 1.0f;
                break;

            case Key::S:
            case Key::Down:
            case Key::NP_Down:
                m_Player.PendingPos.y -= 1.0f;
                break;

            case Key::D:
            case Key::Right:
            case Key::NP_Right:
                m_Player.PendingPos.x += 1.0f;
                break;

            case Key::A:
            case Key::Left:
            case Key::NP_Left:
                m_Player.PendingPos.x -= 1.0f;
                break;

            case Key::Space:
                m_Player.PendingPos = m_Player.FlashLightDir;
                break;

            case Key::Esc:
                Quit();
                break;

            default:
                break;
        }
    }

    if (key == Key::W && state == KeyState::Press)
    {
        m_Player.JumpRequested       = true;
        m_Player.JumpInputBufferTime = 0.1f;
        std::cout << "W presionada, JumpRequested activado, BufferTime: " << m_Player.JumpInputBufferTime << std::endl;
    }

    if (key == Key::MB_Left)
        m_Player.LMBPressed = (state != KeyState::Release);

    if (state == KeyState::Release && key == Key::Tab)
        LoadNewMap();
}

// Actualiza la posición del ratón del jugador según la entrada.
void Game::MouseEvent(float2 pos)
{
    m_Player.MousePos = pos;
}

// Genera el mapa del juego con bordes, muros y desafíos para cada piso.
void Game::GenerateMap()
{
    const uint2 TexDim  = Constants.MapTexDim;
    auto&       MapData = m_Map.MapData;

    MapData.clear();
    MapData.resize(size_t{TexDim.x} * size_t{TexDim.y}, 0);

    for (Uint32 x = 0, x2 = TexDim.x * (TexDim.y - 1); x < TexDim.x; ++x, ++x2)
    {
        MapData[x]  = true;
        MapData[x2] = true;
    }

    for (size_t y = 0; y < TexDim.y; ++y)
    {
        MapData[y * TexDim.x]           = true;
        MapData[(y + 1) * TexDim.x - 1] = true;
    }

    // Tipo: Inicio de la generación de muros
    {
        const Uint32 NumFloors       = 4;
        const Uint32 HoleWidth       = 6;
        const Uint32 VerticalSpacing = TexDim.y / (NumFloors + 1);

        for (Uint32 i = 0; i < NumFloors; ++i)
        {
            if (i == 1 || i == 2) continue;

            Uint32 y         = TexDim.y - 1 - (i + 1) * VerticalSpacing;
            Uint32 HoleStart = (i & 1) ? 1 : TexDim.x - 1 - HoleWidth;

            for (Uint32 x = 1; x < TexDim.x - 1; ++x)
            {
                if (x >= HoleStart && x < HoleStart + HoleWidth)
                    continue;
                MapData[x + y * TexDim.x] = true;
            }
        }

        // Tipo: Desafío del segundo piso con muros verticales
        {
            Uint32       yFloor                = TexDim.y - 1 - 3 * VerticalSpacing;
            const Uint32 NumWalls              = 6;
            const Uint32 WallSpacing           = 8;
            const Uint32 WallWidth             = 3;
            const Uint32 WallHeights[NumWalls] = {3, 5, 7, 3, 5, 7};
            const Uint32 FloorHoleStart        = TexDim.x - 1 - 5;

            for (Uint32 x = 1; x < TexDim.x - 1; ++x)
            {
                if (x >= FloorHoleStart) continue;
                MapData[x + yFloor * TexDim.x] = true;
            }

            for (Uint32 i = 0; i < NumWalls; ++i)
            {
                Uint32 x      = 4 + i * WallSpacing;
                Uint32 height = WallHeights[i];

                for (Uint32 dx = 0; dx < WallWidth; ++dx)
                {
                    Uint32 currentX = x + dx;
                    if (currentX < 1 || currentX >= TexDim.x - 1) continue;

                    for (Uint32 dy = 0; dy < height; ++dy)
                    {
                        Uint32 y = yFloor + dy;
                        if (y >= TexDim.y - 1) continue;
                        MapData[currentX + y * TexDim.x] = true;
                    }
                }
            }
        }

        // Tipo: Desafío del tercer piso con muros verticales desplazados
        {
            Uint32       yFloor         = TexDim.y - 1 - 2 * VerticalSpacing;
            Uint32       yCeiling       = yFloor - 5;
            const Uint32 FloorHoleStart = 1;
            const Uint32 FloorHoleEnd   = FloorHoleStart + HoleWidth - 1;

            for (Uint32 x = 1; x < TexDim.x - 1; ++x)
            {
                if (x >= FloorHoleStart && x <= FloorHoleEnd) continue;
                MapData[x + yFloor * TexDim.x] = true;
            }

            const Uint32 WallWidth = 3;
            struct WallInfo
            {
                Uint32 xStart;
                Uint32 height;
                bool   isUpward;
            };
            const WallInfo Walls[] = {
                {10, 3, true},
                {16, 5, false},
                {22, 7, true},
                {28, 7, false},
                {34, 5, true},
                {40, 3, false},
                {46, 3, true},
                {52, 5, false},
                {58, 5, true}};
            const Uint32 NumWalls = sizeof(Walls) / sizeof(Walls[0]);

            for (Uint32 i = 0; i < NumWalls; ++i)
            {
                Uint32 x        = Walls[i].xStart;
                Uint32 height   = Walls[i].height;
                bool   isUpward = Walls[i].isUpward;

                for (Uint32 dx = 0; dx < WallWidth; ++dx)
                {
                    Uint32 currentX = x + dx;
                    if (currentX < 1 || currentX >= TexDim.x - 1) continue;

                    for (Uint32 dy = 0; dy < height; ++dy)
                    {
                        Uint32 y = isUpward ? yFloor + dy : yFloor - dy;
                        if (y < 1 || y >= TexDim.y - 1) continue;
                        MapData[currentX + y * TexDim.x] = true;
                    }
                }
            }
        }

        // Tipo: Desafío del primer piso con pilares al estilo Flappy Bird
        {
            Uint32 yWallTop   = TexDim.y - 1 - NumFloors * VerticalSpacing;
            Uint32 yCorridor0 = 1;
            Uint32 yCorridor1 = yWallTop - 1;

            const Uint32 HoleHeight    = 3;
            bool         HoleDownFirst = true;

            const Uint32 NumPillars    = 5;
            const Uint32 CorridorWidth = TexDim.x - 8;
            const Uint32 PillarSpacing = CorridorWidth / (NumPillars - 1);

            for (Uint32 i = 0; i < NumPillars; ++i)
            {
                Uint32 x = 4 + i * PillarSpacing;

                Uint32 gapStart = HoleDownFirst ? yCorridor0 + 1 : yCorridor1 - HoleHeight;
                HoleDownFirst   = !HoleDownFirst;

                for (Uint32 y = yCorridor0; y <= yCorridor1; ++y)
                {
                    if (y >= gapStart && y < gapStart + HoleHeight)
                        continue;
                    MapData[x + y * TexDim.x] = true;
                }
            }
        }

        {
            int2 Teleport                               = int2{int(TexDim.x) - 2, 2};
            MapData[Teleport.x + Teleport.y * TexDim.x] = false;
            m_Map.TeleportPos                           = Teleport.Recast<float>();
            m_Map.TeleportWaveAnim                      = 0.0f;
        }

        {
            Uint32 yFloor                           = TexDim.y - 1 - 2 * VerticalSpacing;
            int2   Spikes                           = int2{8, int(yFloor)};
            MapData[Spikes.x + Spikes.y * TexDim.x] = false;
            m_Map.SpikesPos                         = Spikes.Recast<float>();
            m_Map.SpikesWaveAnim                    = 0.0f;
        }
    }
}

// Crea una textura de campo de distancia firmado (SDF) a partir de los datos del mapa para renderizado.
void Game::CreateSDFMap()
{
    const uint2 SrcTexDim = Constants.MapTexDim;
    const uint2 DstTexDim = Constants.SDFTexDim;

    RefCntAutoPtr<ITexture> pSrcTex;
    {
        TextureDesc TexDesc;
        TexDesc.Name   = "SDF Map texture";
        TexDesc.Type   = RESOURCE_DIM_TEX_2D;
        TexDesc.Width  = DstTexDim.x;
        TexDesc.Height = DstTexDim.y;
        for (TEXTURE_FORMAT Format : {TEX_FORMAT_R16_FLOAT, TEX_FORMAT_R32_FLOAT})
        {
            if (GetDevice()->GetTextureFormatInfoExt(Format).BindFlags & BIND_UNORDERED_ACCESS)
            {
                TexDesc.Format = Format;
                break;
            }
        }
        CHECK_THROW(TexDesc.Format != TEX_FORMAT_UNKNOWN);

        TexDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS;

        m_Map.pMapTex = nullptr;
        GetDevice()->CreateTexture(TexDesc, nullptr, &m_Map.pMapTex);
        CHECK_THROW(m_Map.pMapTex != nullptr);

        TexDesc.Name      = "Src texture";
        TexDesc.Width     = SrcTexDim.x;
        TexDesc.Height    = SrcTexDim.y;
        TexDesc.Format    = TEX_FORMAT_R8_UNORM;
        TexDesc.BindFlags = BIND_SHADER_RESOURCE;

        GetDevice()->CreateTexture(TexDesc, nullptr, &pSrcTex);
        CHECK_THROW(pSrcTex != nullptr);
    }

    RefCntAutoPtr<IPipelineState>         pGenSdfPSO;
    RefCntAutoPtr<IShaderResourceBinding> pGenSdfSRB;
    {
        {
            ShaderMacroHelper Macros;
            Macros.AddShaderMacro("RADIUS", Constants.TexFilterRadius);
            Macros.AddShaderMacro("DIST_SCALE", 1.0f / float(Constants.SDFTexScale));
            Macros.AddShaderMacro("DST_TEXTURE_FORMAT_32F", m_Map.pMapTex->GetDesc().Format == TEX_FORMAT_R32_FLOAT);

            auto Callback = MakeCallback([&](ShaderCreateInfo& ShaderCI, SHADER_TYPE ShaderType, bool& IsAddToCache) {
                ShaderCI.Macros = Macros;
            });

            m_pRSNLoader->LoadPipelineState({"Generate SDF map PSO", PIPELINE_TYPE_COMPUTE, true, true, nullptr, nullptr, Callback, Callback}, &pGenSdfPSO);
            CHECK_THROW(pGenSdfPSO != nullptr);
        }

        pGenSdfPSO->CreateShaderResourceBinding(&pGenSdfSRB, true);
        CHECK_THROW(pGenSdfSRB != nullptr);

        pGenSdfSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_SrcTex")->Set(pSrcTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
        pGenSdfSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_DstTex")->Set(m_Map.pMapTex->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));
    }

    auto* pContext = GetContext();

    {
        std::vector<Uint8> MapData(m_Map.MapData.size());
        VERIFY_EXPR(MapData.size() == (size_t{SrcTexDim.x} * size_t{SrcTexDim.y}));

        for (size_t i = 0; i < MapData.size(); ++i)
            MapData[i] = m_Map.MapData[i] ? 0xFF : 0;

        TextureSubResData SubresData;
        SubresData.pData       = MapData.data();
        SubresData.Stride      = sizeof(MapData[0]) * SrcTexDim.x;
        SubresData.DepthStride = static_cast<Uint32>(MapData.size());

        pContext->UpdateTexture(pSrcTex, 0, 0, Box{0, SrcTexDim.x, 0, SrcTexDim.y}, SubresData, RESOURCE_STATE_TRANSITION_MODE_NONE, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }

    {
        const uint2 LocalGroupSize = {8, 8};

        pContext->SetPipelineState(pGenSdfPSO);
        pContext->CommitShaderResources(pGenSdfSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        DispatchComputeAttribs dispatchAttrs;
        dispatchAttrs.ThreadGroupCountX = (DstTexDim.x + LocalGroupSize.x - 1) / LocalGroupSize.x;
        dispatchAttrs.ThreadGroupCountY = (DstTexDim.y + LocalGroupSize.y - 1) / LocalGroupSize.y;

        pContext->DispatchCompute(dispatchAttrs);
    }

    pContext->Flush();
}

// Configura el estado del pipeline y los buffers de constantes para el renderizado.
void Game::CreatePipelineState()
{
    auto Callback = MakeCallback([&](PipelineStateCreateInfo& PipelineCI) {
        auto& GraphicsPipelineCI{static_cast<GraphicsPipelineStateCreateInfo&>(PipelineCI)};
        GraphicsPipelineCI.GraphicsPipeline.RTVFormats[0]    = GetSwapChain()->GetDesc().ColorBufferFormat;
        GraphicsPipelineCI.GraphicsPipeline.NumRenderTargets = 1;
    });

    m_pRSNLoader->LoadPipelineState({"Draw map PSO", PIPELINE_TYPE_GRAPHICS, true, true, Callback, Callback}, &m_Map.pPSO);
    CHECK_THROW(m_Map.pPSO != nullptr);

    BufferDesc CBDesc;
    CBDesc.Name      = "Map constants buffer";
    CBDesc.Size      = sizeof(MapConstants);
    CBDesc.Usage     = USAGE_DEFAULT;
    CBDesc.BindFlags = BIND_UNIFORM_BUFFER;

    GetDevice()->CreateBuffer(CBDesc, nullptr, &m_Map.pConstants);
    CHECK_THROW(m_Map.pConstants != nullptr);
}

// Inicializa la posición inicial del jugador y el buffer de constantes.
void Game::InitPlayer()
{
    if (!m_Player.pConstants)
    {
        BufferDesc CBDesc;
        CBDesc.Name      = "Player constants buffer";
        CBDesc.Size      = sizeof(PlayerConstants);
        CBDesc.Usage     = USAGE_DEFAULT;
        CBDesc.BindFlags = BIND_UNIFORM_BUFFER;

        GetDevice()->CreateBuffer(CBDesc, nullptr, &m_Player.pConstants);
        CHECK_THROW(m_Player.pConstants != nullptr);
    }

    m_Player.Pos = float2{1.5f, static_cast<float>(Constants.MapTexDim.y) - 1.5f};
}

// Vincula recursos como texturas y buffers al pipeline.
void Game::BindResources()
{
    m_Map.pSRB = nullptr;
    m_Map.pPSO->CreateShaderResourceBinding(&m_Map.pSRB, true);
    CHECK_THROW(m_Map.pSRB != nullptr);

    m_Map.pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "cbMapConstants")->Set(m_Map.pConstants);
    m_Map.pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "cbMapConstants")->Set(m_Map.pConstants);
    m_Map.pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_SDFMap")->Set(m_Map.pMapTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    m_Map.pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "cbPlayerConstants")->Set(m_Player.pConstants);
}

// Carga un nuevo mapa regenerando y reinicializando los recursos del juego.
void Game::LoadNewMap()
{
    try
    {
        GetDevice()->IdleGPU();
        GenerateMap();
        CreateSDFMap();
        InitPlayer();
        BindResources();
    }
    catch (...)
    {}
}

} // namespace Diligent