#include <linux/videodev2.h>

#include <xen/be/Exception.hpp>
#include <xen/io/cameraif.h>

#include "V4L2ToXen.hpp"

const V4L2ToXen::xen_to_v4l2 V4L2ToXen::XEN_TYPE_TO_V4L2_CID[] = {
    {
        .xen = XENCAMERA_CTRL_BRIGHTNESS,
        .v4l2 = V4L2_CID_BRIGHTNESS,
    },
    {
        .xen = XENCAMERA_CTRL_CONTRAST,
        .v4l2 = V4L2_CID_CONTRAST,
    },
    {
        .xen = XENCAMERA_CTRL_SATURATION,
        .v4l2 = V4L2_CID_SATURATION,
    },
    {
        .xen = XENCAMERA_CTRL_HUE,
        .v4l2 = V4L2_CID_HUE
    },
    {
        -1
    }
};

const V4L2ToXen::xen_to_v4l2 V4L2ToXen::XEN_COLORSPACE_TO_V4L2[] = {
    {
        .xen = XENCAMERA_COLORSPACE_DEFAULT,
        .v4l2 = V4L2_COLORSPACE_DEFAULT,
    },
    {
        .xen = XENCAMERA_COLORSPACE_SMPTE170M,
        .v4l2 = V4L2_COLORSPACE_SMPTE170M,
    },
    {
        .xen = XENCAMERA_COLORSPACE_REC709,
        .v4l2 = V4L2_COLORSPACE_REC709,
    },
    {
        .xen = XENCAMERA_COLORSPACE_SRGB,
        .v4l2 = V4L2_COLORSPACE_SRGB,
    },
    {
        .xen = XENCAMERA_COLORSPACE_OPRGB,
        .v4l2 = V4L2_COLORSPACE_ADOBERGB,
    },
    {
        .xen = XENCAMERA_COLORSPACE_BT2020,
        .v4l2 = V4L2_COLORSPACE_BT2020,
    },
    {
        .xen = XENCAMERA_COLORSPACE_DCI_P3,
        .v4l2 = V4L2_COLORSPACE_DCI_P3,
    },
    {
        -1
    }
};

const V4L2ToXen::xen_to_v4l2 V4L2ToXen::XEN_XFER_FUNC_TO_V4L2[] = {
    {
        .xen = XENCAMERA_XFER_FUNC_DEFAULT,
        .v4l2 = V4L2_XFER_FUNC_DEFAULT,
    },
    {
        .xen = XENCAMERA_XFER_FUNC_709,
        .v4l2 = V4L2_XFER_FUNC_709,
    },
    {
        .xen = XENCAMERA_XFER_FUNC_SRGB,
        .v4l2 = V4L2_XFER_FUNC_SRGB,
    },
    {
        .xen = XENCAMERA_XFER_FUNC_OPRGB,
        .v4l2 = V4L2_XFER_FUNC_ADOBERGB,
    },
    {
        .xen = XENCAMERA_XFER_FUNC_NONE,
        .v4l2 = V4L2_XFER_FUNC_NONE,
    },
    {
        .xen = XENCAMERA_XFER_FUNC_DCI_P3,
        .v4l2 = V4L2_XFER_FUNC_DCI_P3,
    },
    {
        .xen = XENCAMERA_XFER_FUNC_SMPTE2084,
        .v4l2 = V4L2_XFER_FUNC_SMPTE2084,
    },
    {
        -1
    }
};

const V4L2ToXen::xen_to_v4l2 V4L2ToXen::XEN_YCBCR_ENC_TO_V4L2[] = {
    {
        .xen = XENCAMERA_YCBCR_ENC_IGNORE,
        .v4l2 = V4L2_YCBCR_ENC_DEFAULT,
    },
    {
        .xen = XENCAMERA_YCBCR_ENC_601,
        .v4l2 = V4L2_YCBCR_ENC_601,
    },
    {
        .xen = XENCAMERA_YCBCR_ENC_709,
        .v4l2 = V4L2_YCBCR_ENC_709,
    },
    {
        .xen = XENCAMERA_YCBCR_ENC_XV601,
        .v4l2 = V4L2_YCBCR_ENC_XV601,
    },
    {
        .xen = XENCAMERA_YCBCR_ENC_XV709,
        .v4l2 = V4L2_YCBCR_ENC_XV709,
    },
    {
        .xen = XENCAMERA_YCBCR_ENC_BT2020,
        .v4l2 = V4L2_YCBCR_ENC_BT2020,
    },
    {
        .xen = XENCAMERA_YCBCR_ENC_BT2020_CONST_LUM,
        .v4l2 = V4L2_YCBCR_ENC_BT2020_CONST_LUM,
    },
    {
        -1
    }
};

const V4L2ToXen::xen_to_v4l2 V4L2ToXen::XEN_QUANTIZATION_TO_V4L2[] = {
    {
        .xen = XENCAMERA_QUANTIZATION_DEFAULT,
        .v4l2 = V4L2_QUANTIZATION_DEFAULT,
    },
    {
        .xen = XENCAMERA_QUANTIZATION_FULL_RANGE,
        .v4l2 = V4L2_QUANTIZATION_FULL_RANGE,
    },
    {
        .xen = XENCAMERA_QUANTIZATION_LIM_RANGE,
        .v4l2 = V4L2_QUANTIZATION_LIM_RANGE,
    },
    {
        -1
    }
};

int V4L2ToXen::toV4L2(int xen, const V4L2ToXen::xen_to_v4l2 *table)
{
    int i;

    for (i = 0; table[i].xen != -1; i++)
        if (table[i].xen == xen)
            return table[i].v4l2;
    return -EINVAL;
}

int V4L2ToXen::toXen(int v4l2, const V4L2ToXen::xen_to_v4l2 *table)
{
    int i;

    for (i = 0; table[i].v4l2 != -1; i++)
        if (table[i].v4l2 == v4l2)
            return table[i].xen;
    return -EINVAL;
}

int V4L2ToXen::ctrlToXen(int v4l2)
{
    int ret = toXen(v4l2, XEN_TYPE_TO_V4L2_CID);

    if (ret < 0)
        throw XenBackend::Exception("Unsupported V4L2 CID " +
                                    std::to_string(v4l2), EINVAL);
    return ret;
}

int V4L2ToXen::ctrlToV4L2(int xen)
{
    int ret = toV4L2(xen, XEN_TYPE_TO_V4L2_CID);

    if (ret < 0)
        throw XenBackend::Exception("Unsupported Xen CID " +
                                    std::to_string(xen), EINVAL);
    return ret;}

int V4L2ToXen::colorspaceToXen(int v4l2)
{
    int ret = toXen(v4l2, XEN_COLORSPACE_TO_V4L2);

    if (ret < 0)
        throw XenBackend::Exception("Unsupported V4L2 colorspace " +
                                    std::to_string(v4l2), EINVAL);
    return ret;}

int V4L2ToXen::colorspaceToV4L2(int xen)
{
    int ret = toV4L2(xen, XEN_COLORSPACE_TO_V4L2);

    if (ret < 0)
        throw XenBackend::Exception("Unsupported Xen xolorspace " +
                                    std::to_string(xen), EINVAL);
    return ret;}

int V4L2ToXen::xferToXen(int v4l2)
{
    int ret = toXen(v4l2, XEN_XFER_FUNC_TO_V4L2);

    if (ret < 0)
        throw XenBackend::Exception("Unsupported V4L2 xfer_func " +
                                    std::to_string(v4l2), EINVAL);
    return ret;
}

int V4L2ToXen::xferToV4L2(int xen)
{
    int ret = toV4L2(xen, XEN_XFER_FUNC_TO_V4L2);

    if (ret < 0)
        throw XenBackend::Exception("Unsupported Xen xfer_func " +
                                    std::to_string(xen), EINVAL);
    return ret;}

int V4L2ToXen::ycbcrToXen(int v4l2)
{
    int ret = toXen(v4l2, XEN_YCBCR_ENC_TO_V4L2);

    if (ret < 0)
        throw XenBackend::Exception("Unsupported V4L2 ycbcr_enc " +
                                    std::to_string(v4l2), EINVAL);
    return ret;}

int V4L2ToXen::ycbcrToV4L2(int xen)
{
    int ret = toV4L2(xen, XEN_YCBCR_ENC_TO_V4L2);

    if (ret < 0)
        throw XenBackend::Exception("Unsupported Xen ycbcr_enc " +
                                    std::to_string(xen), EINVAL);
    return ret;}

int V4L2ToXen::quantizationToXen(int v4l2)
{
    int ret = toXen(v4l2, XEN_QUANTIZATION_TO_V4L2);

    if (ret < 0)
        throw XenBackend::Exception("Unsupported V4L2 quantization " +
                                    std::to_string(v4l2), EINVAL);
    return ret;
}

int V4L2ToXen::quantizationToV4L2(int xen)
{
    int ret = toV4L2(xen, XEN_QUANTIZATION_TO_V4L2);

    if (ret < 0)
        throw XenBackend::Exception("Unsupported Xen quantization " +
                                    std::to_string(xen), EINVAL);
    return ret;
}

int V4L2ToXen::ctrlFlagsToXen(int v4l2)
{
    int flags = 0;

    if (v4l2 & V4L2_CTRL_FLAG_READ_ONLY)
        flags |= XENCAMERA_CTRL_FLG_RO;

    if (v4l2 & V4L2_CTRL_FLAG_WRITE_ONLY)
        flags |= XENCAMERA_CTRL_FLG_WO;

    if (v4l2 & V4L2_CTRL_FLAG_VOLATILE)
        flags |= XENCAMERA_CTRL_FLG_VOLATILE;

    return flags;
}

int V4L2ToXen::ctrlFlagsToV4L2(int xen)
{
    int flags = 0;

    if (xen & XENCAMERA_CTRL_FLG_RO)
        flags |= V4L2_CTRL_FLAG_READ_ONLY;

    if (xen & XENCAMERA_CTRL_FLG_WO)
        flags |= V4L2_CTRL_FLAG_WRITE_ONLY;

    if (xen & XENCAMERA_CTRL_FLG_VOLATILE)
        flags |= V4L2_CTRL_FLAG_VOLATILE;

    return flags;
}

