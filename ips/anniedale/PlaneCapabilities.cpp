/*
 * Copyright © 2012 Intel Corporation
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Jackie Li <yaodong.li@intel.com>
 */

#include <HwcTrace.h>
#include <DisplayPlane.h>
#include <hal_public.h>
#include <OMX_IVCommon.h>
#include <PlaneCapabilities.h>
#include <common/OverlayHardware.h>

#define SPRITE_PLANE_MAX_STRIDE_TILED      16384
#define SPRITE_PLANE_MAX_STRIDE_LINEAR     10240

#define OVERLAY_PLANE_MAX_STRIDE_PACKED    4096
#define OVERLAY_PLANE_MAX_STRIDE_LINEAR    8192

namespace android {
namespace intel {

bool PlaneCapabilities::isFormatSupported(int planeType, uint32_t format, uint32_t trans)
{
    if (planeType == DisplayPlane::PLANE_SPRITE || planeType == DisplayPlane::PLANE_PRIMARY) {
        switch (format) {
        case HAL_PIXEL_FORMAT_BGRA_8888:
        case HAL_PIXEL_FORMAT_BGRX_8888:
        case HAL_PIXEL_FORMAT_RGBA_8888:
        case HAL_PIXEL_FORMAT_RGBX_8888:
        case HAL_PIXEL_FORMAT_RGB_565:
            return trans ? false : true;
        default:
            VTRACE("unsupported format %#x", format);
            return false;
        }
    } else if (planeType == DisplayPlane::PLANE_OVERLAY) {
        switch (format) {
        case HAL_PIXEL_FORMAT_I420:
        case HAL_PIXEL_FORMAT_NV12:
        case HAL_PIXEL_FORMAT_YUY2:
        case HAL_PIXEL_FORMAT_UYVY:
            // TODO: overlay supports 180 degree rotation
            if (trans == HAL_TRANSFORM_ROT_180) {
                WTRACE("180 degree rotation is not supported yet");
            }
            return trans ? false : true;
        case HAL_PIXEL_FORMAT_YV12:
        case OMX_INTEL_COLOR_FormatYUV420PackedSemiPlanar:
        case OMX_INTEL_COLOR_FormatYUV420PackedSemiPlanar_Tiled:
            return true;
        default:
            VTRACE("unsupported format %#x", format);
            return false;
        }
    } else {
        ETRACE("invalid plane type %d", planeType);
        return false;
    }
}

bool PlaneCapabilities::isSizeSupported(int planeType,
                                             uint32_t format,
                                             uint32_t w, uint32_t h,
                                             const stride_t& stride)
{
    bool isYUVPacked;
    uint32_t maxStride;

    if (planeType == DisplayPlane::PLANE_SPRITE || planeType == DisplayPlane::PLANE_PRIMARY) {
        switch (format) {
        case HAL_PIXEL_FORMAT_BGRA_8888:
        case HAL_PIXEL_FORMAT_BGRX_8888:
        case HAL_PIXEL_FORMAT_RGBA_8888:
        case HAL_PIXEL_FORMAT_RGBX_8888:
        case HAL_PIXEL_FORMAT_RGB_565:
            VTRACE("stride %d", stride.rgb.stride);
            if (stride.rgb.stride > SPRITE_PLANE_MAX_STRIDE_LINEAR) {
                VTRACE("too large stride %d", stride.rgb.stride);
                return false;
            }
            return true;
        default:
            VTRACE("unsupported format %#x", format);
            return false;
        }
    } else if (planeType == DisplayPlane::PLANE_OVERLAY) {
        switch (format) {
        case HAL_PIXEL_FORMAT_YV12:
        case HAL_PIXEL_FORMAT_I420:
        case HAL_PIXEL_FORMAT_NV12:
        case OMX_INTEL_COLOR_FormatYUV420PackedSemiPlanar:
        case OMX_INTEL_COLOR_FormatYUV420PackedSemiPlanar_Tiled:
            isYUVPacked = false;
            break;
        case HAL_PIXEL_FORMAT_YUY2:
        case HAL_PIXEL_FORMAT_UYVY:
            isYUVPacked = true;
            break;
        default:
            VTRACE("unsupported format %#x", format);
            return false;
        }
        // don't use overlay plane if stride is too big
        maxStride = OVERLAY_PLANE_MAX_STRIDE_LINEAR;
        if (isYUVPacked) {
            maxStride = OVERLAY_PLANE_MAX_STRIDE_PACKED;
        }

        if (stride.yuv.yStride > maxStride) {
            VTRACE("stride %d is too large", stride.yuv.yStride);
            return false;
        }
        return true;
    } else {
        ETRACE("invalid plane type %d", planeType);
        return false;
    }
}

bool PlaneCapabilities::isBlendingSupported(int planeType, uint32_t blending, uint8_t planeAlpha)
{
    if (planeType == DisplayPlane::PLANE_SPRITE || planeType == DisplayPlane::PLANE_PRIMARY) {
        // support premultipled & none blanding
        switch (blending) {
        case DisplayPlane::PLANE_BLENDING_NONE:
        case DisplayPlane::PLANE_BLENDING_PREMULT:
            return true;
        default:
            VTRACE("unsupported blending %#x", blending);
            return false;
        }
    } else if (planeType == DisplayPlane::PLANE_OVERLAY) {
        // overlay doesn't support blending
        return (blending == DisplayPlane::PLANE_BLENDING_NONE) ? true : false;
    } else {
        ETRACE("invalid plane type %d", planeType);
        return false;
    }
}

bool PlaneCapabilities::isScalingSupported(int planeType, hwc_frect_t& src, hwc_rect_t& dest, uint32_t trans)
{
    int srcW, srcH;
    int dstW, dstH;

    srcW = (int)src.right - (int)src.left;
    srcH = (int)src.bottom - (int)src.top;
    dstW = dest.right - dest.left;
    dstH = dest.bottom - dest.top;

    if (planeType == DisplayPlane::PLANE_SPRITE || planeType == DisplayPlane::PLANE_PRIMARY) {
        // no scaling is supported
        return ((srcW == dstW) && (srcH == dstH)) ? true : false;

    } else if (planeType == DisplayPlane::PLANE_OVERLAY) {
        // overlay cannot support resolution that bigger than 2047x2047.
        if ((srcW > INTEL_OVERLAY_MAX_WIDTH - 1) || (srcH > INTEL_OVERLAY_MAX_HEIGHT - 1)) {
            return false;
        }

        if (trans == HAL_TRANSFORM_ROT_90 || trans == HAL_TRANSFORM_ROT_270) {
            int tmp = srcW;
            srcW = srcH;
            srcH = tmp;
        }
        int scaleX = srcW / dstW;
        int scaleY = srcH / dstH;
        if (trans && (scaleX >= 3 || scaleY >= 3)) {
            DTRACE("overlay rotation with scaling >= 3, fall back to GLES");
            return false;
        }

        return true;
    } else {
        ETRACE("invalid plane type %d", planeType);
        return false;
    }
}

bool PlaneCapabilities::isTransformSupported(int planeType, uint32_t trans)
{
    if (planeType == DisplayPlane::PLANE_OVERLAY)
        return true;
    // don't transform any tranform
    return trans ? false : true;
}

} // namespace intel
} // namespace android

