#pragma once

namespace Nemesis::SystemShaderBloom::API_Documentation
{

struct SimpleBloomAPI
{
/*
NEMESIS BLOOM API

Use it in 3 steps:

1. Create once:
   auto bloom = Nemesis::SystemShaderBloom::CreateBloomEffect(device, context, width, height);

2. Render when you have a source texture:
   bloom->Render(sourceTexture, Nemesis::SystemShaderBloom::BloomColor::White());

3. Use the result:
   ID3D11ShaderResourceView* output = bloom->GetOutputTexture();


DEFAULT LOOK

BloomParams defaults are already usable:
   threshold  = 1.0f
   intensity  = 1.0f
   blurRadius = 2.0f
   downscale  = 0.5f

Most code should only call:
   bloom->Render(sourceTexture, color);


CUSTOM LOOK

Use params only when you need tuning:

   BloomParams params = BloomParams::Strong();

   bloom->Render(sourceTexture, BloomColor::Gold(), params);


PARAMETERS

threshold:
   Lower = more pixels glow.
   Higher = only very bright pixels glow.

intensity:
   Bloom strength.

blurRadius:
   Glow width.

downscale:
   0.5 is the normal fast/clean choice.
   0.25 is faster.
   1.0 is sharper but heavier.


SAFE USAGE RULES

- Create BloomEffect once, not every frame.
- Call Render only with a valid sourceTexture.
- Call GetOutputTexture after Render.
- Use it on the render thread.
- unique_ptr cleanup is enough; Release is optional.
*/
};

struct Presets
{
/*
BloomParams::Soft()
BloomParams::Normal()
BloomParams::Strong()

BloomColor::White()
BloomColor::Red()
BloomColor::Blue()
BloomColor::Gold()
*/
};

}
