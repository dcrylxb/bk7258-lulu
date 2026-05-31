// Copyright 2023-2024 Beken
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <common/bk_include.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the KWS (Keyword Spotting) module
 *
 * This function initializes the KWS module, including memory allocation,
 * model loading, and necessary hardware configuration.
 *
 * @param base       Base address for memory allocation
 */
void bk_kws_init(uint32_t base);

/**
 * @brief Perform keyword recognition on audio data
 *
 * This function processes the input audio data to detect predefined keywords.
 * It extracts features, runs the neural network model, and applies post-processing
 * to determine if a keyword is detected.
 *
 * @param buf        Pointer to the audio data buffer (16-bit PCM format)
 * @param buf_len    Length of the audio data buffer in **bytes**, must be 1280 (640 samples)
 * @param text       Output parameter to store the recognized keyword text (pointer to string)
 * @param score      Output parameter to store the recognition confidence score
 * @param result     Output parameter to store the recognition result ID (category index)
 * 
 * @return           0 on success, -1 on failure
 */
int bk_tflite_ASR_Recog(short *buf, int buf_len, const char **text, float *score, int16_t *result);

#ifdef __cplusplus
}
#endif