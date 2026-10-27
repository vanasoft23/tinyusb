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

#include "unity.h"

#include "class/cdc/cdc_device_pvt.h"

static uint8_t buffer[16];

void setUp(void) {
}

void tearDown(void) {
}

void test_rx_xfer_info_selects_linear_tail_after_older_data(void) {
  const tu_fifo_buffer_info_t read_info = {
    .linear  = {.len = 8, .ptr = buffer + 2},
    .wrapped = {.len = 0, .ptr = NULL},
  };
  tu_fifo_buffer_info_t xfer_info;

  TEST_ASSERT_TRUE(cdcd_get_rx_xfer_info(&read_info, 3, &xfer_info));
  TEST_ASSERT_EQUAL_UINT16(3, xfer_info.linear.len);
  TEST_ASSERT_EQUAL_PTR(buffer + 7, xfer_info.linear.ptr);
  TEST_ASSERT_EQUAL_UINT16(0, xfer_info.wrapped.len);
  TEST_ASSERT_NULL(xfer_info.wrapped.ptr);
}

void test_rx_xfer_info_selects_both_spans_when_transfer_wraps(void) {
  const tu_fifo_buffer_info_t read_info = {
    .linear  = {.len = 4, .ptr = buffer + 12},
    .wrapped = {.len = 5, .ptr = buffer},
  };
  tu_fifo_buffer_info_t xfer_info;

  TEST_ASSERT_TRUE(cdcd_get_rx_xfer_info(&read_info, 7, &xfer_info));
  TEST_ASSERT_EQUAL_UINT16(2, xfer_info.linear.len);
  TEST_ASSERT_EQUAL_PTR(buffer + 14, xfer_info.linear.ptr);
  TEST_ASSERT_EQUAL_UINT16(5, xfer_info.wrapped.len);
  TEST_ASSERT_EQUAL_PTR(buffer, xfer_info.wrapped.ptr);
}

void test_rx_xfer_info_selects_wrapped_tail_after_older_data(void) {
  const tu_fifo_buffer_info_t read_info = {
    .linear  = {.len = 4, .ptr = buffer + 12},
    .wrapped = {.len = 6, .ptr = buffer},
  };
  tu_fifo_buffer_info_t xfer_info;

  TEST_ASSERT_TRUE(cdcd_get_rx_xfer_info(&read_info, 3, &xfer_info));
  TEST_ASSERT_EQUAL_UINT16(0, xfer_info.linear.len);
  TEST_ASSERT_NULL(xfer_info.linear.ptr);
  TEST_ASSERT_EQUAL_UINT16(3, xfer_info.wrapped.len);
  TEST_ASSERT_EQUAL_PTR(buffer + 3, xfer_info.wrapped.ptr);
}

void test_rx_xfer_info_rejects_more_than_readable_data(void) {
  const tu_fifo_buffer_info_t read_info = {
    .linear  = {.len = 4, .ptr = buffer},
    .wrapped = {.len = 0, .ptr = NULL},
  };
  tu_fifo_buffer_info_t xfer_info;

  TEST_ASSERT_FALSE(cdcd_get_rx_xfer_info(&read_info, 5, &xfer_info));
}
