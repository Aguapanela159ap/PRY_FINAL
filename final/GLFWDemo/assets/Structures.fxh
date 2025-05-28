
struct PlayerConstants
{
    float2 PlayerPos;
    float  PlayerRadius;
    float  AmbientLightRadius;
    float2 FlashLightDir;
    float  FlashLightPower;
    float  FlshLightMaxDist;
};

struct MapConstants
{
    float2 ScreenRectLR; // left, right
    float2 ScreenRectTB; // top, bottom
    float2 UVToMap;
    float2 MapToUV;
    float2 TeleportPos;
    float  TeleportRadius;
    float  TeleportWaveRadius;
    float2 SpikesPos;           // Position of spikes
    float  SpikesRadius;        // Radius of spikes
    float  SpikesWaveRadius;    // Wave animation radius for spikes
};
