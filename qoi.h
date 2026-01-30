#ifndef QOI_FORMAT_CODEC_QOI_H_
#define QOI_FORMAT_CODEC_QOI_H_

#include <cstring>
#include "utils.h"

constexpr uint8_t QOI_OP_INDEX_TAG = 0x00;
constexpr uint8_t QOI_OP_DIFF_TAG  = 0x40;
constexpr uint8_t QOI_OP_LUMA_TAG  = 0x80;
constexpr uint8_t QOI_OP_RUN_TAG   = 0xc0;
constexpr uint8_t QOI_OP_RGB_TAG   = 0xfe;
constexpr uint8_t QOI_OP_RGBA_TAG  = 0xff;
constexpr uint8_t QOI_PADDING[8] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u};
constexpr uint8_t QOI_MASK_2 = 0xc0;

/**
 * @brief encode the raw pixel data of an image to qoi format.
 *
 * @param[in] width image width in pixels
 * @param[in] height image height in pixels
 * @param[in] channels number of color channels, 3 = RGB, 4 = RGBA
 * @param[in] colorspace image color space, 0 = sRGB with linear alpha, 1 = all channels linear
 *
 * @return bool true if it is a valid qoi format image, false otherwise
 */
bool QoiEncode(uint32_t width, uint32_t height, uint8_t channels, uint8_t colorspace = 0);

/**
 * @brief decode the qoi format of an image to raw pixel data
 *
 * @param[out] width image width in pixels
 * @param[out] height image height in pixels
 * @param[out] channels number of color channels, 3 = RGB, 4 = RGBA
 * @param[out] colorspace image color space, 0 = sRGB with linear alpha, 1 = all channels linear
 *
 * @return bool true if it is a valid qoi format image, false otherwise
 */
bool QoiDecode(uint32_t &width, uint32_t &height, uint8_t &channels, uint8_t &colorspace);


bool QoiEncode(uint32_t width, uint32_t height, uint8_t channels, uint8_t colorspace) {

    // qoi-header part
    QoiWriteChar('q');
    QoiWriteChar('o');
    QoiWriteChar('i');
    QoiWriteChar('f');
    QoiWriteU32(width);
    QoiWriteU32(height);
    QoiWriteU8(channels);
    QoiWriteU8(colorspace);

    /* qoi-data part: run = count of same pixels (0, 2..63). Stored byte = 0xc0|(run-1-1) = 0xc0|(run-2) for run 2..63 */
    int run = 0;
    int px_num = static_cast<int>(width * height);

    uint8_t history[64][4];
    memset(history, 0, sizeof(history));

    uint8_t r, g, b, a;
    a = 255u;
    uint8_t pre_r = 0u, pre_g = 0u, pre_b = 0u, pre_a = 255u;

    for (int i = 0; i < px_num; ++i) {
        r = QoiReadU8();
        g = QoiReadU8();
        b = QoiReadU8();
        if (channels == 4) a = QoiReadU8();
        else a = 255u;

        if (r == pre_r && g == pre_g && b == pre_b && a == pre_a) {
            if (run == 0) run = 2;
            else run++;
            if (run == 64) {
                QoiWriteU8(static_cast<uint8_t>(QOI_OP_RUN_TAG | 61));  // 62 copies = 63 pixels
                run = 2;
            }
        } else {
            if (run >= 2) {
                QoiWriteU8(static_cast<uint8_t>(QOI_OP_RUN_TAG | (run - 2)));
                run = 0;
            }

            int idx = QoiColorHash(r, g, b, a);
            if (history[idx][0] == r && history[idx][1] == g &&
                history[idx][2] == b && history[idx][3] == a) {
                QoiWriteU8(static_cast<uint8_t>(QOI_OP_INDEX_TAG | idx));
            } else {
                int dr = static_cast<int>(r) - pre_r;
                int dg = static_cast<int>(g) - pre_g;
                int db = static_cast<int>(b) - pre_b;
                if (dr >= -2 && dr <= 1 && dg >= -2 && dg <= 1 && db >= -2 && db <= 1) {
                    QoiWriteU8(static_cast<uint8_t>(QOI_OP_DIFF_TAG | ((dr + 2) << 4) |
                        ((dg + 2) << 2) | (db + 2)));
                } else {
                    int dg_ = static_cast<int>(g) - pre_g;
                    int dr_dg = static_cast<int>(r) - pre_r - dg_;
                    int db_dg = static_cast<int>(b) - pre_b - dg_;
                    if (dg_ >= -32 && dg_ <= 31 && dr_dg >= -8 && dr_dg <= 7 &&
                        db_dg >= -8 && db_dg <= 7) {
                        QoiWriteU8(static_cast<uint8_t>(QOI_OP_LUMA_TAG | (dg_ + 32)));
                        QoiWriteU8(static_cast<uint8_t>(((dr_dg + 8) << 4) | (db_dg + 8)));
                    } else {
                        if (a != pre_a) {
                            QoiWriteU8(QOI_OP_RGBA_TAG);
                            QoiWriteU8(r);
                            QoiWriteU8(g);
                            QoiWriteU8(b);
                            QoiWriteU8(a);
                        } else {
                            QoiWriteU8(QOI_OP_RGB_TAG);
                            QoiWriteU8(r);
                            QoiWriteU8(g);
                            QoiWriteU8(b);
                        }
                    }
                }
            }

            history[idx][0] = r;
            history[idx][1] = g;
            history[idx][2] = b;
            history[idx][3] = a;
            pre_r = r;
            pre_g = g;
            pre_b = b;
            pre_a = a;
        }
    }
    if (run >= 2) {
        QoiWriteU8(static_cast<uint8_t>(QOI_OP_RUN_TAG | (run - 2)));
    }

    for (size_t i = 0; i < sizeof(QOI_PADDING) / sizeof(QOI_PADDING[0]); ++i) {
        QoiWriteU8(QOI_PADDING[i]);
    }

    return true;
}

bool QoiDecode(uint32_t &width, uint32_t &height, uint8_t &channels, uint8_t &colorspace) {

    char c1 = QoiReadChar();
    char c2 = QoiReadChar();
    char c3 = QoiReadChar();
    char c4 = QoiReadChar();
    if (c1 != 'q' || c2 != 'o' || c3 != 'i' || c4 != 'f') {
        return false;
    }

    width = QoiReadU32();
    height = QoiReadU32();
    channels = QoiReadU8();
    colorspace = QoiReadU8();

    int px_num = static_cast<int>(width * height);

    uint8_t history[64][4];
    memset(history, 0, sizeof(history));

    uint8_t r = 0, g = 0, b = 0, a = 255u;
    uint8_t pre_r = 0, pre_g = 0, pre_b = 0, pre_a = 255u;

    int out_count = 0;
    while (out_count < px_num) {
        uint8_t b1 = QoiReadU8();

        if (b1 == QOI_OP_RGB_TAG) {
            r = QoiReadU8();
            g = QoiReadU8();
            b = QoiReadU8();
            a = pre_a;
        } else if (b1 == QOI_OP_RGBA_TAG) {
            r = QoiReadU8();
            g = QoiReadU8();
            b = QoiReadU8();
            a = QoiReadU8();
        } else if ((b1 & QOI_MASK_2) == QOI_OP_RUN_TAG) {
            int run_len = (b1 & 0x3f) + 1;
            for (int j = 0; j < run_len && out_count < px_num; ++j) {
                QoiWriteU8(pre_r);
                QoiWriteU8(pre_g);
                QoiWriteU8(pre_b);
                if (channels == 4) QoiWriteU8(pre_a);
                out_count++;
            }
            continue;
        } else if ((b1 & QOI_MASK_2) == QOI_OP_INDEX_TAG) {
            int idx = b1 & 0x3f;
            r = history[idx][0];
            g = history[idx][1];
            b = history[idx][2];
            a = history[idx][3];
        } else if ((b1 & QOI_MASK_2) == QOI_OP_DIFF_TAG) {
            r = static_cast<uint8_t>(pre_r + ((b1 >> 4) & 3) - 2);
            g = static_cast<uint8_t>(pre_g + ((b1 >> 2) & 3) - 2);
            b = static_cast<uint8_t>(pre_b + (b1 & 3) - 2);
            a = pre_a;
        } else if ((b1 & QOI_MASK_2) == QOI_OP_LUMA_TAG) {
            int dg = (b1 & 0x3f) - 32;
            uint8_t b2 = QoiReadU8();
            int dr_dg = (b2 >> 4) - 8;
            int db_dg = (b2 & 0x0f) - 8;
            r = static_cast<uint8_t>(pre_r + dg + dr_dg);
            g = static_cast<uint8_t>(pre_g + dg);
            b = static_cast<uint8_t>(pre_b + dg + db_dg);
            a = pre_a;
        } else {
            return false;
        }

        QoiWriteU8(r);
        QoiWriteU8(g);
        QoiWriteU8(b);
        if (channels == 4) QoiWriteU8(a);
        out_count++;

        int idx = QoiColorHash(r, g, b, a);
        history[idx][0] = r;
        history[idx][1] = g;
        history[idx][2] = b;
        history[idx][3] = a;
        pre_r = r;
        pre_g = g;
        pre_b = b;
        pre_a = a;
    }

    bool valid = true;
    for (size_t i = 0; i < sizeof(QOI_PADDING) / sizeof(QOI_PADDING[0]); ++i) {
        if (QoiReadU8() != QOI_PADDING[i]) valid = false;
    }

    return valid;
}

#endif // QOI_FORMAT_CODEC_QOI_H_
