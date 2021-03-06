#include "util.h"

#include <math.h>

std::string strip(const std::string& s)
{
    size_t i = 0;
    while (i < s.size() && isspace(s[i]))
        ++i;
    if (i > 0)
        --i;
    auto result = s.substr(i);
    while (!result.empty() && isspace(result.back()))
    {
        result.erase(result.size() - 1);
    }
    return result;
}

void split(const std::string& s,
           const std::string& delim,
           std::vector<std::string>& v)
{
    size_t start = 0, end = 0;
    while (end != std::string::npos)
    {
        end = s.find(delim, start);
        v.push_back(s.substr(start,
                             (end == std::string::npos) ? std::string::npos : end - start));
        start = (end > (std::string::npos - delim.size())) ? std::string::npos : end + delim.size();
    }
}

void draw_spinner(TFT_t& dev, int degrees, int colour)
{
    const double radius = CONFIG_WIDTH * 0.4;
    const auto rads = M_PI*degrees/180.0;
    lcdDrawFillCircle(&dev,
                      CONFIG_WIDTH/2 + cos(rads)*radius,
                      CONFIG_HEIGHT/2 + sin(rads)*radius,
                      12, colour);
}

static const int step_degrees = 36;
static const int nof_bins = 5;
static int bins[nof_bins];
static int active_bins = 0;
static int colours[] = { TFT_CYAN, 0x0555, 0x02AA, 0x0145, TFT_BLACK };

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
