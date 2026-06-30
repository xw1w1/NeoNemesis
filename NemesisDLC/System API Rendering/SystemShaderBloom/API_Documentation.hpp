#pragma once

namespace Nemesis::SystemShaderBloom::API_Documentation
{

struct FullAPIGuide
{
/*

═══════════════════════════════════════════════════════════════
                    NEMESIS BLOOM EFFECT API
                           FULL GUIDE
═══════════════════════════════════════════════════════════════

1. QUICK START
──────────────────────────────────────────────────────────────

#include "System API Rendering/SystemShaderBloom/SystemShaderBloom.hpp"

// Initialize
std::unique_ptr<BloomEffect> bloom = 
    Nemesis::SystemShaderBloom::CreateBloomEffect(device, context, 1920, 1080);

if (!bloom || !bloom->IsInitialized())
{
    NWARN("Bloom initialization failed");
    return;
}

// Render
Nemesis::SystemShaderBloom::BloomParams params;
params.threshold = 0.8f;
params.intensity = 1.5f;
params.downscale = 0.5f;

Nemesis::SystemShaderBloom::BloomColor color(1.0f, 0.0f, 0.0f, 1.0f);
bloom->Render(sourceTexture, color, params);

// Get result
ID3D11ShaderResourceView* result = bloom->GetOutputTexture();

// Cleanup (automatic with unique_ptr)
bloom.reset();


2. API STRUCTURES
──────────────────────────────────────────────────────────────

BloomParams struct:
  - threshold (0.0-2.0): Brightness cutoff for bloom extraction
  - intensity (0.0-3.0): Bloom effect strength multiplier  
  - blurRadius (1.0-10.0): Gaussian blur radius
  - downscale (0.25-1.0): Resolution reduction factor

BloomColor struct:
  - r, g, b, a (0.0-1.0): RGBA color components
  - ToXMFLOAT4(): Convert to DirectXMath format


3. MAIN API METHODS
──────────────────────────────────────────────────────────────

bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context,
                uint32_t width, uint32_t height)
  - Initialize bloom effect with render target dimensions
  - Call once at startup
  - Returns false on error

bool Render(ID3D11ShaderResourceView* sourceTexture,
            const BloomColor& color,
            const BloomParams& params)
  - Execute bloom pipeline on source texture
  - Returns false on error
  - Sets render targets internally

ID3D11ShaderResourceView* GetOutputTexture() const
  - Get final bloom result for composition
  - Returns nullptr on error
  - Valid only after Render() call

void SetDefaultParams(const BloomParams& params)
  - Set parameters used when Render() called without params argument
  - Useful for tuning bloom without changing call sites

bool IsInitialized() const
  - Check if bloom is ready to use
  - Returns false before Initialize() or after Release()

void Release()
  - Manually free all GPU resources
  - Called automatically by destructor
  - Cannot use object after Release()


4. COLOR SYSTEM
──────────────────────────────────────────────────────────────

Creating Colors:

Direct RGBA (0.0-1.0 range):
  BloomColor(1.0f, 0.0f, 0.0f, 1.0f)  // Red

Predefined Colors (use Color class from BloomColor.hpp):
  Color::White
  Color::Black
  Color::Red
  Color::Green
  Color::Blue
  Color::Yellow
  Color::Cyan
  Color::Magenta
  Color::Orange
  Color::Purple
  Color::Pink
  Color::Lime

Note: Use Color class, convert to BloomColor if needed:
  Color white = Color::White;
  BloomColor bloomColor = BloomColor(white.r, white.g, white.b, white.a);


5. BLOOM PARAMETER TUNING
──────────────────────────────────────────────────────────────

Threshold:
  0.3-0.5:  Very bright bloom, many pixels affected
  0.8-1.0:  Balanced (recommended for most games)
  1.5-2.0:  Only brightest lights bloom

Intensity:
  0.5:      Subtle glow
  1.0-1.5:  Standard bloom effect
  2.0+:     Strong, visible bloom

Downscale:
  0.25:     4x faster, quarter resolution (mobile)
  0.5:      2x faster, half resolution (recommended)
  1.0:      Full resolution (best quality)


6. INTEGRATION PATTERNS
──────────────────────────────────────────────────────────────

Pattern 1: Global Bloom
  static std::unique_ptr<BloomEffect> g_bloom;

  // Initialize once
  g_bloom = CreateBloomEffect(device, context, screenWidth, screenHeight);

  // Use every frame after rendering scene
  g_bloom->Render(sceneTexture, BloomColor(1.0f, 1.0f, 1.0f, 1.0f), params);


Pattern 2: Multiple Bloom Instances
  std::vector<std::unique_ptr<BloomEffect>> blooms;

  for (const auto& entity : entities)
  {
      auto bloom = CreateBloomEffect(device, context, width, height);
      bloom->Render(entity.texture, entity.bloomColor, entity.bloomParams);
      blooms.push_back(std::move(bloom));
  }


Pattern 3: Dynamic Bloom Control
  // Adjust based on gameplay state
  BloomParams params;
  if (inExplosion)
      params.intensity = 3.0f;
  else
      params.intensity = 1.0f;

  bloom->Render(texture, color, params);


7. ERROR HANDLING
──────────────────────────────────────────────────────────────

Check initialization:
  auto bloom = CreateBloomEffect(device, context, width, height);
  if (!bloom || !bloom->IsInitialized())
  {
      NWARN("Bloom failed to initialize");
      return false;
  }

Check render:
  if (!bloom->Render(sourceTexture, color, params))
  {
      NWARN("Bloom render failed");
      return false;
  }

Check output:
  auto result = bloom->GetOutputTexture();
  if (!result)
  {
      NWARN("Bloom output unavailable");
      return false;
  }

Monitor logs:
  - NWARN() for errors
  - NLOG() for initialization info


8. PERFORMANCE GUIDELINES
──────────────────────────────────────────────────────────────

Optimal Settings by Platform:

Mobile/Low-End:
  downscale = 0.25
  threshold = 0.9f
  intensity = 0.8f
  → ~0.1-0.2ms per frame

Mid-Range:
  downscale = 0.5
  threshold = 0.8f
  intensity = 1.5f
  → ~0.5-0.8ms per frame

High-End:
  downscale = 1.0
  threshold = 0.7f
  intensity = 2.0f
  → ~2-3ms per frame


Memory Usage:
  At 0.25 downscale:  ~750 KB
  At 0.5 downscale:   ~3 MB (recommended)
  At 1.0 downscale:   ~12 MB


9. THREAD SAFETY
──────────────────────────────────────────────────────────────

NOT thread-safe!
  - Create on render thread only
  - Call Render() from render thread only
  - Do NOT share between threads
  - Synchronize device/context access externally


10. RESOURCE LIFECYCLE
──────────────────────────────────────────────────────────────

Creation:
  1. CreateBloomEffect() allocates GPU resources
  2. Shaders compiled at runtime
  3. Textures and buffers created
  4. All resources valid on IsInitialized() == true

Usage:
  1. Call Render() with source texture
  2. GPU executes: threshold → blur horizontal → blur vertical → compose
  3. Call GetOutputTexture() to retrieve result

Cleanup:
  1. bloom.reset() (automatic with unique_ptr)
  2. ~BloomEffect() calls Release()
  3. All GPU resources freed
  4. Cannot use object after cleanup


11. SHADER PIPELINE
──────────────────────────────────────────────────────────────

Stage 1: Threshold (ThresholdPS)
  Input: Scene color texture
  Output: Bright pixels extracted above threshold

Stage 2: Blur Horizontal (BlurHPS)
  Input: Extracted bright texture
  Output: 7-tap Gaussian blur in X direction

Stage 3: Blur Vertical (BlurVPS)
  Input: Horizontal blurred texture
  Output: Final blur in Y direction (full bloom)

Stage 4: Compose (ComposePS)
  Input: Blurred bloom texture + color tint
  Output: Color-tinted bloom ready for screen


12. ADVANCED FEATURES
──────────────────────────────────────────────────────────────

Custom Colors:
  BloomColor custom(0.8f, 0.2f, 0.5f, 1.0f);
  bloom->Render(texture, custom, params);

Parameter Animation:
  BloomParams params;
  params.intensity = 1.0f + sin(time) * 0.5f;
  bloom->Render(texture, color, params);

Multiple Effects:
  std::unique_ptr<BloomEffect> bloom1, bloom2;
  bloom1->Render(texture1, Color::Red, params);
  bloom2->Render(texture2, Color::Blue, params);


13. TROUBLESHOOTING
──────────────────────────────────────────────────────────────

Issue: Bloom not visible
  - Check threshold isn't too high (try 0.8f)
  - Verify intensity > 0.5f
  - Confirm source texture has bright pixels
  - Check output texture is being used in composition

Issue: Low frame rate
  - Increase downscale (try 0.25)
  - Reduce intensity to cut fill rate
  - Reduce threshold to limit bloom area

Issue: Bloom looks blocky
  - Increase blur radius
  - Use higher resolution (reduce downscale)
  - Check source texture has proper bright data

*/
};

struct QuickStart
{
/*
1. INITIALIZATION:
   std::unique_ptr<BloomEffect> bloom = CreateBloomEffect(device, context, width, height);

2. RENDERING:
   BloomParams params;
   params.threshold = 0.8f;
   params.intensity = 1.5f;
   bloom->Render(sourceTexture, BloomColor(1.0f, 1.0f, 1.0f, 1.0f), params);

3. GET RESULT:
   ID3D11ShaderResourceView* result = bloom->GetOutputTexture();

4. CLEANUP:
   bloom.reset();
*/
};

struct BloomParamsGuide
{
/*
threshold (0.0 - 2.0):
  - Low (0.3-0.5): Very bright bloom, affects many pixels
  - Medium (0.8-1.0): Balanced, typical gameplay use
  - High (1.5-2.0): Only brightest areas bloom

intensity (0.0 - 3.0):
  - Low (0.5): Subtle glow
  - Medium (1.0-1.5): Standard bloom effect
  - High (2.0+): Strong, visible bloom

blurRadius (1.0 - 10.0):
  - Small (1-2): Sharp, localized bloom
  - Medium (3-5): Typical soft glow
  - Large (6+): Wide, diffuse bloom

downscale (0.25 - 1.0):
  - 0.25: 4x faster, lower quality
  - 0.5: 2x faster, good balance (recommended)
  - 1.0: Full resolution, slowest but best quality
*/
};

struct ColorUsageGuide
{
/*
Predefined Colors from Color class:
  Color::White     - Full intensity (1,1,1,1)
  Color::Black     - No bloom (0,0,0,1)
  Color::Red       - (1,0,0,1)
  Color::Green     - (0,1,0,1)
  Color::Blue      - (0,0,1,1)
  Color::Yellow    - (1,1,0,1)
  Color::Cyan      - (0,1,1,1)
  Color::Magenta   - (1,0,1,1)
  Color::Orange    - (1,0.5,0,1)
  Color::Purple    - (0.5,0,0.5,1)
  Color::Pink      - (1,0.192,0.203,1)
  Color::Lime      - (0.5,1,0,1)

Custom Creation with BloomColor:
  BloomColor custom(1.0f, 0.5f, 0.0f, 1.0f)
  BloomColor white(1.0f, 1.0f, 1.0f, 1.0f)

Conversions:
  color.ToXMFLOAT4()
  color.ToXMVector()

Operations:
  BloomColor result = color * 0.5f;
  float brightness = (color.r + color.g + color.b) / 3.0f;
*/
};

struct PerformanceOptimization
{
/*
Memory Usage:
  - Full 1920x1080: ~12 MB (4 textures at full resolution)
  - Half resolution: ~3 MB (most common)
  - Quarter resolution: ~750 KB

Rendering Cost:
  - Typically 1-3ms on modern GPU at 1920x1080
  - Scales linearly with bloom pass count
  - Downscaling provides best performance/quality tradeoff

Recommendations:
  1. Use downscale = 0.5 for 60FPS games
  2. Use downscale = 0.25 for CPU-bound scenarios
  3. Cache bloom textures across frames
  4. Avoid rebuilding textures every frame
  5. Use simpler thresholds in performance-critical paths
*/
};

struct CommonPatterns
{
/*
Pattern 1: Global Bloom Effect
  Single bloom instance shared across frame
  Applied once at end of render pipeline

Pattern 2: Per-Object Bloom
  Separate bloom for specific objects/UI
  Allows different colors/intensities per target

Pattern 3: Dynamic Bloom Control
  Adjust intensity based on gameplay state
  Example: Pulsating bloom based on game events

Pattern 4: Color-Coded Bloom
  Different colors for different entity types
  Red for enemies, Blue for allies, etc.

Pattern 5: Post-Processing Chain
  Bloom as part of larger post-process stack
  Order: Bloom -> Tone Mapping -> Output
*/
};

struct ThreadSafety
{
/*
Current Implementation:
  - NOT thread-safe for rendering
  - Must be used from single render thread
  - Device/context must be from same thread

Recommendations:
  1. Create bloom effect on render thread
  2. Call Render() only from render thread
  3. Do NOT share bloom instance between threads
  4. Synchronize device/context access externally
*/
};

struct ResourceManagement
{
/*
Automatic Cleanup:
  std::unique_ptr<BloomEffect> bloom = CreateBloomEffect(...);
  bloom.reset();  Destructor calls Release()

Manual Cleanup:
  bloom->Release();  Explicit resource release

Lifecycle:
  1. CreateBloomEffect() allocates all GPU resources
  2. Render() executes bloom pipeline
  3. GetOutputTexture() retrieves final SRV
  4. ~BloomEffect() or Release() frees resources

Note: After Release(), object cannot be reused
*/
};

struct ErrorHandling
{
/*
Initialization Failures:
  - Check IsInitialized() after CreateBloomEffect()
  - Possible causes: invalid device/context, OOM, invalid resolution

Rendering Failures:
  - Render() returns false on error (e.g., invalid SRV)
  - Always check return value in production code
  - Null input texture treated as error

Debug Output:
  - NWARN() and NLOG() messages to LogsSystem
  - Useful for troubleshooting in development
*/
};

struct ShaderCompilation
{
/*
Compiled Internally:
  All HLSL shaders compiled at runtime
  Located in BloomShaders.hpp as constexpr strings

Shader Targets:
  Vertex: vs_4_0 (fullscreen pass generation)
  Pixel: ps_4_0 (all image processing)

Pipeline Stages:
  1. FullscreenVS: Generates fullscreen quad
  2. ThresholdPS: Extracts bright pixels
  3. BlurHPS: Horizontal Gaussian blur
  4. BlurVPS: Vertical Gaussian blur
  5. ComposePS: Final color composition

Customization:
  To modify shaders, edit BloomShaders.hpp
  Recompile implementation after changes
*/
};

}
