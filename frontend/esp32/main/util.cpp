#include "util.h"

#include <math.h>

void draw_spinner(TFT_t& dev, int degrees, int colour)
{
    const double radius = CONFIG_WIDTH * 0.4;
    const auto rads = M_PI*degrees/180.0;
    lcdDrawFillCircle(&dev,
                      CONFIG_WIDTH/2 + cos(rads)*radius,
                      CONFIG_HEIGHT/2 + sin(rads)*radius,
                      16, colour);
}

static const int step_degrees = 36;
static const int nof_bins = 4;
static int bins[nof_bins];
static int active_bins = 0;
static int colours[] = { CYAN, 0x0555, 0x02AA, BLACK };

void update_spinner(TFT_t& dev)
{
    // Initialize
    if (active_bins == 0)
    {
        bins[0] = 0;
        active_bins = 1;
    }
    else
    {
        if (active_bins < nof_bins)
            ++active_bins;
        for (int i = active_bins - 1; i > 0; --i)
            bins[i] = bins[i-1];
        bins[0] = bins[1] + step_degrees;
        if (bins[0] >= 360)
            bins[0] = 0;
    }
    for (int i = 0; i < active_bins; ++i)
        draw_spinner(dev, bins[i], colours[i]); 
}
