#include <stdint.h>

//PerracoLabs - https://codereview.stackexchange.com/questions/28645/

void resize_bilinear(const uint32_t *in, uint32_t *out,
	const uint32_t srcW, const uint32_t srcH,
	const uint32_t destW, const uint32_t destH)
{
    uint32_t r, g, b, a;

    const uint32_t wStepFixed16b = ((srcW - 1) << 16)  / ((uint32_t)(destW - 1));
    const uint32_t hStepFixed16b = ((srcH - 1) << 16) / ((uint32_t)(destH - 1));

    uint32_t heightCoefficient = 0;

    for (int y = 0; y < destH; y++)
    {
        uint32_t offsetY = (heightCoefficient >> 16);
        uint32_t hc2 = (heightCoefficient >> 9) & (unsigned char)127;
        uint32_t hc1 = 128 - hc2;

        uint32_t widthCoefficient = 0;

        int offsetPixelY  = offsetY * srcW;
        int offsetPixelY1 = (offsetY + 1) * srcW;

        for (int x = 0; x < destW; x++)
        {
            uint32_t offsetX = (widthCoefficient >> 16);
            uint32_t wc2 = (widthCoefficient >> 9) & (unsigned char)127;
            uint32_t wc1 = 128 - wc2;

            int offsetX1 = offsetX + 1;

            uint32_t pixel1 = *(in + (offsetPixelY  + offsetX));
            uint32_t pixel2 = *(in + (offsetPixelY1 + offsetX));
            uint32_t pixel3 = *(in + (offsetPixelY  + offsetX1));
            uint32_t pixel4 = *(in + (offsetPixelY1 + offsetX1));

            a = ((((pixel1 >> 24) & 0xff) * hc1 + ((pixel2 >> 24) & 0xff) * hc2) * wc1 +
                (((pixel3  >> 24) & 0xff) * hc1 + ((pixel4 >> 24) & 0xff) * hc2) * wc2) >> 14;

            r = ((((pixel1 >> 16) & 0xff) * hc1 + ((pixel2 >> 16) & 0xff) * hc2) * wc1 +
                (((pixel3  >> 16) & 0xff) * hc1 + ((pixel4 >> 16) & 0xff) * hc2) * wc2) >> 14;

            g = ((((pixel1 >> 8) & 0xff) * hc1 + ((pixel2 >> 8) & 0xff) * hc2) * wc1 +
                (((pixel3  >> 8) & 0xff) * hc1 + ((pixel4 >> 8) & 0xff) * hc2) * wc2) >> 14;

            b = ((((pixel1) & 0xff) * hc1 + ((pixel2) & 0xff) * hc2) * wc1 +
                (((pixel3)  & 0xff) * hc1 + ((pixel4) & 0xff) * hc2) * wc2) >> 14;

            *out++ = (a << 24) | (r << 16) | (g << 8) | b;

            widthCoefficient += wStepFixed16b;
        }

        heightCoefficient += hStepFixed16b;
    }
}
