# Research: Rework Rendering Pipeline for Stanley Parable Quality

## Problem Analysis

The scene looks washed out or stylized because the StylizePass applies posterization, dithering, and edge detection. Even with `enableDither = false`, the shader still processes colors through an S-curve luma reshape + desaturation before output.

## Stanley Parable Reference

- SP Ultra Deluxe uses **Unity** with standard PBR rendering
- Clean, smooth surfaces with moderate lighting and clear shadows
- No dithering, no posterization, no edge detection
- ACES or similar tonemapping for HDR → LDR
- Warm neutral lighting, moderate exposure

## Current Pipeline

```
Scene → sceneFBO → CompositePass → compositeFBO → StylizePass → Screen
```

**CompositePass** (composite.frag): sky, fog, bloom, ACES tonemapping, vignette, grain, split-tone, saturation, contrast, gamma — this is all standard and good.

**StylizePass** (stylize.frag): posterization + dithering + edge detection — this is what creates the stylized 1-bit look. When `uEnableDither == 0`, it STILL applies posterization:

```glsl
// Lines 357-362: posterization happens regardless of dither flag
float luma = dot(gradedColor, lumaWeights);
float shapedLuma = clamp((luma - 0.006) * 1.16, 0.0, 1.0);
shapedLuma = shapedLuma * shapedLuma * (3.0 - 2.0 * shapedLuma);
vec3 posterColor = gradedColor * (shapedLuma / max(luma, 0.001));
posterColor = mix(vec3(shapedLuma), posterColor, 0.92); // 8% desaturation
```

## Required Changes

### 1. stylize.frag — Clean passthrough when dither disabled
When `uEnableDither == 0`, output `gradedColor` directly (with optional edge overlay), NOT `posterColor`. This skips all posterization math.

### 2. PostProcessParams.h — Change defaults
- `enableDither`: `true` → `false`
- `enableEdges`: `true` → `false`

### 3. EnvironmentProfile.cpp — Tune for clean warm look
- Set `enableDither = false`, `enableEdges = false`
- Adjust exposure, bloom, vignette, saturation for natural warm output
- Keep ACES tonemapping (mode 1)
