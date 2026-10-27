/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 TinyUSB contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef _TUSB_CDC_DEVICE_PVT_H_
#define _TUSB_CDC_DEVICE_PVT_H_

#include "osal/osal.h"
#include "common/tusb_fifo.h"

// Select the newest xferred_bytes from the FIFO's readable spans.
TU_ATTR_ALWAYS_INLINE static inline bool cdcd_get_rx_xfer_info(const tu_fifo_buffer_info_t *read_info,
                                                               uint32_t                     xferred_bytes,
                                                               tu_fifo_buffer_info_t       *xfer_info) {
  const uint32_t readable_bytes = (uint32_t)read_info->linear.len + read_info->wrapped.len;
  if (xferred_bytes > readable_bytes) {
    return false;
  }

  tu_fifo_buffer_info_t result = {0};
  uint32_t              skip   = readable_bytes - xferred_bytes;

  if (skip < read_info->linear.len) {
    uint32_t linear_bytes = read_info->linear.len - skip;
    if (linear_bytes > xferred_bytes) {
      linear_bytes = xferred_bytes;
    }

    result.linear.ptr = read_info->linear.ptr + skip;
    result.linear.len = (uint16_t)linear_bytes;
    xferred_bytes -= linear_bytes;
    skip = 0;
  } else {
    skip -= read_info->linear.len;
  }

  if (xferred_bytes > 0) {
    if ((skip + xferred_bytes) > read_info->wrapped.len) {
      return false;
    }

    result.wrapped.ptr = read_info->wrapped.ptr + skip;
    result.wrapped.len = (uint16_t)xferred_bytes;
  }

  *xfer_info = result;
  return true;
}

#endif
