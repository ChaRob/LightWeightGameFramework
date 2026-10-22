# Text rendering (V1)

LWGF supports simple single-line 2D text using TrueType fonts and stb_truetype.
Font owns glyph metrics and a single-channel atlas; a future Bitmap Font loader
can reuse this data representation without a new renderer or interface hierarchy.

## Usage

Load the TTF once after Application and Renderer2D are initialized.
The sample includes Anonymous Pro with its SIL Open Font License 1.1.

```cpp
Font font;
if (!font.Load("assets/fonts/AnonymousPro-Regular.ttf", 48.0f))
{
    return false;
}

renderer.DrawText(font, "STAGE 12", 32.0f, 670.0f, 32.0f,
                  Color{ 1.0f, 1.0f, 1.0f, 1.0f });

const float width = renderer.MeasureTextWidth(font, "GAME OVER", 48.0f);
renderer.DrawText(font, "GAME OVER", (1280.0f - width) * 0.5f,
                  350.0f, 48.0f, Color{ 1.0f, 0.3f, 0.3f, 1.0f });
```

## V1 policies and limits

- Printable ASCII (0x20 to 0x7E) only. Every unsupported byte (UTF-8, newline,
  control characters) is displayed as '?'. No kerning or paragraph layout.
- Pixel height is nominal. Load at the size most often drawn; DrawText scales
  rasterized glyphs and extreme enlargement may blur.
- Bottom-left-origin screen pixels, consistent with DrawRect. DrawText (x,y)
  represents the left end of the text baseline.
- MeasureTextWidth returns pen advance rather than tight ink bounds.
- Load reports errors and preserves a previous valid font on failure.
- Font is noncopyable and owns the GPU texture. Destroy fonts and the renderer
  before Application destroys the OpenGL context.
- The glyph atlas is baked once per successful Load, not every frame.
- Current V1 draws one quad per visible glyph; optimize only after measuring.
- Android requires platform/window/GL ES changes outside font loading.
- Bundle licensed font files with release builds. Do not rely on OS fonts.

## Manual validation

1. Build x64-debug and x64-release.
2. Run SampleGame and verify STAGE 12, animated TIME/SCORE, and GAME OVER.
3. Confirm existing textures and rectangles still render correctly.
4. Try different colors, positions, sizes, empty and unsupported strings.
5. Repeat font loading; compare RAM/GPU memory and frame time with the baseline.
6. Temporarily remove the font file and confirm a clear initialization error.
