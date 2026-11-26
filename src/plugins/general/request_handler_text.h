/*
 * SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef REQUEST_HANDLER_TEXT_H
#define REQUEST_HANDLER_TEXT_H

typedef struct request_text_s {
    char  *input;
    char  *output;
} metrics_request_text_t;

// Short-names
typedef metrics_request_text_t mets_req_text_t;

#endif // REQUEST_HANDLER_TEXT_H