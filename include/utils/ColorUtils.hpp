#pragma once
#include <vector>
#include <cmath>
#include "../core.hpp" // For struct Color

class ColorUtils {
public:
    // Convert HSV (Hue [0..1], Saturation [0..1], Value [0..1]) to RGB Color
    static Color hsv_to_rgb(float h, float s, float v) {
        int i = int(h * 6);
        float f = h * 6 - i;
        float p = v * (1 - s);
        float q = v * (1 - f * s);
        float t = v * (1 - (1 - f) * s);

        float rf, gf, bf;
        switch (i % 6) {
            case 0: rf = v; gf = t; bf = p; break;
            case 1: rf = q; gf = v; bf = p; break;
            case 2: rf = p; gf = v; bf = t; break;
            case 3: rf = p; gf = q; bf = v; break;
            case 4: rf = t; gf = p; bf = v; break;
            case 5: rf = v; gf = p; bf = q; break;
            default: rf = 0; gf = 0; bf = 0; break;
        }
        
        return {
            static_cast<int>(rf * 255),
            static_cast<int>(gf * 255),
            static_cast<int>(bf * 255)
        };
    }

    // Generate a heatmap color (Blue -> Red) based on value t [0..1]
    static Color get_heatmap_color(float t) {
        // Clamp t to [0, 1]
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        
        // Hue goes from 240 (Blue) to 0 (Red)
        float h = (1.0f - t) * (240.0f / 360.0f);
        return hsv_to_rgb(h, 0.85f, 0.95f);
    }
};
