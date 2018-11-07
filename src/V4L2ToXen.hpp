/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Xen para-virtualized camera backend
 *
 * Copyright (C) 2018 EPAM Systems Inc.
 */
#ifndef SRC_V4L2TOXEN_HPP_
#define SRC_V4L2TOXEN_HPP_

class V4L2ToXen
{
public:

    static int ctrlToXen(int v4l2);
    static int ctrlToV4L2(int xen);

    static int ctrlFlagsToXen(int v4l2);
    static int ctrlFlagsToV4L2(int xen);

    static int colorspaceToXen(int v4l2);
    static int colorspaceToV4L2(int xen);

    static int xferToXen(int v4l2);
    static int xferToV4L2(int xen);

    static int ycbcrToXen(int v4l2);
    static int ycbcrToV4L2(int xen);

    static int quantizationToXen(int v4l2);
    static int quantizationToV4L2(int xen);

private:
    struct xen_to_v4l2 {
        int xen;
        int v4l2;
    };

    static const xen_to_v4l2 XEN_TYPE_TO_V4L2_CID[];
    static const xen_to_v4l2 XEN_COLORSPACE_TO_V4L2[];
    static const xen_to_v4l2 XEN_XFER_FUNC_TO_V4L2[];
    static const xen_to_v4l2 XEN_YCBCR_ENC_TO_V4L2[];
    static const xen_to_v4l2 XEN_QUANTIZATION_TO_V4L2[];

    static int toV4L2(int xen, const xen_to_v4l2 *table);
    static int toXen(int v4l2, const xen_to_v4l2 *table);
};

#endif /* SRC_V4L2TOXEN_HPP_ */

