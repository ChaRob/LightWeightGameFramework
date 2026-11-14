#pragma once

// Source pixels, measured from the top-left of the original image.
struct TextureRegion
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    constexpr bool IsValid() const
    {
        return x >= 0 && y >= 0 && width > 0 && height > 0;
    }

    constexpr bool FitsWithin(int _width, int _height) const
    {
        return IsValid() && x <= _width && y <= _height &&
            width <= _width - x && height <= _height - y;
    }
};
